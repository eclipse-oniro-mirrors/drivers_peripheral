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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dlfcn.h>
#include "wifi_vendor_hal_list.h"
#include "wifi_vendor_hal_stubs.h"

namespace OHOS {
namespace HDI {
namespace Wlan {
namespace Chip {
namespace V2_0 {

const std::string VENDOR_HARDWARE_PATH = "libwifi_hal_hw.z.so";
const std::string VENDOR_DAFAULT_PATH = "libwifi_hal_default.z.so";

WifiVendorHalList::WifiVendorHalList(
    const std::weak_ptr<IfaceTool> ifaceTool)
    : ifaceTool_(ifaceTool) {}

std::vector<std::shared_ptr<WifiVendorHal>> WifiVendorHalList::GetHals()
{
    if (vendorHals_.empty()) {
        InitVendorHalsDescriptorList();
        for (auto& desc : descs_) {
            std::shared_ptr<WifiVendorHal> hal =
                std::make_shared<WifiVendorHal>(ifaceTool_, desc.fn,
                                                desc.primary);
            vendorHals_.push_back(hal);
        }
    }
    return vendorHals_;
}

void WifiVendorHalList::InitVendorHalsDescriptorList()
{
    WifiHalLibDesc desc;
    std::string path = VENDOR_DAFAULT_PATH;
    if (strncmp(HAL_SO_NAME, "hardware", strlen(HAL_SO_NAME)) == 0) {
        path = VENDOR_HARDWARE_PATH;
    }
    desc.primary = true;
    if (LoadVendorHalLib(path, desc)) {
        if (desc.primary) {
            descs_.insert(descs_.begin(), desc);
        } else {
            descs_.push_back(desc);
        }
    }
}

bool WifiVendorHalList::LoadVendorHalLib(const std::string& path, WifiHalLibDesc &desc)
{
    void* fd = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    InitWifiVendorHalFuncTableT initfn;
    WifiError res;
    if (!fd) {
        HDF_LOGE("failed to open vendor hal library: %{public}s", path.c_str());
        return false;
    }
    initfn = reinterpret_cast<InitWifiVendorHalFuncTableT>(dlsym(
        fd, "InitWifiVendorHalFuncTable"));
    if (!initfn) {
        HDF_LOGE("InitWifiVendorHalFuncTable not found in: %{public}s", path.c_str());
        dlclose(fd);
        return false;
    }
    if (!InitHalFuncTableWithStubs(&desc.fn)) {
        HDF_LOGE("Can not initialize the basic function pointer table");
        dlclose(fd);
        return false;
    }
    res = initfn(&desc.fn);
    if (res != HAL_SUCCESS) {
        HDF_LOGE("failed to initialize the vendor func table in: %{public}s, error: %{public}d",
            path.c_str(), res);
        dlclose(fd);
        return false;
    }
    res = desc.fn.vendorHalPreInit();
    if (res != HAL_SUCCESS && res != HAL_NOT_SUPPORTED) {
        HDF_LOGE("early initialization failed in: %{public}s, error: %{public}d", path.c_str(), res);
        dlclose(fd);
        return false;
    }
    desc.handle = fd;
    return true;
}

} // namespace v2_0
} // namespace Chip
} // namespace Wlan
} // namespace HDI
} // namespace OHOS