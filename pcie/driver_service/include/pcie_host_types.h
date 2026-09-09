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

#ifndef OHOS_HDI_PCIE_HOST_TYPES_H
#define OHOS_HDI_PCIE_HOST_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

#include "v1_0/pcie_types.h"

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace Driver {

/* Keep these values aligned with the SA-side PcieDriverLoadStatus definition. */
enum class PcieDriverLoadStatus : int32_t {
    LOADED = 0,
    PATH_REJECTED = 10,
    OPEN_FAILED = 11,
    PATH_TAMPERED = 12,
    LOAD_RESOURCE_BUSY = 13,
    LOAD_UNKNOWN = 14,
    LOAD_OPERATION_NOT_PERMITTED = 15,
    LOAD_OUT_OF_MEMORY = 16,
    LOAD_IO_ERROR = 17,
    LOAD_INVALID_FILE_DESCRIPTOR = 18,
    LOAD_NOT_SUPPORTED = 19,
    LOAD_ACCESS_DENIED = 20,
    LOAD_INVALID_MEMORY_ADDRESS = 21,
    LOAD_INVALID_ARGUMENT = 22,
    LOAD_INVALID_MODULE_FORMAT = 23,
    LOAD_MODULE_ALREADY_EXISTS = 24,
    LOAD_SIGNING_KEY_MISSING = 25,
    LOAD_SIGNATURE_REJECTED = 26,
    LOAD_INVALID_DATA = 27,
    LOAD_FILE_TOO_LARGE = 28,
    LOAD_FILE_BUSY = 29,
    LOAD_REQUIRED_OBJECT_MISSING = 30,
    LOAD_INSUFFICIENT_SPACE = 31,
    LOAD_DEVICE_UNAVAILABLE = 32,
    LOAD_SIGNING_KEY_EXPIRED = 33,
    LOAD_SIGNING_KEY_REVOKED = 34,
};

/* Host-side DTOs — separate from SA types, converted at HDI boundary */

struct PcieDeviceInfo {
    std::string boardId;       /* BDF slot */
    std::string sysfsPath;
    std::string vendorId;      /* "0x10EC" */
    std::string deviceId;      /* "0x8168" */
    std::string classCode;     /* "0x020000" */
    std::string modalias;      /* "pci:v000010ECd..." */
    bool        isModaliasValid = false;
    std::string boundDriver;   /* kernel driver name bound to this board, "" if none */
};

using OHOS::HDI::Pcie::V1_0::PcieDriverLoadItem;
using OHOS::HDI::Pcie::V1_0::PcieDriverLoadResult;

struct PcieDriverHostExecutionResult {
    std::vector<PcieDriverLoadResult> loadResults;
};

} // namespace Driver
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
#endif
