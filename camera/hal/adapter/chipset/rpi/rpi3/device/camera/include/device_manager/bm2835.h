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

#ifndef HOS_CAMERA_BM2835_H
#define HOS_CAMERA_BM2835_H

#include "isensor.h"
#include "create_sensor_factory.h"
#include "device_manager_adapter.h"

namespace OHOS::Camera {
class Bm2835 : public ISensor {
    DECLARE_SENSOR(Bm2835)
public:
    Bm2835();
    virtual ~Bm2835();
    void InitSensitivityRange(CameraMetadata& camera_meta_data);
    void InitAwbModes(CameraMetadata& camera_meta_data);
    void InitCompensationRange(CameraMetadata& camera_meta_data);
    void InitFpsTarget(CameraMetadata& camera_meta_data);
    void InitAvailableModes(CameraMetadata& camera_meta_data);
    void InitAntiBandingModes(CameraMetadata& camera_meta_data);
    void InitPhysicalSize(CameraMetadata& camera_meta_data);
    void Init(CameraMetadata& camera_meta_data);
};
} // namespace OHOS::Camera
#endif