/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_HDI_BLUETOOTH_HCI_HCI_INTERNAL_H
#define OHOS_HDI_BLUETOOTH_HCI_HCI_INTERNAL_H
#include <cstdint>

namespace OHOS {
namespace HDI {
namespace Bluetooth {
namespace Hci {
enum HciPacketType : uint8_t {
    HCI_PACKET_TYPE_UNKNOWN = 0,
    HCI_PACKET_TYPE_COMMAND = 1,
    HCI_PACKET_TYPE_ACL_DATA = 2,
    HCI_PACKET_TYPE_SCO_DATA = 3,
    HCI_PACKET_TYPE_EVENT = 4,
    HCI_PACKET_TYPE_ISO_DATA = 5,
    HCI_PACKET_TYPE_MAX,
};

struct PacketHeader {
    uint8_t headerSize;
    uint8_t dataLengthOffset;
    uint8_t dataLengthSize;
};

constexpr uint8_t HCI_EVENT_CODE_COMMAND_COMPLETE = 0x0E;
constexpr uint8_t HCI_EVENT_CODE_VENDOR_SPECIFIC = 0xFF;

constexpr uint8_t HCI_DATA_TYPE_HCI_OFFSET = 0x11; // 扩展HCI命令数据包类型
constexpr uint8_t HCI_DATA_TYPE_SECOND_MASK = 0x0F; // 扩展数据包类型低4位掩码
constexpr uint16_t HCI_VSC_OPCODE_SET_BD_ADDR = 0xFC32;
}  // namespace Hci
}  // namespace Bluetooth
}  // namespace HDI
}  // namespace OHOS
#endif /* OHOS_HDI_BLUETOOTH_HCI_HCI_INTERNAL_H */