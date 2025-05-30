/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef CAMERA_DEVICE_FUZZER_H
#define CAMERA_DEVICE_FUZZER_H
#define FUZZ_PROJECT_NAME "cameradevice_fuzzer"
#include "hdi_common_v1_3.h"
namespace OHOS::Camera {
    std::shared_ptr<OHOS::Camera::HdiCommonV1_3> cameraTest = nullptr;
}
#endif
