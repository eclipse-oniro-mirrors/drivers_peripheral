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

#include "vibrator_fuzzer.h"
#include <cstddef>
#include <cstdint>
#include "hdf_log.h"
#include "v2_0/vibrator_interface_proxy.h"
#include "v2_0/vibrator_interface_stub.h"

using namespace OHOS::HDI::Vibrator::V2_0;

namespace OHOS {
constexpr size_t THRESHOLD = 10;
constexpr int32_t OFFSET = 4;
constexpr int32_t ARRAY0 = 0;
constexpr int32_t ARRAY1 = 1;
constexpr int32_t ARRAY2 = 2;
constexpr int32_t ARRAY3 = 3;
constexpr int32_t DIGIT8 = 8;
constexpr int32_t DIGIT16 = 16;
constexpr int32_t DIGIT24 = 24;
const std::u16string VIBRATOR_INTERFACE_TOKEN = u"ohos.hdi.vibrator.v1_1.IVibratorInterface";

uint32_t Convert2Uint32(const uint8_t* ptr)
{
    if (ptr == nullptr) {
        return 0;
    }
    /*
     * Move the 0th digit 24 to the left, the first digit 16 to the left, the second digit 8 to the left,
     * and the third digit no left
     */
    return (ptr[ARRAY0] << DIGIT24) | (ptr[ARRAY1] << DIGIT16) | (ptr[ARRAY2] << DIGIT8) | (ptr[ARRAY3]);
}

bool DoSomethingInterestingWithMyAPI(const uint8_t* rawData, size_t size)
{
    if (rawData == nullptr) {
        return false;
    }
    uint32_t code = Convert2Uint32(rawData);
    rawData = rawData + OFFSET;
    size = size - OFFSET;

    MessageParcel data;
    data.WriteInterfaceToken(VIBRATOR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);
    MessageParcel reply;
    MessageOption option;
    sptr<IVibratorInterface> g_vibratorInterface = IVibratorInterface::Get(false);
    if (g_vibratorInterface == nullptr) {
        HDF_LOGE("%{public}s:IVibratorInterface::Get() failed.", __func__);
        return false;
    }
    sptr<VibratorInterfaceStub> vibratorInterface = new VibratorInterfaceStub(g_vibratorInterface);
    if (vibratorInterface == nullptr) {
        HDF_LOGE("%{public}s:new VibratorInterfaceStub failed.", __func__);
        return false;
    }
    vibratorInterface->OnRemoteRequest(code, data, reply, option);

    return true;
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < OHOS::THRESHOLD) {
        return 0;
    }

    if (data == nullptr) {
        return 0;
    }

    if (size < sizeof(int32_t)) {
        return 0;
    }
    /* Run your code on data */
    OHOS::DoSomethingInterestingWithMyAPI(data, size);
    return 0;
}

