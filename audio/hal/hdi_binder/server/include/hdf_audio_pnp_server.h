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

#ifndef HDF_AUDIO_PNP_SERVER_H
#define HDF_AUDIO_PNP_SERVER_H

#include "v1_0/audio_types.h"
#include "hdf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* statusInfo is update new info */
int32_t AudioPnpUpdateInfo(const char *statusInfo);
int32_t AudioPnpUpdateInfoOnly(struct AudioEvent audioEvent);
int32_t AudioUhdfUnloadDriver(const char *driverName);
int32_t AudioUhdfLoadDriver(const char *driverName);

#ifdef __cplusplus
}
#endif
#endif
