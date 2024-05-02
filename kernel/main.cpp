#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "mouse.hpp"
#include "pci.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/device.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/xhci/xhci.hpp"
// cstdintは、uintX_tという整数型(Xはビット数)
// short,
// intは、ビット数が決まっていないため、ビット数を固定したいときに用いる。

// デスクトップカラーの設定
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

char console_buf[sizeof(Console)];
Console* console;
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

int printk(const char* format, ...) {
    // 可変超引数を取る
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    // sprintfの親戚で、可変超引数の代わりにva_list型の変数を受け取ることができる。
    // printkの内部で1024バイトの固定配列をとっているため、最大文字数に制限がありますが、それ以外はprintfと同様
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

// #@@range_begin(switch_echi2xhci)
void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::num_device; ++i) {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
            0x8086 == pci::ReadVendorId(pci::devices[i])) {
            intel_ehc_exist = true;
            break;
        }
    }
    if (!intel_ehc_exist) {
        return;
    }

    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc);  // USB3PRM
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports);           // USB3_PSSEN
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4);   // XUSB2PRM
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports);            // XUSB2PR
    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n", superspeed_ports,
        ehci2xhci_ports);
}
// #@@range_end(switch_echi2xhci)

char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    // 一般的なnew演算子は、new <クラス名> なので、引数を取らない
    // 一般のnewは指定したクラスのインスタンスをヒープ領域(関数の実行が終了しても破棄されない。)に生成する。
    // mallocとnewの違いは、クラスのコンストラクタが呼び出されるかどうか。
    // ヒープ領域に確保するのは、OSがメモリ管理できる様になって初めて可能
    // メモリ管理ができない時でもクラスのインスタンスを作りたいときに登場するのが、"配置new"
    // 配置newでは、メモリ領域の確保を行わない。
    // 配置newを使うには、<new>を淫クルーづするか自分で定義する必要がある。
    // C++では、operatorキーワードを使うことで演算子を定義できる。
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new (pixel_writer_buf)
                RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new (pixel_writer_buf)
                BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    // デスクトップ本体内部を塗りつぶす
    FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50},
                  kDesktopBGColor);
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50},
                  {1, 8, 17});
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50},
                  {80, 80, 80});
    DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30},
                  {160, 160, 160});

    console = new (console_buf)
        Console{*pixel_writer, kDesktopFGColor, kDesktopBGColor};
    printk("Welcome to MikanOS!\n");
    SetLogLevel(kInfo);

    mouse_cursor = new (mouse_cursor_buf)
        MouseCursor{pixel_writer, kDesktopBGColor, {300, 200}};

    // PCIデバイスを操作する。
    auto err = pci::ScanAllBus();
    Log(kDebug, "ScanAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; i++) {
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n", dev.bus,
            dev.device, dev.function, vendor_id, class_code, dev.header_type);
    }

    // Intel 製を優先してxHCを探す。
    pci::Device* xhc_dev = nullptr;
    for (int i = 0; i < pci::num_device; i++) {
        Log(kInfo, "%d\n", i);
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
            xhc_dev = &pci::devices[i];
            Log(kInfo, "xHC Found: %d\n", i);
            if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
                break;
            }
        }
    }

    if (xhc_dev) {
        Log(kInfo, "xHC has been found: %d.%d.%d\n", xhc_dev->bus,
            xhc_dev->device, xhc_dev->function);
    } else {
        // デバイスが見つからなかったとき
        Log(kError, "Error xHC not found: %08lx\n", xhc_dev);
        while (1) __asm__("hlt");
    }

    // BARを読む。(MMIO上でのアドレス位置を特定する。)
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kInfo, "ReadBar: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kInfo, "xHC mmio_base = %08lx\n", xhc_mmio_base);

    // xhcを初期化する。
    usb::xhci::Controller xhc{xhc_mmio_base};
    if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        SwitchEhci2Xhci(*xhc_dev);
    }
    Log(kInfo, "xHX initialize start.");

    {
        auto err = xhc.Initialize();
        Log(kInfo, "xhc.Initialize: %s\n", err.Name());
    }
    Log(kInfo, "xHC starting\n");
    xhc.Run();

    // ポートコンフィグ
    usb::HIDMouseDriver::default_observer = MouseObserver;

    // すべてのUSBポートを探索して、何かが接続されているポートの設定を行う
    for (int i = 1; i <= xhc.MaxPorts(); i++) {
        auto port = xhc.PortAt(i);
        Log(kInfo, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if (port.IsConnected()) {
            // ConfigurePortは、ポートのリセットやxHC内部設定、クラスドライバの生成などを行う
            // あるポートにUSBマウスが接続されていた場合、USB::HIDMouseDrive::default_observerに設定した関数が
            // そのUSBマウスからのデータを受信する関数として、USBマウス用のクラスドライバに登録される。
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    while (1) {
        if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n", err.Name(),
                err.File(), err.Line());
        }
    }

    while (1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}