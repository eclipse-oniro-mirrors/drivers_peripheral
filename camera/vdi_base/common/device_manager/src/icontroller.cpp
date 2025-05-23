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

#include "icontroller.h"

namespace OHOS {
namespace Camera {
IController::IController() {}

IController::IController(std::string hardwareName) : hardwareName_(hardwareName) {}

IController::~IController() {}

bool IController::GetPowerOnState()
{
    std::lock_guard<std::mutex> l(powerOnStatelock_);
    return powerOnState_;
}

void IController::SetPowerOnState(bool powerOnState)
{
    std::lock_guard<std::mutex> l(powerOnStatelock_);
    powerOnState_ = powerOnState;
}

void IController::SetMetaDataFlag(bool metaDataFlag)
{
    (void)metaDataFlag;
    return;
}

void IController::SetMetaDataCallBack(const MetaDataCb cb)
{
    (void)cb;
    return;
}
} // namespace Camera
} // namespace OHOS