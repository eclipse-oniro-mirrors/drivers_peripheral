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

#ifndef OHOS_HDI_MOTION_V1_1_IMOTIONCALLBACK_VDI_H
#define OHOS_HDI_MOTION_V1_1_IMOTIONCALLBACK_VDI_H

#include <hdf_base.h>
#include <hdi_base.h>
#include <vector>
#include <iproxy_broker.h>
#include "refbase.h"

namespace OHOS {
namespace HDI {
namespace Motion {
namespace V1_1 {

struct HdfMotionEventVdi {
    int32_t motion;
    int32_t result;
    int32_t status;
    int32_t datalen;
    std::vector<int32_t> data;
};

class IMotionCallbackVdi : public HdiBase {
public:
    virtual ~IMotionCallbackVdi() = default;
    virtual int32_t OnDataEventVdi(const HdfMotionEventVdi& eventVdi) = 0;
    virtual sptr<IRemoteObject> HandleCallbackDeath() = 0;
};

} // V1_1
} // Motion
} // HDI
} // OHOS

#endif // OHOS_HDI_MOTION_V1_1_IMOTIONCALLBACK_VDI_H
