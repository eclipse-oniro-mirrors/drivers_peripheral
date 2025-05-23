/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef AUDIO_DFX_H
#define AUDIO_DFX_H

#include <stdint.h>
#include "v5_0/audio_types.h"
#include "v5_0/iaudio_manager.h"
#include "v5_0/iaudio_adapter.h"
#include "v5_0/iaudio_render.h"
#include "v5_0/iaudio_capture.h"
#include "audio_uhdf_log.h"
#include <sys/time.h>

#define TIME_1000 1000
#define TIME_THRESHOLD 30

#ifdef __cplusplus
extern "C" {
#endif

void HdfAudioStartTrace(const char* value, int valueLen);
void HdfAudioFinishTrace(void);
int32_t SetTimer(const char* name);
void CancelTimer(int32_t id);
struct timeval AudioDfxSysEventGetTimeStamp(void);
int32_t AudioDfxSysEventError(const char* log, struct timeval startTime, int timeThreshold, int err);
#ifdef __cplusplus
}
#endif
#endif /* AUDIO_DFX_H */