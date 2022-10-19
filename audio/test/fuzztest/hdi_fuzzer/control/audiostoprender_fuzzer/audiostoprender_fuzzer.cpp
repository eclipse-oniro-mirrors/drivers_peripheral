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

#include "audiostoprender_fuzzer.h"
#include "audio_hdi_fuzzer_common.h"

using namespace OHOS::Audio;
namespace OHOS {
namespace Audio {
bool AudioStopRenderFuzzTest(const uint8_t *data, size_t size)
{
    bool result = false;
    TestAudioManager *stopFuzzManager = nullptr;
    struct AudioAdapter *stopFuzzAdapter = nullptr;
    struct AudioRender *stopFuzzRender = nullptr;
    int32_t ret = AudioGetManagerCreateStartRender(stopFuzzManager, &stopFuzzAdapter, &stopFuzzRender);
    if (ret < 0 || stopFuzzAdapter == nullptr || stopFuzzRender == nullptr || stopFuzzManager == nullptr) {
        HDF_LOGE("%{public}s: AudioGetManagerCreateStartRender failed \n", __func__);
        return false;
    }

    struct AudioRender *renderFuzz = reinterpret_cast<struct AudioRender *>(const_cast<uint8_t *>(data));
    ret = stopFuzzRender->control.Stop((AudioHandle)renderFuzz);
    if (ret == HDF_SUCCESS) {
        result = true;
    }
    stopFuzzAdapter->DestroyRender(stopFuzzAdapter, stopFuzzRender);
    stopFuzzManager->UnloadAdapter(stopFuzzManager, stopFuzzAdapter);
    stopFuzzRender = nullptr;
    return result;
}
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::Audio::AudioStopRenderFuzzTest(data, size);
    return 0;
}