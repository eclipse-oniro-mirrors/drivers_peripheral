/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "usbd_function.h"

#include <dlfcn.h>
#include <unistd.h>
#include <cerrno>

#include "devmgr_hdi.h"
#include "hdf_log.h"
#include "hdf_remote_service.h"
#include "hdf_sbuf.h"
#include "idevmgr_hdi.h"
#include "iservmgr_hdi.h"
#include "message_option.h"
#include "message_parcel.h"
#include "osal_time.h"
#include "parameter.h"
#include "securec.h"
#include "string_ex.h"
#include "usbd_type.h"
#include "usbfn_mtp_impl.h"
#include "usbd_wrapper.h"
#include "usbfn_device.h"
#include "usb_report_sys_event.h"

namespace OHOS {
namespace HDI {
namespace Usb {
namespace V1_2 {
uint32_t UsbdFunction::currentFuncs_ = USB_FUNCTION_HDC;
std::mutex UsbdFunction::setFunctionLock_;

using OHOS::HDI::DeviceManager::V1_0::IDeviceManager;
using OHOS::HDI::ServiceManager::V1_0::IServiceManager;
using OHOS::HDI::Usb::Gadget::Mtp::V1_0::IUsbfnMtpInterface;
using GetMtpImplFunc = void*(*)();

constexpr uint32_t UDC_NAME_MAX_LEN = 32;
constexpr int32_t WAIT_UDC_MAX_LOOP = 30;
constexpr uint32_t WAIT_UDC_TIME = 100000;
constexpr int32_t WRITE_UDC_MAX_RETRY = 5;
/* mtp and ptp use same driver and same service */
static std::string MTP_PTP_SERVICE_NAME {"usbfn_mtp_interface_service"};
#define UDC_PATH "/config/usb_gadget/g1/UDC"

static void *g_libHandle = nullptr;
static GetMtpImplFunc g_getMtpImpl = nullptr;
static struct UsbFnDevice *g_mtpDev = nullptr;

static void InitGetMtpImpl()
{
    if (g_getMtpImpl != nullptr) {
        return;
    }

    g_libHandle = dlopen("libusbfn_mtp_interface_service_1.0.z.so", RTLD_LAZY);
    if (g_libHandle == nullptr) {
        HDF_LOGE("%{public}s dlopen failed: %{public}s", __func__, dlerror());
        return;
    }

    void *funcPtr = dlsym(g_libHandle, "UsbfnMtpInterfaceImplGetInstance");
    if (funcPtr == nullptr) {
        HDF_LOGE("%{public}s dlsym failed: %{public}s", __func__, dlerror());
        dlclose(g_libHandle);
        g_libHandle = nullptr;
        return;
    }

    g_getMtpImpl = reinterpret_cast<GetMtpImplFunc>(funcPtr);
}

static void ReleaseGetMtpImpl()
{
    g_getMtpImpl = nullptr;
    if (g_libHandle != nullptr) {
        dlclose(g_libHandle);
        g_libHandle = nullptr;
    }
}

static IUsbfnMtpInterface *GetUsbfnMtpImpl()
{
    InitGetMtpImpl();
    if (g_getMtpImpl == nullptr) {
        return nullptr;
    }

    void *instance = g_getMtpImpl();
    if (instance != nullptr) {
        return reinterpret_cast<IUsbfnMtpInterface *>(instance);
    }
    return nullptr;
}

static bool CreateMtpDeviceByHcs()
{
    struct DeviceResourceIface *iface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (iface == nullptr || iface->GetRootNode == nullptr || iface->GetChildNode == nullptr ||
        iface->GetString == nullptr) {
        HDF_LOGE("%{public}s: DeviceResourceGetIfaceInstance failed", __func__);
        return false;
    }

    const struct DeviceResourceNode *rootNode = iface->GetRootNode();
    if (rootNode == nullptr) {
        HDF_LOGE("%{public}s: GetRootNode failed", __func__);
        return false;
    }

    const struct DeviceResourceNode *fnConfigNode = iface->GetChildNode(rootNode, "usbfn_config");
    if (fnConfigNode == nullptr) {
        HDF_LOGE("%{public}s: GetChildNode failed", __func__);
        return false;
    }

    const char *udcName;
    if (iface->GetString(fnConfigNode, "udc_name", &udcName, "invalid_udc_name") != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: get udc name failed", __func__);
        return false;
    }

    struct UsbFnDescriptorData usbFnData = { .type = USBFN_DESC_DATA_TYPE_PROP, .functionMask = USB_FUNCTION_MTP };
    usbFnData.property = fnConfigNode;

    g_mtpDev = const_cast<struct UsbFnDevice *>(UsbFnCreateDefaultDevice(udcName, &usbFnData));
    if (g_mtpDev == nullptr) {
        HDF_LOGE("%{public}s: create mtp function device failed", __func__);
        return false;
    }
    HDF_LOGI("%{public}s: create mtp function device success", __func__);
    return true;
}

static bool DestroyMtpDevice()
{
    if (g_mtpDev == nullptr) {
        HDF_LOGI("%{public}s: g_mtpDev is null", __func__);
        return true;
    }
    if (UsbFnRemoveDevice(g_mtpDev) == HDF_SUCCESS) {
        g_mtpDev = nullptr;
        HDF_LOGI("%{public}s: remove mtp device success", __func__);
        return true;
    }
    HDF_LOGE("%{public}s: remove mtp device failed", __func__);
    return false;
}

static bool CancelMtpReady()
{
    if (!DestroyMtpDevice()) {
        HDF_LOGE("%{public}s:DestroyMtpDevice failed", __func__);
        return false;
    }

    int32_t status = SetParameter(SYS_USB_MTP_READY, SYS_USB_MTP_NOT_READY);
    if (status != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:remove mtp config failed, error=%{public}d", __func__, status);
        return false;
    }
    return true;
}

int32_t UsbdFunction::SendCmdToService(const char *name, int32_t cmd, unsigned char funcMask)
{
    auto servMgr = IServiceManager::Get();
    if (servMgr == nullptr) {
        HDF_LOGE("%{public}s: get IServiceManager failed", __func__);
        return HDF_FAILURE;
    }

    sptr<IRemoteObject> remote = servMgr->GetService(name);
    if (remote == nullptr) {
        HDF_LOGE("%{public}s: get remote object failed: %{public}s", __func__, name);
        return HDF_FAILURE;
    }

    OHOS::MessageParcel data;
    OHOS::MessageParcel reply;
    OHOS::MessageOption option;

    if (!data.WriteInterfaceToken(Str8ToStr16(HDF_USB_USBFN_DESC))) {
        HDF_LOGE("%{public}s: WriteInterfaceToken failed", __func__);
        return HDF_FAILURE;
    }

    if (!data.WriteUint8(funcMask)) {
        HDF_LOGE("%{public}s: WriteInt8 failed: %{public}d", __func__, funcMask);
        return HDF_FAILURE;
    }

    if(!data.WriteUint8(isActive)) {
        HDF_LOGE("%{public}s: WriteUInt8 failed: %{public}d", __func__, isActive);
        return HDF_FAILURE;
    }

    int32_t ret = remote->SendRequest(cmd, data, reply, option);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: send request to %{public}s failed, ret=%{public}d", __func__, name, ret);
        return ret;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::InitMtp()
{
    int32_t ret = UsbdRegisterDevice(MTP_PTP_SERVICE_NAME);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: register mtp device failed: %{public}d", __func__, ret);
        return ret;
    }
    auto serviceImpl = GetUsbfnMtpImpl();
    if (serviceImpl == nullptr) {
        HDF_LOGE("%{public}s: failed to get of implement service", __func__);
        return HDF_FAILURE;
    }
    ret = serviceImpl->Init();
    if (ret != HDF_SUCCESS) {
        UsbdUnregisterDevice(MTP_PTP_SERVICE_NAME);
        HDF_LOGE("%{public}s: init mtp device failed: %{public}d", __func__, ret);
    }
    HDF_LOGI("%{public}s: start Init done", __func__);
    return ret;
}

int32_t UsbdFunction::ReleaseMtp()
{
    auto serviceImpl = GetUsbfnMtpImpl();
    if (serviceImpl == nullptr) {
        HDF_LOGE("%{public}s: failed to get of implement service", __func__);
        return HDF_FAILURE;
    }
    int32_t ret = serviceImpl->Release();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: release mtp device failed: %{public}d", __func__, ret);
    }
    ReleaseGetMtpImpl();

    UsbdUnregisterDevice(MTP_PTP_SERVICE_NAME);
    HDF_LOGI("%{public}s: release Mtp done", __func__);
    return ret;
}

bool UsbdFunction::IsHdcOpen()
{
    char persistConfig[UDC_NAME_MAX_LEN] = {0};
    int32_t ret = GetParameter("persist.sys.usb.config", "invalid", persistConfig, UDC_NAME_MAX_LEN);
    if (ret <= 0) {
        HDF_LOGE("%{public}s:GetPersistParameter failed", __func__);
        return false;
    }
    const char HDC_SIGNATURE[] = "hdc";
    if (strstr(persistConfig, HDC_SIGNATURE) != nullptr) {
        HDF_LOGI("%{public}s:hdc is opening", __func__);
        return true;
    }
    return false;
}

int32_t UsbdFunction::RemoveHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_OFF);
    if (status != 0) {
        HDF_LOGE("%{public}s:remove hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::AddHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_ON);
    if (status != 0) {
        HDF_LOGE("%{public}s:add hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }

    status = SetParameter(PERSIST_SYS_USB_CONFIG, HDC_CONFIG_ON);
    if (status != 0) {
        HDF_LOGE("%{public}s:add hdc persist config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToRndis()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_RNDIS);
    if (status != 0) {
        HDF_LOGE("%{public}s:add rndis config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToStorage()
{
    if (UsbdFuntion::SetDDKFunction(USB_FUNCTION_MTP, false) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:SetDDKFunction failed", __func__);
        return HDF_FAILURE;
    }
    HDF_LOGE("%{public}s:add mtp function success", __func__);

    int32_t status = SetParameter(SYS_USB_MTP_READY, SYS_USB_MTP_HAS_READY);
    if (status != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:add mtp config failed, error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }

    status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_STORAGE);
    if (status != 0) {
        HDF_LOGE("%{public}s:add storage config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }

    status = SetParameter(PERSIST_SYS_USB_CONFIG, HDC_CONFIG_STORAGE);
    if (status != 0) {
        HDF_LOGE("%{public}s:add storage persist config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToRndisHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_RNDIS_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add rndis hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToManufactureHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_MANUFACTURE_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add manufacture hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToMtp()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_MTP);
    if (status != 0) {
        HDF_LOGE("%{public}s:add MTP config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToPtp()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_PTP);
    if (status != 0) {
        HDF_LOGE("%{public}s:add PTP config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToMtpHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_MTP_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add MTP&HDC config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToPtpHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_PTP_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add PTP&HDC config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToStorageHdc()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_STORAGE_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add storage hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToUsbAccessory()
{
    HDF_LOGD("%{public}s enter", __func__);
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_AOA);
    if (status != 0) {
        HDF_LOGE("%{public}s:add aoa config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToNcm()
{
    HDF_LOGD("%{public}s enter", __func__);
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_NCM);
    if (status != 0) {
        HDF_LOGE("%{public}s:add ncm config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToNcmHdc()
{
    HDF_LOGD("%{public}s enter", __func__);
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_NCM_HDC);
    if (status != 0) {
        HDF_LOGE("%{public}s:add ncm hdc config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToDevModeAuth()
{
    int32_t status = SetParameter(SYS_USB_CONFIG, HDC_CONFIG_DEVMODE_AUTH);
    if (status != 0) {
        HDF_LOGE("%{public}s:add devmode_auth config error = %{public}d", __func__, status);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::SetFunctionToNone()
{
    uint32_t ddkFuns = currentFuncs_ & USB_DDK_FUNCTION_SUPPORT;
    if (ddkFuns > 0) {
        if ((ddkFuns & USB_FUNCTION_ACM) != 0) {
            UsbdFunction::SendCmdToService(ACM_SERVICE_NAME, ACM_RELEASE, USB_FUNCTION_ACM);
            UsbdUnregisterDevice(std::string(ACM_SERVICE_NAME));
        }
        if ((ddkFuns & USB_FUNCTION_ECM) != 0) {
            UsbdFunction::SendCmdToService(ECM_SERVICE_NAME, ECM_RELEASE, USB_FUNCTION_ECM);
            UsbdUnregisterDevice(std::string(ECM_SERVICE_NAME));
        }
        if ((ddkFuns & USB_FUNCTION_MTP) != 0 || (ddkFuns & USB_FUNCTION_PTP) != 0) {
            if (ReleaseMtp() != HDF_SUCCESS) {
                HDF_LOGE("%{public}s: release mtp failed", __func__);
            }
        }
    }
    UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_DEL, USB_DDK_FUNCTION_SUPPORT);
    UsbdUnregisterDevice(std::string(DEV_SERVICE_NAME));
    int32_t ret = RemoveHdc();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: RemoveHdc error, ret = %{public}d", __func__, ret);
        return ret;
    }

    ret = UsbdWaitToNone();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: UsbdWaitToNone error, ret = %{public}d", __func__, ret);
        UsbReportSysEvent::ReportUsbRecognitionFailSysEvent("UsbdSetFunction", ret, "UsbdWaitToNone error");
        return ret;
    }
    currentFuncs_ = USB_FUNCTION_NONE;
    return ret;
}

int32_t UsbdFunction::SetDDKFunction(uint32_t funcs, bool isActive)
{
    HDF_LOGD("%{public}s: SetDDKFunction funcs=%{public}d", __func__, funcs);
    uint32_t ddkFuns = static_cast<uint32_t>(funcs) & USB_DDK_FUNCTION_SUPPORT;
    if (ddkFuns == 0) {
        HDF_LOGE("%{public}s: not use ddkfunction", __func__);
        return HDF_SUCCESS;
    }
    int32_t ret = UsbdRegisterDevice(std::string(DEV_SERVICE_NAME));
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: failed to register device", __func__);
        return ret;
    }
    if (UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_ADD, ddkFuns, isActive)) {
        HDF_LOGE("%{public}s: create dev error: %{public}d", __func__, ddkFuns);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdWriteUdc(char* udcName, size_t len)
{
    FILE *fpWrite = fopen(UDC_PATH, "w");
    if (fpWrite == NULL) {
        HDF_LOGE("%{public}s: fopen failed", __func__);
        return HDF_ERR_BAD_FD;
    }

    size_t count = fwrite(udcName, len, 1, fpWrite);
    if (count != 1) {
        HDF_LOGE("%{public}s: fwrite failed, errno: %{public}d", __func__, errno);
        (void)fclose(fpWrite);
        return HDF_FAILURE;
    }

    if (ferror(fpWrite)) {
        HDF_LOGW("%{public}s: fwrite failed, errno: %{public}d", __func__, errno);
    }
    if (fclose(fpWrite) == EOF) {
        HDF_LOGE("%{public}s: flcose failed, errno: %{public}d", __func__, errno);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
int32_t UsbdFunction::UsbdReadUdc(char* udcName, size_t len)
{
    FILE *fpRead = fopen(UDC_PATH, "r");
    if (fpRead == NULL) {
        HDF_LOGE("%{public}s: fopen failed", __func__);
        return HDF_ERR_BAD_FD;
    }

    size_t count = fread(udcName, len, 1, fpRead);
    if (count != 1) {
        if (feof(fpRead)) {
            HDF_LOGI("%{public}s: fread end of file reached.", __func__);
        } else if (ferror(fpRead)) {
            HDF_LOGE("%{public}s: fread failed, errno: %{public}d", __func__, errno);
        } else {
            HDF_LOGW("%{public}s: fread len than expected", __func__);
        }
        (void)fclose(fpRead);
        return HDF_FAILURE;
    }

    if (fclose(fpRead) == EOF) {
        HDF_LOGW("%{public}s: flcose failed, errno: %{public}d", __func__, errno);
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdEnableDevice(int32_t funcs)
{
    // get udc name
    char udcName[UDC_NAME_MAX_LEN] = {0};
    int32_t ret = GetParameter("sys.usb.controller", "invalid", udcName, UDC_NAME_MAX_LEN);
    if (ret <= 0) {
        HDF_LOGE("%{public}s: GetParameter failed", __func__);
        return HDF_FAILURE;
    }

    char tmpName[UDC_NAME_MAX_LEN] = {0};
    for (int32_t i = 0; i < WRITE_UDC_MAX_RETRY; i++) {
        if (i != 0 && ret != HDF_SUCCESS) {
            ret = SetDDKFunction(funcs);
            if (ret != HDF_SUCCESS) {
                UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_DEL, USB_DDK_FUNCTION_SUPPORT);
                UsbdUnregisterDevice(std::string(DEV_SERVICE_NAME));
                usleep(WAIT_UDC_TIME);
                continue;
            }
        }
        ret = UsbdWriteUdc(udcName, strlen(udcName));
        if (ret != HDF_SUCCESS) {
            UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_DEL, USB_DDK_FUNCTION_SUPPORT);
            UsbdUnregisterDevice(std::string(DEV_SERVICE_NAME));
            usleep(WAIT_UDC_TIME);
            continue;
        }

        (void)memset_s(tmpName, UDC_NAME_MAX_LEN, 0, UDC_NAME_MAX_LEN);
        ret = UsbdReadUdc(tmpName, strlen(udcName));
        if (ret != HDF_SUCCESS) {
            UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_DEL, USB_DDK_FUNCTION_SUPPORT);
            UsbdUnregisterDevice(std::string(DEV_SERVICE_NAME));
            usleep(WAIT_UDC_TIME);
            continue;
        }

        if (strcmp(udcName, tmpName) == 0) {
            return HDF_SUCCESS;
        }
        HDF_LOGI("%{public}s:  tmpName: %{public}s", __func__, tmpName);
        usleep(WAIT_UDC_TIME);
    }

    if (strcmp(udcName, tmpName) != 0) {
        HDF_LOGE("%{public}s: strcmp failed", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdWaitUdc()
{
    // get udc name
    char udcName[UDC_NAME_MAX_LEN] = {0};
    int32_t ret = GetParameter("sys.usb.controller", "invalid", udcName, UDC_NAME_MAX_LEN - 1);
    if (ret <= 0) {
        HDF_LOGE("%{public}s: GetParameter failed", __func__);
        return HDF_FAILURE;
    }

    char tmpName[UDC_NAME_MAX_LEN] = {0};
    for (int32_t i = 0; i < WAIT_UDC_MAX_LOOP; i++) {
        (void)memset_s(tmpName, UDC_NAME_MAX_LEN, 0, UDC_NAME_MAX_LEN);
        ret = UsbdReadUdc(tmpName, strlen(udcName));
        if (ret != HDF_SUCCESS) {
            usleep(WAIT_UDC_TIME);
            continue;
        }
 
        if (strcmp(udcName, tmpName) == 0) {
            return HDF_SUCCESS;
        }
        HDF_LOGE("%{public}s: read UDC_PATH: %{public}s", __func__, tmpName);
        usleep(WAIT_UDC_TIME);
    }

    if (strcmp(udcName, tmpName) != 0) {
        HDF_LOGE("%{public}s: strcmp failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdWaitToNone()
{
    char stateName[UDC_NAME_MAX_LEN] = {0};
    for (int32_t i = 0; i < WAIT_UDC_MAX_LOOP; i++) {
        (void)memset_s(stateName, UDC_NAME_MAX_LEN, 0, UDC_NAME_MAX_LEN);
        int32_t ret = GetParameter(SYS_USB_STATE, "invalid", stateName, UDC_NAME_MAX_LEN - 1);
        if (ret <= 0) {
            HDF_LOGE("%{public}s: GetParameter failed", __func__);
            return HDF_FAILURE;
        }
        if (strcmp(stateName, HDC_CONFIG_OFF) == 0) {
            return HDF_SUCCESS;
        }
        usleep(WAIT_UDC_TIME);
    }

    if (strcmp(stateName, HDC_CONFIG_OFF) != 0) {
        HDF_LOGE("%{public}s: strcmp failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdInitDDKFunction(uint32_t funcs)
{
    int32_t ret;
    if ((funcs & USB_FUNCTION_ACM) != 0) {
        ret = UsbdRegisterDevice(std::string(ACM_SERVICE_NAME));
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: failed to register device", __func__);
            return HDF_FAILURE;
        }
        if (SendCmdToService(ACM_SERVICE_NAME, ACM_INIT, USB_FUNCTION_ACM) != 0) {
            UsbdUnregisterDevice(std::string(ACM_SERVICE_NAME));
            HDF_LOGE("%{public}s: acm init error", __func__);
            return HDF_FAILURE;
        }
        currentFuncs_ |= USB_FUNCTION_ACM;
    }
    if ((funcs & USB_FUNCTION_ECM) != 0) {
        ret = UsbdRegisterDevice(std::string(ECM_SERVICE_NAME));
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: failed to register device", __func__);
            return HDF_FAILURE;
        }
        if (SendCmdToService(ECM_SERVICE_NAME, ECM_INIT, USB_FUNCTION_ECM) != 0) {
            UsbdUnregisterDevice(std::string(ECM_SERVICE_NAME));
            HDF_LOGE("%{public}s: ecm init error", __func__);
            return HDF_FAILURE;
        }
        currentFuncs_ |= USB_FUNCTION_ACM;
    }
    if ((funcs & USB_FUNCTION_MTP) != 0 || (funcs & USB_FUNCTION_PTP) != 0) {
        ret = InitMtp();
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: failed to init mtp", __func__);
            UsbReportSysEvent::ReportUsbRecognitionFailSysEvent("UsbdSetFunction", ret, "UsbdWaitToNone error");
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdSetKernelFunction(int32_t kfuns, int32_t funcs)
{
    switch (funcs) {
        case USB_FUNCTION_MTP:
            HDF_LOGI("%{public}s: set MTP", __func__);
            return UsbdFunction::SetFunctionToMtp();
        case USB_FUNCTION_PTP:
            HDF_LOGI("%{public}s: set PTP", __func__);
            return UsbdFunction::SetFunctionToPtp();
        case USB_FUNCTION_MTP | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set MTP&HDC", __func__);
            return UsbdFunction::SetFunctionToMtpHdc();
        case USB_FUNCTION_PTP | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set PTP&HDC", __func__);
            return UsbdFunction::SetFunctionToPtpHdc();
    }
    switch (kfuns) {
        case USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set hdc", __func__);
            return UsbdFunction::AddHdc();
        case USB_FUNCTION_RNDIS:
            HDF_LOGI("%{public}s: set rndis", __func__);
            return UsbdFunction::SetFunctionToRndis();
        case USB_FUNCTION_STORAGE:
            HDF_LOGI("%{public}s: set mass_storage", __func__);
            return UsbdFunction::SetFunctionToStorage();
        case USB_FUNCTION_RNDIS | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set rndis hdc", __func__);
            return UsbdFunction::SetFunctionToRndisHdc();
        case USB_FUNCTION_STORAGE | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set storage hdc", __func__);
            return UsbdFunction::SetFunctionToStorageHdc();
        case USB_FUNCTION_MANUFACTURE | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set manufacture hdc", __func__);
            return UsbdFunction::SetFunctionToManufactureHdc();
        case USB_FUNCTION_ACCESSORY:
            HDF_LOGI("%{public}s: set usb accessory", __func__);
            return UsbdFunction::SetFunctionToUsbAccessory();
        case USB_FUNCTION_NCM:
            HDF_LOGI("%{public}s: set ncm", __func__);
            return UsbdFunction::SetFunctionToNcm();
        case USB_FUNCTION_NCM | USB_FUNCTION_HDC:
            HDF_LOGI("%{public}s: set ncm hdc", __func__);
            return UsbdFunction::SetFunctionToNcmHdc();
        case USB_FUNCTION_DEVMODE_AUTH:
            HDF_LOGI("%{public}s: set devmode_auth", __func__);
            return UsbdFunction::SetFunctionToDevModeAuth();
        default:
            HDF_LOGI("%{public}s: enable device", __func__);
            return UsbdEnableDevice(funcs);
    }
}

static bool g_isFirstBoot = true;

int32_t UsbdFunction::UsbdSetFunction(uint32_t funcs)
{
    std::lock_guard<std::mutex> lock(setFunctionLock_);
    HDF_LOGI("%{public}s: UsbdSetFunction funcs=%{public}d, currentFuncs=%{public}d", __func__, funcs, currentFuncs_);
    if (funcs == currentFuncs_) {
        return HDF_SUCCESS;
    }
    if ((funcs | USB_FUNCTION_SUPPORT) != USB_FUNCTION_SUPPORT) {
        HDF_LOGE("%{public}s: funcs invalid", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    if (currentFuncs_ == USB_FUNCTION_STORAGE && !CancelMtpReady()) {
        HDF_LOGE("%{public}s: CancelMtpReady failed", __func__);
        return HDF_FAILURE;
    }

    uint32_t kfuns = static_cast<uint32_t>(funcs) & (~USB_DDK_FUNCTION_SUPPORT);
    if (!g_isFirstBoot && UsbdFunction::SetFunctionToNone()) {
        HDF_LOGW("%{public}s: setFunctionToNone error", __func__);
    }

    g_isFirstBoot = false;

    if (funcs == USB_FUNCTION_NONE) {
        HDF_LOGW("%{public}s: setFunctionToNone", __func__);
        return HDF_SUCCESS;
    }

    if (UsbdFunction::SetDDKFunction(funcs)) {
        HDF_LOGE("%{public}s:SetDDKFunction error", __func__);
        UsbReportSysEvent::ReportUsbRecognitionFailSysEvent("UsbdSetFunction", HDF_FAILURE, "SetDDKFunction error");
        SetFunctionToStorage();
        currentFuncs_ = USB_FUNCTION_STORAGE;
        return HDF_FAILURE;
    }

    int32_t ret = UsbdSetKernelFunction(kfuns, funcs);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s, set kernel func failed", __func__);
        UsbReportSysEvent::ReportUsbRecognitionFailSysEvent("UsbdSetFunction", HDF_FAILURE,
            "SetUsbdSetKernelFunctionDDKFunction error");
        SetFunctionToStorage();
        return HDF_FAILURE;
    }
    currentFuncs_ |= kfuns;
    if (funcs == USB_FUNCTION_NONE) {
        HDF_LOGI("%{public}s, none function", __func__);
        return HDF_SUCCESS;
    }

    if (UsbdWaitUdc() != HDF_SUCCESS) {
        HDF_LOGE("%{public}s, wait udc failed", __func__);
        UsbReportSysEvent::ReportUsbRecognitionFailSysEvent("UsbdSetFunction", HDF_FAILURE, "UsbdWaitUdc error");
        SetFunctionToStorage();
        currentFuncs_ = USB_FUNCTION_STORAGE;
        return HDF_FAILURE;
    }
    if (UsbdInitDDKFunction(funcs) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s, init ddk func failed", __func__);
        UsbdFunction::SendCmdToService(DEV_SERVICE_NAME, FUNCTION_DEL, USB_DDK_FUNCTION_SUPPORT);
        UsbdUnregisterDevice(std::string(DEV_SERVICE_NAME));
        SetFunctionToStorage();
        currentFuncs_ = USB_FUNCTION_STORAGE;
        return HDF_FAILURE;
    }
    currentFuncs_ = funcs;
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdGetFunction(void)
{
    return currentFuncs_;
}

int32_t UsbdFunction::UsbdUpdateFunction(uint32_t funcs)
{
    if ((funcs | USB_FUNCTION_SUPPORT) != USB_FUNCTION_SUPPORT && funcs != (USB_FUNCTION_HDC + USB_FUNCTION_RNDIS) &&
        funcs != (USB_FUNCTION_HDC + USB_FUNCTION_STORAGE)) {
        HDF_LOGE("%{public}s: funcs invalid funcs is: %{public}d", __func__, funcs);
        return HDF_FAILURE;
    }
    currentFuncs_ = funcs;
    if (funcs == USB_FUNCTION_STORAGE) {
        if (!CreateMtpDeviceByHcs()) {
            HDF_LOGE("%{public}s, CreateMtpDeviceByHcs failed", __func__);
            return HDF_FAILURE;
        }
        if (SetParameter(SYS_USB_MTP_READY, SYS_USB_MTP_HAS_READY) != HDF_SUCCESS) {
            HDF_LOGE("%{public}s, add mtp config failed", __func__);
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

int32_t UsbdFunction::UsbdRegisterDevice(const std::string &serviceName)
{
    int32_t ret;
    OHOS::sptr<IDeviceManager> devMgr = IDeviceManager::Get();
    if (devMgr == nullptr) {
        HDF_LOGE("%{public}s: get IDeviceManager failed", __func__);
        return HDF_FAILURE;
    }
    ret = devMgr->LoadDevice(serviceName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s, load %{public}s failed", __func__, serviceName.c_str());
        return ret;
    }
    return ret;
}

void UsbdFunction::UsbdUnregisterDevice(const std::string &serviceName)
{
    int32_t ret;
    OHOS::sptr<IDeviceManager> devMgr = IDeviceManager::Get();
    if (devMgr == nullptr) {
        HDF_LOGE("%{public}s: get devMgr object failed", __func__);
        return;
    }
    ret = devMgr->UnloadDevice(serviceName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGW("%{public}s, %{public}s unload  failed", __func__, serviceName.c_str());
    }
}
} // namespace V1_0
} // namespace Usb
} // namespace HDI
} // namespace OHOS
