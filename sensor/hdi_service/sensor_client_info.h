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

#ifndef HDI_SENSOR_CLIENT_H
#define HDI_SENSOR_CLIENT_H

#include <mutex>
#include <unordered_map>
#include "v1_1/isensor_interface.h"
#include "isensor_interface_vdi.h"

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V1_1 {

class SensorClientInfo {
public:
    SensorClientInfo();
    ~SensorClientInfo();
    explicit SensorClientInfo(const sptr<ISensorCallback> &callbackObj)
        : pollCallback_(callbackObj) {};
    void SetReportDataCb(const sptr<ISensorCallback> &callbackObj);
    sptr<ISensorCallback> GetReportDataCb();
private:
    sptr<ISensorCallback> pollCallback_;
};

} // V1_1
} // Sensor
} // HDI
} // OHOS

#endif // HDI_SENSOR_CLIENT_H