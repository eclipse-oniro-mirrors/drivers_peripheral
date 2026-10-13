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

#ifndef MOCK_DDK_PERMISSION_CONTROL_H
#define MOCK_DDK_PERMISSION_CONTROL_H

namespace mock_ddk {
/* Controls the DdkPermissionManager test double compiled into this test binary. */
void SetPermissionGranted(bool granted);
bool IsPermissionGranted();
void SetHapApiVersion(int32_t apiVersion);
int32_t GetHapApiVersionForTest();

/* Failure injection for the DDK host / PnP entry points (see mock_ddk_pnp.cpp). */
void FailNextListenerAdd(int32_t errorCode);
void FailNextUsbInitHostSdk(int32_t errorCode);
void FailNextUsbExitHostSdk(int32_t errorCode);
} // namespace mock_ddk

#endif // MOCK_DDK_PERMISSION_CONTROL_H
