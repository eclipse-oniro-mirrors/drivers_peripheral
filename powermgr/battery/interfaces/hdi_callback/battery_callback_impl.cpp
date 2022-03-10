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

#include "battery_callback_impl.h"
#include "hdf_base.h"
#include "hdf_log.h"

#define HDF_LOG_TAG BatteryCallbackImpl

namespace OHOS {
namespace HDI {
namespace Battery {
namespace V1_0 {
BatteryCallbackImpl::BatteryEventCallback BatteryCallbackImpl::eventCb_ = nullptr;

int32_t BatteryCallbackImpl::Update(const CallbackInfo& event)
{
    HDF_LOGI("%{public}s: CallbackInfo capacity=%{public}d, voltage=%{public}d, temperature=%{public}d, " \
        "healthState=%{public}d, pluggedType=%{public}d, pluggedMaxCurrent=%{public}d, " \
        "pluggedMaxVoltage=%{public}d, chargeState=%{public}d, chargeCounter=%{public}d, present=%{public}d, " \
        "technology=%{public}s", __func__, event.capacity, event.voltage,
        event.temperature, event.healthState, event.pluggedType,
        event.pluggedMaxCurrent, event.pluggedMaxVoltage, event.chargeState,
        event.chargeCounter, event.present, event.technology.c_str());

    if (eventCb_ == nullptr) {
        return HDF_FAILURE;
    }
    return eventCb_(event);
}

int32_t BatteryCallbackImpl::RegisterBatteryEvent(const BatteryEventCallback& eventCb)
{
    eventCb_ = eventCb;
    return HDF_SUCCESS;
}
} // V1_0
} // Battery
} // HDI
} // OHOS