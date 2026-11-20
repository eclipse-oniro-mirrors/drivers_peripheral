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

#ifndef OHOS_HDI_PCIE_SYSFS_SCANNER_H
#define OHOS_HDI_PCIE_SYSFS_SCANNER_H

#include "pcie_host_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace Driver {

/* Scans /sys/bus/pci/devices/<id>/ for vendor, device, class,
 * modalias, and bound driver (driver symlink target). */
class PcieSysfsScanner {
public:
    explicit PcieSysfsScanner(std::string sysfsRoot = "/sys/bus/pci");
    int32_t Scan(std::vector<PcieDeviceInfo>& deviceInfos) const;

private:
    std::string ReadOneLine(const std::string& path) const;
    PcieDeviceInfo BuildDeviceInfo(const std::string& devicePath) const;
    std::string sysfsRoot_;
};

} // namespace Driver
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
#endif
