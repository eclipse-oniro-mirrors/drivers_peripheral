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
#include <cmath>

#include "osal_mem.h"
#include "sensor_callback_impl_test_v2_1.h"
#include "sensor_type.h"

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V2_1 {
int32_t SensorCallbackImplTestV2_1::OnDataEvent(const HdfSensorEvents& event)
{
    HDF_LOGI("%{public}s: sensorId=%{public}d", __func__, event.sensorId);
    (void)event;
    return HDF_SUCCESS;
}

int32_t SensorCallbackImplTestV2_1::OnDataEventAsync(const std::vector<HdfSensorEvents>& events)
{
    HDF_LOGI("%{public}s: sensorId=%{public}d, timestamp=%{public}lld", __func__,
        events[0].sensorId, events[0].timestamp);
    (void)events;
    return HDF_SUCCESS;
}
} // V2_1
} // Sensor
} // HDI
} // OHOS
