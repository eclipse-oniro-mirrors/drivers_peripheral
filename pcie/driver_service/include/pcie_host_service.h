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

#ifndef OHOS_HDI_PCIE_DRIVER_HOST_SERVICE_H
#define OHOS_HDI_PCIE_DRIVER_HOST_SERVICE_H

#include "pcie_host_types.h"
#include "pcie_sysfs_scanner.h"
#include <functional>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace Driver {

struct PcieDriverHostDeps {
    // Tests can supply these callbacks; otherwise, the service uses the actual system interfaces.
    // Checks whether the EDM SA is available.
    std::function<bool()> isEdmSaAvailable;
    // Loads the EDM SA and returns the result.
    std::function<bool()> loadEdmSaAndWait;
    // Replaces driver path resolution, file opening, and file information validation.
    std::function<int32_t(const std::string&, std::string&, int&, struct stat&, int&)> resolveDriverPath;
    // Replaces kernel module insertion so tests do not actually load drivers.
    std::function<int(int, const std::string&)> finitModule;
};

class PcieDriverHostService {
public:
    // Tests can replace the scanner and dependency callbacks; default construction uses actual implementations.
    explicit PcieDriverHostService(PcieSysfsScanner scanner = PcieSysfsScanner(),
        PcieDriverHostDeps injectedDependencies = {});

    int32_t                         GetPcieDevices(std::vector<PcieDeviceInfo>& devices);
    bool                          StartEdmSaForPcieDevices();
    PcieDriverHostExecutionResult     ExecuteDriverLoads(
        const std::vector<PcieDriverLoadItem>& loadRequests);

private:
    bool            EnsureEdmSaLoaded() const;
    PcieDriverLoadResult LoadDriverModule(const PcieDriverLoadItem& loadRequest) const;

    PcieSysfsScanner scanner_;
    // Optional test callbacks; absent callbacks use the production operations described above.
    PcieDriverHostDeps injectedDependencies_;
};

} // namespace Driver
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
#endif
