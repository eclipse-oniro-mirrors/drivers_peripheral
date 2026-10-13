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

/*
 * Test double for DdkPermissionManager.
 *
 * The real implementation asks AccessTokenKit, which cannot grant the USB DDK
 * permission to a native (non HAP) test process. Compiling this file instead of
 * the production one lets the tests drive both branches: no permission and
 * granted, without any dependency on the system security service.
 */

#include "usb_ddk_permission.h"

#include "mock_ddk_permission_control.h"

/* Default API version reported by the test double (named to avoid a magic literal). */
#define MOCK_HAP_API_VERSION 20

namespace {
bool g_permissionGranted = true;
int32_t g_hapApiVersion = MOCK_HAP_API_VERSION;
} // namespace

namespace mock_ddk {
void SetPermissionGranted(bool granted)
{
    g_permissionGranted = granted;
}

bool IsPermissionGranted()
{
    return g_permissionGranted;
}

void SetHapApiVersion(int32_t apiVersion)
{
    g_hapApiVersion = apiVersion;
}

int32_t GetHapApiVersionForTest()
{
    return g_hapApiVersion;
}
} // namespace mock_ddk

namespace OHOS {
namespace HDI {
namespace Usb {
namespace Ddk {
bool DdkPermissionManager::VerifyPermission(const std::string &permissionName)
{
    (void)permissionName;
    return g_permissionGranted;
}

void DdkPermissionManager::Reset()
{
    g_permissionGranted = true;
}

int32_t DdkPermissionManager::GetHapApiVersion(int32_t &apiVersion)
{
    apiVersion = g_hapApiVersion;
    return 0;
}
} // namespace Ddk
} // namespace Usb
} // namespace HDI
} // namespace OHOS
