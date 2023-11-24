/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "dcameraopencamera_fuzzer.h"

#include <cstddef>
#include <cstdint>

#include "dcamera_host.h"
#include "v1_0/icamera_device_callback.h"

namespace OHOS {
namespace DistributedHardware {

class DemoCameraDeviceCallback : public ICameraDeviceCallback {
public:
    DemoCameraDeviceCallback() = default;
    virtual ~DemoCameraDeviceCallback() = default;
    int32_t OnError(ErrorType type, int32_t errorCode)
    {
        return 0;
    }
    int32_t OnResult(uint64_t timestamp, const std::vector<uint8_t>& result)
    {
        return 0;
    }
};

void DcameraOpenCameraFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size < sizeof(uint8_t))) {
        return;
    }
    std::string cameraId(reinterpret_cast<const char*>(data), size);
    sptr<ICameraDeviceCallback> callbackObj(new DemoCameraDeviceCallback());
    sptr<ICameraDevice> demoCameraDevice = nullptr;
    DCameraHost::GetInstance()->OpenCamera(cameraId, callbackObj, demoCameraDevice);
}
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DistributedHardware::DcameraOpenCameraFuzzTest(data, size);
    return 0;
}
