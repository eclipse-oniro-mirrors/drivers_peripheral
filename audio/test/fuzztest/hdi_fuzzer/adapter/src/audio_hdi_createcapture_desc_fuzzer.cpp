/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "audio_hdi_fuzzer_common.h"

using namespace HMOS::Audio;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    TestAudioManager *manager = nullptr;
    int32_t ret = GetManager(manager);
    if (ret < 0 || manager == nullptr) {
        return 0;
    }
    struct AudioAdapter *adapter = nullptr;
    struct AudioPort *capturePort = nullptr;
    ret = GetLoadAdapter(manager, &adapter, capturePort);
    if (ret < 0 || adapter == nullptr) {
        return 0;
    }
    struct AudioSampleAttributes attrs = {};
    InitAttrs(attrs);
    struct AudioCapture *capture = nullptr;
    uint8_t *dataFuzz = const_cast<uint8_t *>(data);
    struct AudioDeviceDescriptor *devDescFuzz = reinterpret_cast<struct AudioDeviceDescriptor *>(dataFuzz);
    adapter->CreateCapture(adapter, devDescFuzz, &attrs, &capture);

    adapter->DestroyCapture(adapter, capture);
    manager->UnloadAdapter(manager, adapter);
    return 0;
}