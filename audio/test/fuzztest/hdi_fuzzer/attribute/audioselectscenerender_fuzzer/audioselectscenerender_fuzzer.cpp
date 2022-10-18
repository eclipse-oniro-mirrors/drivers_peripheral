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

#include "audioselectscenerender_fuzzer.h"
#include "audio_hdi_fuzzer_common.h"

using namespace OHOS::Audio;
namespace OHOS {
namespace Audio {
    bool AudioSelectSceneRenderFuzzTest(const uint8_t *data, size_t size)
    {
        bool result = false;
        struct AudioSceneDescriptor scenes = {};
        scenes.scene.id = 0;
        TestAudioManager *selectFuzzManager = nullptr;
        struct AudioAdapter *selectFuzzAdapter = nullptr;
        struct AudioRender *selectFuzzRender = nullptr;
        int32_t ret = AudioGetManagerCreateRender(selectFuzzManager, &selectFuzzAdapter, &selectFuzzRender);
        if (ret < 0 || selectFuzzAdapter == nullptr || selectFuzzRender == nullptr || selectFuzzManager == nullptr) {
            HDF_LOGE("%{public}s: AudioGetManagerCreateRender failed \n", __func__);
            return false;
        }

        struct AudioRender *handle = reinterpret_cast<struct AudioRender *>(const_cast<uint8_t *>(data));
        ret = selectFuzzRender->scene.SelectScene(handle, &scenes);
        if (ret == HDF_SUCCESS) {
            result = true;
        }
        selectFuzzAdapter->DestroyRender(selectFuzzAdapter, selectFuzzRender);
        selectFuzzManager->UnloadAdapter(selectFuzzManager, selectFuzzAdapter);
        return result;
    }
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::Audio::AudioSelectSceneRenderFuzzTest(data, size);
    return 0;
}