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

#include <hdf_base.h>
#include <hdf_device_desc.h>
#include <hdf_log.h>
#include <hdf_sbuf_ipc.h>
#include <iproxy_broker.h>
#include <seccomp_policy.h>

#include <cerrno>
#include <mutex>
#include <new>
#include <sys/prctl.h>
#include <thread>

#include "pcie_host_service.h"
#include "v1_0/pcie_interface_stub.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
static constexpr uint32_t LOG_DOMAIN = 0xD002516;

using OHOS::HDI::Pcie::Driver::PcieDriverHostService;
using OHOS::HDI::Pcie::V1_0::IPcieInterface;

namespace {
constexpr const char *SECCOMP_FILTER = "system";
std::once_flag g_seccompOnce;

static bool ApplySeccomp()
{
    static bool seccompSetupSucceeded = true;
    std::call_once(g_seccompOnce, [&] {
        if (!IsEnableSeccomp()) {
            HDF_LOGI("seccomp disabled");
            return;
        }
        // Check if init system already installed seccomp filter for this process
        int seccompMode = prctl(PR_GET_SECCOMP);
        if (seccompMode > 0) {
            HDF_LOGI("seccomp filter already active (mode=%d), skipping", seccompMode);
            return;
        }
        // No CAP_SYS_ADMIN in this host; need NO_NEW_PRIVS to install seccomp
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
            HDF_LOGW("prctl NO_NEW_PRIVS failed: %d, seccomp may fail", errno);
        }
        if (!SetSeccompPolicyWithName(SYSTEM_OTHERS, SECCOMP_FILTER)) {
            HDF_LOGW("seccomp filter failed");
            seccompSetupSucceeded = false;
        }
    });
    return seccompSetupSucceeded;
}
} // namespace

struct HdfPcieDdkHost {
    struct IDeviceIoService ioService;
    OHOS::sptr<OHOS::IRemoteObject> stub;
    std::thread saLoadThread;
};

static int32_t PcieDdkDriverDispatch(
    struct HdfDeviceIoClient *client, int cmdId, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGI("PCIE_LOAD_TRACE DISPATCH_ENTER cmd=%{public}d", cmdId);
    if (!client || !client->device || !client->device->service) {
        HDF_LOGE("PCIE_LOAD_TRACE DISPATCH_INVALID_CLIENT cmd=%{public}d", cmdId);
        return HDF_ERR_INVALID_PARAM;
    }
    auto *host = CONTAINER_OF(client->device->service, struct HdfPcieDdkHost, ioService);
    if (!host || !host->stub) {
        HDF_LOGE("PCIE_LOAD_TRACE DISPATCH_INVALID_STUB cmd=%{public}d", cmdId);
        return HDF_ERR_INVALID_OBJECT;
    }

    OHOS::MessageParcel *dataParcel = nullptr;
    OHOS::MessageParcel *replyParcel = nullptr;
    OHOS::MessageOption option;
    if (SbufToParcel(data, &dataParcel) != HDF_SUCCESS || SbufToParcel(reply, &replyParcel) != HDF_SUCCESS) {
        HDF_LOGE("PCIE_LOAD_TRACE DISPATCH_PARCEL_FAILED cmd=%{public}d", cmdId);
        return HDF_ERR_INVALID_PARAM;
    }
    int32_t dispatchStatus = host->stub->SendRequest(cmdId, *dataParcel, *replyParcel, option);
    HDF_LOGI("PCIE_LOAD_TRACE DISPATCH_REPLY cmd=%{public}d status=%{public}d", cmdId, dispatchStatus);
    return dispatchStatus;
}

static int32_t HdfPcieHostDriverBind(struct HdfDeviceObject *dev)
{
    HDF_LOGI("PCIE_LOAD_TRACE HOST_BIND_ENTER");
    if (!ApplySeccomp()) {
        HDF_LOGE("seccomp filter failed, refusing to bind privileged host");
        return HDF_FAILURE;
    }
    if (!dev) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_BIND_INVALID_DEVICE");
        return HDF_ERR_INVALID_PARAM;
    }
    auto *pcieDdkHost = new (std::nothrow) HdfPcieDdkHost;
    if (!pcieDdkHost) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_BIND_ALLOC_FAILED");
        return HDF_FAILURE;
    }
    pcieDdkHost->ioService.Dispatch = PcieDdkDriverDispatch;
    auto impl = IPcieInterface::Get(true);
    if (!impl) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_BIND_IMPL_FAILED");
        delete pcieDdkHost;
        return HDF_FAILURE;
    }
    pcieDdkHost->stub = OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(
        impl, OHOS::HDI::Pcie::V1_0::IPcieInterface::GetDescriptor());
    if (!pcieDdkHost->stub) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_BIND_STUB_FAILED");
        delete pcieDdkHost;
        return HDF_FAILURE;
    }
    dev->service = &pcieDdkHost->ioService;
    HDF_LOGI("PCIE_LOAD_TRACE HOST_BIND_SUCCESS");
    return HDF_SUCCESS;
}

static int32_t HdfPcieDriverHostDriverInit(struct HdfDeviceObject *dev)
{
    HDF_LOGI("PCIE_LOAD_TRACE HOST_INIT");
    if (!dev || !dev->service) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_INIT_INVALID_DEVICE");
        return HDF_ERR_INVALID_PARAM;
    }
    auto *host = CONTAINER_OF(dev->service, struct HdfPcieDdkHost, ioService);
    host->saLoadThread = std::thread([] {
        PcieDriverHostService svc;
        bool saLoadResult = svc.StartEdmSaForPcieDevices();
        if (!saLoadResult) {
            HDF_LOGI("PCIe boot-scan EDM SA load failed");
        }
    });

    return HDF_SUCCESS;
}

static void HdfPcieDriverHostDriverRelease(struct HdfDeviceObject *dev)
{
    HDF_LOGI("PCIE_LOAD_TRACE HOST_RELEASE");
    if (dev && dev->service) {
        auto *host = CONTAINER_OF(dev->service, struct HdfPcieDdkHost, ioService);
        if (host->saLoadThread.joinable()) {
            host->saLoadThread.join();
        }
        delete host;
        dev->service = nullptr;
    }
}

static struct HdfDriverEntry g_entry = {
    .moduleVersion = 1,
    .moduleName = "pcie_hdf_driver",
    .Bind = HdfPcieHostDriverBind,
    .Init = HdfPcieDriverHostDriverInit,
    .Release = HdfPcieDriverHostDriverRelease,
};
HDF_INIT(g_entry);
