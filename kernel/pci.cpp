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
    return shl(1, 31) | shl(bus, 16) | shl(device, 11) | shl(function, 8) |
           (reg_addr & 0xfcu);
}

/** @brief devices[num_devices] に情報を書き込む num_device をインクリメントする
 */
Error AddDevice(uint8_t bus, uint8_t device, uint8_t function,
                uint8_t header_type) {
    if (num_device == devices.size()) {
        return Error::kFull;
    }

    devices[num_device] = Device{bus, device, function, header_type};
    num_device++;
    return Error::kSuccess;
}

Error ScanBus(uint8_t bus);

/** @brief 指定のファンクション番号のファンクションをスキャンする。
 * もし PCI-PCIブリッジなら、セカンダリバスに対し ScanBusを実行する
 */
Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
    auto header_type = ReadHeaderType(bus, device, function);

    // 実際にデバイスに追加する。
    if (auto err = AddDevice(bus, device, function, header_type)) {
        return err;
    }

    auto class_code = ReadClassCode(bus, device, function);
    uint8_t base = (class_code >> 24) & 0xffu;
    uint8_t sub = (class_code >> 16) & 0xffu;

    if (base == 0x06u && sub == 0x04u) {
        // standard PCI-PCI bridge
        auto bus_numbers = ReadBusNumbers(bus, device, function);
        uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
        return ScanBus(secondary_bus);
    }

    return Error::kSuccess;
}

/** @brief 指定のデバイス番号の各ファンクションをスキャンする。
 * 有効なファンクションを見つけたら ScanFunction を実行する
 */
Error ScanDevice(uint8_t bus, uint8_t device) {
    if (auto err = ScanFunction(bus, device, 0)) {
        return err;
    }
    if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
        return Error::kSuccess;
    }

    for (uint8_t function = 1; function < 8; function++) {
        if (ReadVendorId(bus, device, function) == 0xffffu) {
            continue;
        }
        if (auto err = ScanFunction(bus, device, function)) {
            return err;
        }
    }
    return Error::kSuccess;
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
    return Error::kSuccess;
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

uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x08));
    return ReadData();
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
    return Error::kSuccess;
}

}  // namespace pci
