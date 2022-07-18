/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#ifndef OHOS_HDI_LOCATION_LOCATION_VENDOR_INTERFACE_H
#define OHOS_HDI_LOCATION_LOCATION_VENDOR_INTERFACE_H

#include <mutex>

#include "location_vendor_lib.h"

namespace OHOS {
namespace HDI {
namespace Location {
class LocationVendorInterface {
public:
    const GnssVendorInterface *GetGnssVendorInterface();
    const void *GetModuleInterface(int moduleId);
    static LocationVendorInterface* GetInstance();
    static void DestroyInstance();
private:
    LocationVendorInterface();
    ~LocationVendorInterface();
    void Init();
    void CleanUp();

    static LocationVendorInterface* instance_;
    static std::mutex mutex_;
    void *vendorHandle_ = nullptr;
    const GnssVendorInterface *vendorInterface_ = nullptr;
};
} // Location
} // HDI
} // OHOS
#endif /* OHOS_HDI_LOCATION_LOCATION_VENDOR_INTERFACE_H */