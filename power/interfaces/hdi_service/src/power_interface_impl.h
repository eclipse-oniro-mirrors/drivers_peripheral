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

#ifndef OHOS_HDI_POWER_V1_0_POWERINTERFACEIMPL_H
#define OHOS_HDI_POWER_V1_0_POWERINTERFACEIMPL_H

#include <functional>
#include "iremote_object.h"
#include "refbase.h"
#include "v1_0/ipower_interface.h"

namespace OHOS {
namespace HDI {
namespace Power {
namespace V1_0 {
class PowerInterfaceImpl : public IPowerInterface {
public:
    virtual ~PowerInterfaceImpl() {}

    int32_t RegisterCallback(const sptr<IPowerHdiCallback>& ipowerHdiCallback) override;

    int32_t StartSuspend() override;

    int32_t StopSuspend() override;

    int32_t ForceSuspend() override;

    int32_t SuspendBlock(const std::string& name) override;

    int32_t SuspendUnblock(const std::string& name) override;

    int32_t PowerDump(std::string& info) override;

    class PowerDeathRecipient : public IRemoteObject::DeathRecipient {
    public:
        explicit PowerDeathRecipient(
            const wptr<PowerInterfaceImpl> &powerInterfaceImpl) : powerInterfaceImpl_(powerInterfaceImpl) {};
        virtual ~PowerDeathRecipient() = default;
        virtual void OnRemoteDied(const wptr<IRemoteObject> &object) override;
    private:
        wptr<PowerInterfaceImpl> powerInterfaceImpl_;
    };

private:
    int32_t UnRegister();
    int32_t AddPowerDeathRecipient(const sptr<IPowerHdiCallback> &callback);
    int32_t RemovePowerDeathRecipient(const sptr<IPowerHdiCallback> &callback);
};
} // V1_0
} // Power
} // HDI
} // OHOS

#endif // OHOS_HDI_POWER_V1_0_POWERINTERFACEIMPL_H