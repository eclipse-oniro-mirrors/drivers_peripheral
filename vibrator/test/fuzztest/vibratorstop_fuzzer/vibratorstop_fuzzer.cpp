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

#include "vibratorstop_fuzzer.h"
#include "hdf_base.h"
#include "v2_0/vibrator_interface_proxy.h"

using namespace OHOS::HDI::Vibrator::V2_0;

namespace OHOS {
    bool VibratorStopFuzzTest(const uint8_t* data, size_t size)
    {
        bool result = false;
        sptr<IVibratorInterface> g_vibratorInterface = IVibratorInterface::Get();
        if (!g_vibratorInterface->Start({-1, 1}, reinterpret_cast<const std::string &>(data))) {
            result = true;
        }
        HdfVibratorMode mode = static_cast<HdfVibratorMode>(*data);
        if (!g_vibratorInterface->Stop({-1, 1}, mode)) {
            result = true;
        }
        return result;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr) {
        return 0;
    }

    if (size < sizeof(int32_t)) {
        return 0;
    }
    OHOS::VibratorStopFuzzTest(data, size);
    return 0;
}

