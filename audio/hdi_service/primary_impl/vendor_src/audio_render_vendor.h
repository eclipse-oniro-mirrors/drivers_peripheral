/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef AUDIO_RENDER_VENDOR_H
#define AUDIO_RENDER_VENDOR_H

#include "i_audio_render.h"
#include "v1_0/iaudio_render.h"

struct IAudioRender *AudioHwiCreateRenderById(const struct AudioSampleAttributes *attrs, uint32_t *renderId,
    struct AudioHwiRender *hwiRender, const struct AudioDeviceDescriptor *desc);
void AudioHwiDestroyRenderById(uint32_t renderId);
struct AudioHwiRender *AudioHwiGetHwiRenderById(uint32_t renderId);
struct IAudioRender *FindRenderCreated(enum AudioPortPin pin, const struct AudioSampleAttributes *attrs,
    uint32_t *rendrId);
uint32_t DecreaseRenderUsrCount(uint32_t renderId);

#endif // AUDIO_RENDER_VENDOR_H
