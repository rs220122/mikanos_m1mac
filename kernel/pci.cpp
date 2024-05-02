#include "pci.hpp"

#include "asmfunc.h"
namespace {
    using namespace pci;

    uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t reg_addr) {
        // ラムダ式(無名関数)
        // 構文: [キャプチャ](引数リスト){ 処理本体 }
        // ラムダ式は、関数内部に定義できるため。
        auto shl = [](uint32_t x, unsigned int bits) { return x << bits; };

        // レジスタオフセットはずらす必要はないが、下位2ビットは0である必要がある。
        // 0xfc = 1111_1100
        // それぞれのCONFIG_ADDRESSの意味は、P143を参照すること
        return shl(1, 31) | shl(bus, 16) | shl(device, 11) | shl(function, 8) |
               (reg_addr & 0xfcu);
    }

    /** @brief devices[num_devices] に情報を書き込む num_device
     * をインクリメントする
     */
    Error AddDevice(const Device& device) {
        if (num_device == devices.size()) {
            return MAKE_ERROR(Error::kFull);
        }

        devices[num_device] = device;
        num_device++;
        return MAKE_ERROR(Error::kSuccess);
    }

    Error ScanBus(uint8_t bus);

    /** @brief 指定のファンクション番号のファンクションをスキャンする。
     * もし PCI-PCIブリッジなら、セカンダリバスに対し ScanBusを実行する
     */
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
        auto header_type = ReadHeaderType(bus, device, function);
        auto class_code = ReadClassCode(bus, device, function);
        Device dev{bus, device, function, header_type, class_code};

        // 実際にデバイスに追加する。
        if (auto err = AddDevice(dev)) {
            return err;
        }

        if (class_code.Match(0x06u, 0x04u)) {
            // standard PCI-PCI bridge
            auto bus_numbers = ReadBusNumbers(bus, device, function);
            uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
            return ScanBus(secondary_bus);
        }

        return MAKE_ERROR(Error::kSuccess);
    }

    /** @brief 指定のデバイス番号の各ファンクションをスキャンする。
     * 有効なファンクションを見つけたら ScanFunction を実行する
     */
    Error ScanDevice(uint8_t bus, uint8_t device) {
        if (auto err = ScanFunction(bus, device, 0)) {
            return err;
        }
        if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
            return MAKE_ERROR(Error::kSuccess);
        }

        for (uint8_t function = 1; function < 8; function++) {
            if (ReadVendorId(bus, device, function) == 0xffffu) {
                continue;
            }
            if (auto err = ScanFunction(bus, device, function)) {
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    /** @brief 指定のバス番号の角デバイスをスキャンする。
     * 有効なデバイスを見つけたら ScanDeviceを実行する
     */
    Error ScanBus(uint8_t bus) {
        for (uint8_t device = 0; device < 32; device++) {
            // ベンダIDが無効値(0xffff)以外なら実際のデバイスであることを表す。
            if (ReadVendorId(bus, device, 0) == 0xffffu) {
                continue;
            }
            if (auto err = ScanDevice(bus, device)) {
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }

}  // namespace

namespace pci {
    void WriteAddress(uint32_t address) {
        // バス番号、デバイス番号、ファンクション番号、レジスタオフセットをCONFIG_ADDRESSに設定
        // ここで設定した機器のPCIコンフィグレーション空間の値を書き込むことができる。
        // 書き込むときは、WriteDataを使う。
        IoOut32(kConfigAddress, address);
    }

    void WriteData(uint32_t value) {
        // CONFIG_ADDRESSに設定した値のところに、データを書き込む
        // アドレスにデータを書き込む。
        IoOut32(kConfigData, value);
    }

    uint32_t ReadData() {
        // CONFIG_DATAを読み込む
        return IoIn32(kConfigData);
    }

    // 以下のそれぞれのPCIコンフィグレーション空間内のデータは、本書P142を参考にすること
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function) {
        // ベンダーIDはコンフィギュレーション空間の先頭にある。
        // 最初の16ビットが、ベンダーID
        WriteAddress(MakeAddress(bus, device, function, 0x00));
        return ReadData() & 0xffffu;
    }

    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function) {
        // 16~32ビットがデバイスID
        WriteAddress(MakeAddress(bus, device, function, 0x00));
        return ReadData() >> 16;
    }

    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x0c));
        return (ReadData() >> 16) & 0xffu;
    }

    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x08));
        auto reg = ReadData();

        // BaseClass は、0x08から0x0Bの間の上位24から32ビット(8bit)
        // SubClassは、0x08から0x0Bの間の上位16から24ビット(8bit)
        // interfaceは、0x08から0x0Bの間の上記8から16ビット(8bit)
        ClassCode cc;
        cc.base = (reg >> 24) & 0xffu;
        cc.sub = (reg >> 16) & 0xffu;
        cc.interface = (reg >> 8) & 0xffu;
        return cc;
    }

    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x18));
        return ReadData();
    }

    bool IsSingleFunctionDevice(uint8_t header_type) {
        return (header_type & 0x80u) == 0;
    }

    Error ScanAllBus() {
        // 全てのPCI機器を探索する。
        num_device = 0;

        auto header_type = ReadHeaderType(0, 0, 0);
        if (IsSingleFunctionDevice(header_type)) {
            return ScanBus(0);
        }

        for (uint8_t function = 1; function < 8; function++) {
            if (ReadVendorId(0, 0, function) == 0xffffu) {
                continue;
            }
            if (auto err = ScanBus(function)) {
                return err;
            }
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr) {
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        return ReadData();
    }

    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value) {
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        WriteData(value);
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index) {
        if (bar_index >= 6) {
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        // PCIコンフィグレーション空間上のBARの位置を特定して、それを読む。
        const auto addr = CalcBarAddress(bar_index);
        const auto bar = ReadConfReg(device, addr);

        // 32 bit address
        if ((bar & 4u) == 0) {
            return {bar, MAKE_ERROR(Error::kSuccess)};
        }

        // 64 bit address
        if (bar_index >= 5) {
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        // 64ビットの場合は、次のBARの値も読む。
        // bar_upperは、上位32ビットを読み込み。それを64ビットにキャストした後に、シフトしている。
        const auto bar_upper = ReadConfReg(device, addr + 4);
        return {bar | (static_cast<uint64_t>(bar_upper) << 32),
                MAKE_ERROR(Error::kSuccess)};
    }

}  // namespace pci
