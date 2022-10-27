/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "dcameragetcameraIds_fuzzer.h"

#include <cstddef>
#include <cstdint>

#include "dcamera_host.h"

namespace OHOS {
namespace DistributedHardware {
void DcameraGetCameraIdsFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return;
    }
    std::string cameraId(reinterpret_cast<const char*>(data), size);
    std::vector<std::string> cameraIds;
    cameraIds.push_back(cameraId);
    DCameraHost::GetInstance()->GetCameraIds(cameraIds);
}
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DistributedHardware::DcameraGetCameraIdsFuzzTest(data, size);
    return 0;
}

