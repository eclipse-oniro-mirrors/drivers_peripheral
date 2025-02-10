/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef AUDIO_DFX_VDI_H
#define AUDIO_DFX_VDI_H
#include "audio_types_vdi.h"

#ifdef __cplusplus
extern "C" {
#endif

void HdfAudioStartTrace(const char* value, int valueLen);
void HdfAudioFinishTrace(void);
int32_t SetTimer(const char* name);
void CancelTimer(int32_t id);
void SetMaxWorkThreadNum(int32_t count);
#ifdef __cplusplus
}
#endif
#endif /* AUDIO_DFX_VDI_H */