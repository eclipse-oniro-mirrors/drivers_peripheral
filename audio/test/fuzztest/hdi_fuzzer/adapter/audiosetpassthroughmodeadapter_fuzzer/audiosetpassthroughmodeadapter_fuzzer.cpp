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

#include "audiosetpassthroughmodeadapter_fuzzer.h"
#include "audio_hdi_fuzzer_common.h"

using namespace OHOS::Audio;
namespace OHOS {
namespace Audio {
bool AudioSetpassthroughmodeAdapterFuzzTest(const uint8_t *data, size_t size)
{
    bool result = false;
    TestAudioManager *setPassthroughFuzzManager = nullptr;
    int32_t ret = GetManager(setPassthroughFuzzManager);
    if (ret < 0 || setPassthroughFuzzManager == nullptr) {
        HDF_LOGE("%{public}s: GetManager failed \n", __func__);
        return false;
    }
    struct AudioAdapter *setPassthroughFuzzAdapter = nullptr;
    struct AudioPort *audioPort = nullptr;
    ret = GetLoadAdapter(setPassthroughFuzzManager, &setPassthroughFuzzAdapter, audioPort);
    if (ret < 0 || setPassthroughFuzzAdapter == nullptr) {
        HDF_LOGE("%{public}s: GetLoadAdapter failed \n", __func__);
        return false;
    }
    AudioPortPassthroughMode mode = PORT_PASSTHROUGH_LPCM;
    struct AudioAdapter *adapterFuzz = reinterpret_cast<struct AudioAdapter *>(const_cast<uint8_t *>(data));
    ret = setPassthroughFuzzAdapter->SetPassthroughMode(adapterFuzz, audioPort, mode);
    if (ret == HDF_SUCCESS) {
        result = true;
    }
    setPassthroughFuzzManager->UnloadAdapter(setPassthroughFuzzManager, setPassthroughFuzzAdapter);
    return result;
}
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::Audio::AudioSetpassthroughmodeAdapterFuzzTest(data, size);
    return 0;
}