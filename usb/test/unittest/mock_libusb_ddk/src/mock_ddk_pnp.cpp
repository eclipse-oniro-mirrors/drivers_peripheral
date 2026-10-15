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
 * Test doubles for the DDK host / PnP entry points used by UsbDdkService::Init()
 * and Release().
 *
 * The production implementations rely on the HDF device manager and the raw USB
 * SDK being initialised by the driver host process (DdkListenerMgrAdd() locks a
 * mutex created by DdkListenerMgrInit(), which is not available in a plain test
 * process). Symbols defined by the executable take precedence over the shared
 * libraries, so these stubs keep the service test focused on the DDK logic.
 */

#include <hdf_base.h>
#include <hdf_io_service_if.h>

#include "usb_ddk_interface.h"
#include "usb_session.h"

#include "mock_ddk_permission_control.h"

namespace {
int32_t g_nextListenerAddError = 0;
int32_t g_nextUsbInitError = 0;
int32_t g_nextUsbExitError = 0;
} // namespace

namespace mock_ddk {
void FailNextListenerAdd(int32_t errorCode)
{
    g_nextListenerAddError = errorCode;
}

void FailNextUsbInitHostSdk(int32_t errorCode)
{
    g_nextUsbInitError = errorCode;
}

void FailNextUsbExitHostSdk(int32_t errorCode)
{
    g_nextUsbExitError = errorCode;
}
} // namespace mock_ddk

extern "C" {
int32_t UsbInitHostSdk(struct UsbSession **session)
{
    (void)session;
    if (g_nextUsbInitError != 0) {
        int32_t error = g_nextUsbInitError;
        g_nextUsbInitError = 0;
        return error;
    }
    return HDF_SUCCESS;
}

int32_t UsbExitHostSdk(const struct UsbSession *session)
{
    (void)session;
    if (g_nextUsbExitError != 0) {
        int32_t error = g_nextUsbExitError;
        g_nextUsbExitError = 0;
        return error;
    }
    return HDF_SUCCESS;
}

int32_t DdkListenerMgrAdd(struct HdfDevEventlistener *listener)
{
    if (listener == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (g_nextListenerAddError != 0) {
        int32_t error = g_nextListenerAddError;
        g_nextListenerAddError = 0;
        return error;
    }
    return HDF_SUCCESS;
}

int32_t DdkListenerMgrRemove(struct HdfDevEventlistener *listener)
{
    if (listener == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    return HDF_SUCCESS;
}
} // extern "C"
