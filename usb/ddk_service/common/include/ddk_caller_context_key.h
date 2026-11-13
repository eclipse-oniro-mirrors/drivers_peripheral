/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef DDK_CALLER_CONTEXT_KEY_H
#define DDK_CALLER_CONTEXT_KEY_H

#include <cstdint>
#include "ipc_skeleton.h"

namespace OHOS {
namespace HDI {
namespace Usb {
namespace Ddk {

// Caller context key bit layout (u64):
// [55:48] busNum | [47:40] devAddr | [39:32] interfaceIndex | [31:0] callerTokenId
constexpr int KEY_OFFSET_BUS_NUM = 48;
constexpr int KEY_OFFSET_DEV_ADDR = 40;
constexpr int KEY_OFFSET_INTERFACE_INDEX = 32;
constexpr uint64_t KEY_MASK_BYTE = 0xFF;
constexpr uint64_t KEY_MASK_CALLER_TOKEN = 0xFFFFFFFF;

// Packs the fields into a single u64 caller context key.
inline uint64_t BuildCallerContextKey(uint64_t busNum, uint64_t devAddr, uint64_t interfaceIndex,
    uint64_t callerTokenId)
{
    return ((busNum & KEY_MASK_BYTE) << KEY_OFFSET_BUS_NUM)
        | ((devAddr & KEY_MASK_BYTE) << KEY_OFFSET_DEV_ADDR)
        | ((interfaceIndex & KEY_MASK_BYTE) << KEY_OFFSET_INTERFACE_INDEX)
        | (callerTokenId & KEY_MASK_CALLER_TOKEN);
}

// Extracts busNum (high 32 bits) / devAddr (low 32 bits) from deviceId and takes the caller
// token from the IPC framework, so the key identifies "which caller opened which interface".
inline uint64_t BuildCallerContextKeyFromDeviceId(uint64_t deviceId, uint64_t interfaceIndex)
{
    constexpr uint8_t THIRTY_TWO_BIT = 32;
    uint64_t busNum = deviceId >> THIRTY_TWO_BIT;
    uint64_t devAddr = deviceId & 0xFFFFFFFF;
    return BuildCallerContextKey(busNum, devAddr, interfaceIndex, IPCSkeleton::GetCallingTokenID());
}

// Returns the caller token id stored in the low 32 bits of the key.
inline uint32_t GetCallerTokenFromKey(uint64_t key)
{
    return static_cast<uint32_t>(key & KEY_MASK_CALLER_TOKEN);
}

} // namespace Ddk
} // namespace Usb
} // namespace HDI
} // namespace OHOS

#endif // DDK_CALLER_CONTEXT_KEY_H
