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

#include "pcie_ddk_service.h"
#include "pcie_host_service.h"

#include <cstddef>
#include <new>

#include "hdf_base.h"
#include "hdf_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
static constexpr uint32_t LOG_DOMAIN = 0xD002516;

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace V1_0 {

namespace {
constexpr size_t MAX_PCIE_LOAD_REQUESTS = 64;
constexpr size_t MAX_PCIE_LOAD_OPTIONS_LENGTH = 4096;
} // namespace

extern "C" IPcieInterface *PcieInterfaceImplGetInstance(void)
{
    return new (std::nothrow) PcieDdkService();
}

static PcieDeviceInfo ConvertDevice(const OHOS::HDI::Pcie::Driver::PcieDeviceInfo &source)
{
    PcieDeviceInfo target;
    target.boardId = source.boardId;
    target.sysfsPath = source.sysfsPath;
    target.vendorId = source.vendorId;
    target.deviceId = source.deviceId;
    target.classCode = source.classCode;
    target.modalias = source.modalias;
    target.isModaliasValid = source.isModaliasValid;
    target.boundDriver = source.boundDriver;
    return target;
}

PcieDdkService::PcieDdkService(PcieDdkServiceDeps driverLoadInjectedDeps)
    : driverLoadInjectedDeps_(std::move(driverLoadInjectedDeps))
{
    if (!driverLoadInjectedDeps_.scanDevices) {
        driverLoadInjectedDeps_.scanDevices = [](std::vector<OHOS::HDI::Pcie::Driver::PcieDeviceInfo> &devices) {
            OHOS::HDI::Pcie::Driver::PcieSysfsScanner scanner;
            return scanner.Scan(devices);
        };
    }
    if (!driverLoadInjectedDeps_.executeDriverLoads) {
        driverLoadInjectedDeps_.executeDriverLoads = [](const std::vector<PcieDriverLoadItem> &loadRequests) {
            OHOS::HDI::Pcie::Driver::PcieDriverHostService hostService;
            return hostService.ExecuteDriverLoads(loadRequests);
        };
    }
}

int32_t PcieDdkService::GetPcieDevices(std::vector<PcieDeviceInfo> &devices)
{
    std::vector<OHOS::HDI::Pcie::Driver::PcieDeviceInfo> scannedDevices;
    // Tests can inject scan results; otherwise, the Host service scans PCIe devices.
    int32_t scanStatus = driverLoadInjectedDeps_.scanDevices(scannedDevices);
    if (scanStatus != HDF_SUCCESS) {
        devices.clear();
        return HDF_FAILURE;
    }
    devices.clear();
    devices.reserve(scannedDevices.size());
    for (const auto &scannedDevice : scannedDevices) {
        devices.push_back(ConvertDevice(scannedDevice));
    }
    return HDF_SUCCESS;
}

int32_t PcieDdkService::LoadPcieDrivers(
    const std::vector<PcieDriverLoadItem> &loadRequests, std::vector<PcieDriverLoadResult> &loadResults)
{
    if (loadRequests.size() > MAX_PCIE_LOAD_REQUESTS) {
        HDF_LOGE("PCIe load request count exceeds limit: %{public}zu", loadRequests.size());
        loadResults.clear();
        return HDF_FAILURE;
    }
    for (const auto &loadRequest : loadRequests) {
        if (loadRequest.options.size() > MAX_PCIE_LOAD_OPTIONS_LENGTH) {
            HDF_LOGE("PCIe load options length exceeds limit: %{public}zu", loadRequest.options.size());
            loadResults.clear();
            return HDF_FAILURE;
        }
    }
    HDF_LOGI("PCIE_LOAD_TRACE DDK_LOAD_ENTER requests=%{public}zu", loadRequests.size());
    // Tests can inject execution results; otherwise, the Host service loads the drivers.
    auto executionResult = driverLoadInjectedDeps_.executeDriverLoads(loadRequests);
    HDF_LOGI("PCIE_LOAD_TRACE DDK_LOAD_EXECUTE results=%{public}zu", executionResult.loadResults.size());
    loadResults = std::move(executionResult.loadResults);
    HDF_LOGI("PCIE_LOAD_TRACE DDK_LOAD_REPLY status=%{public}d results=%{public}zu", HDF_SUCCESS, loadResults.size());
    return HDF_SUCCESS;
}

} // namespace V1_0
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
