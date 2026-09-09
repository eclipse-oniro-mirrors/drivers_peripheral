/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef OHOS_HDI_PCIE_INTERFACE_SERVICE_H
#define OHOS_HDI_PCIE_INTERFACE_SERVICE_H

#include <functional>

#include "pcie_host_types.h"
#include "v1_0/ipcie_interface.h"

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace V1_0 {

struct PcieDdkServiceDeps {
    // Tests can replace these operations; otherwise, the DDK service uses the actual Host implementations.
    // Replaces PCIe device scanning.
    std::function<int32_t(std::vector<OHOS::HDI::Pcie::Driver::PcieDeviceInfo>&)> scanDevices;
    // Replaces the Host driver loading flow.
    std::function<OHOS::HDI::Pcie::Driver::PcieDriverHostExecutionResult(
        const std::vector<PcieDriverLoadItem>&)> executeDriverLoads;
};

/* HDI Stub: SA calls IPcieInterface methods → IPC → this service on Host side */
class PcieDdkService : public IPcieInterface {
public:
    // Tests can inject scanning and loading behavior; production uses the actual Host service by default.
    explicit PcieDdkService(PcieDdkServiceDeps driverLoadInjectedDeps = {});
    ~PcieDdkService() override = default;

    int32_t GetPcieDevices(std::vector<PcieDeviceInfo>& devices) override;
    int32_t LoadPcieDrivers(const std::vector<PcieDriverLoadItem>& loadRequests,
        std::vector<PcieDriverLoadResult>& loadResults) override;

private:
    // Retains optional test callbacks used by device queries and driver loading.
    PcieDdkServiceDeps driverLoadInjectedDeps_;
};

} // namespace V1_0
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
#endif
