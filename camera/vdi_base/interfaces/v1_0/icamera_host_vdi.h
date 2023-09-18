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

#ifndef OHOS_VDI_CAMERA_V1_0_ICAMERAHOSTVDI_H
#define OHOS_VDI_CAMERA_V1_0_ICAMERAHOSTVDI_H

#include <stdint.h>
#include <string>
#include <vector>
#include <hdf_base.h>
#include <hdi_base.h>
#include "v1_0/icamera_device_vdi.h"
#include "v1_0/icamera_device_vdi_callback.h"
#include "v1_0/icamera_host_vdi_callback.h"
#include "hdf_load_vdi.h"


namespace OHOS {
namespace VDI {
namespace Camera {
namespace V1_0 {
using namespace OHOS;
using namespace OHOS::HDI;

class ICameraHostVdi : public HdiBase {
public:
    virtual ~ICameraHostVdi() = default;

    static sptr<ICameraHostVdi> Get(bool isStub = false);
    static sptr<ICameraHostVdi> Get(const std::string &serviceName, bool isStub = false);

    virtual int32_t SetCallback(const sptr<ICameraHostVdiCallback> &callbackObj) = 0;

    virtual int32_t GetCameraIds(std::vector<std::string> &cameraIds) = 0;

    virtual int32_t GetCameraAbility(const std::string &cameraId, std::vector<uint8_t> &cameraAbility) = 0;

    virtual int32_t OpenCamera(const std::string &cameraId, const sptr<ICameraDeviceVdiCallback> &callbackObj,
         sptr<ICameraDeviceVdi> &device) = 0;

    virtual int32_t SetFlashlight(const std::string &cameraId, bool isEnable) = 0;
};


struct VdiWrapperCameraHost {
    struct HdfVdiBase base;
    ICameraHostVdi *module;
};
} // V1_0
} // Camera
} // VDI
} // OHOS
#endif // OHOS_VDI_CAMERA_V1_0_ICAMERAHOSTVDI_H