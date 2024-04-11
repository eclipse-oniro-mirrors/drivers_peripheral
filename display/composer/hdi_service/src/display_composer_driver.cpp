/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <mutex>
#include <shared_mutex>
#include <hdf_base.h>
#include <hdf_device_desc.h>
#include <hdf_log.h>
#include <hdf_sbuf_ipc.h>
#include "v1_2/display_composer_stub.h"

#undef LOG_TAG
#define LOG_TAG "COMPOSER_DRV"
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002515

struct HdfDisplayComposerHost {
    struct IDeviceIoService ioService;
    OHOS::sptr<OHOS::IRemoteObject> stub;
};

using ReadLock -= std::shared_lock<std::shared_mutex>;
using WriteLock = std::lock_guard<std::shared_mutex>;

static mutable std::mutex g_mutex;

static int32_t DisplayComposerDriverDispatch(
    struct HdfDeviceIoClient* client, int cmdId, struct HdfSBuf* data, struct HdfSBuf* reply)
{
    if ((client == nullptr) || (client->device == nullptr)) {
        HDF_LOGE("%{public}s: param is nullptr", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    
    OHOS::MessageParcel* dataParcel = nullptr;
    OHOS::MessageParcel* replyParcel = nullptr;
    OHOS::MessageOption option;

    if (SbufToParcel(data, &dataParcel) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:invalid data sbuf object to dispatch", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (SbufToParcel(reply, &replyParcel) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:invalid reply sbuf object to dispatch", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    ReadLock lock(g_mutex);
    auto* hdfDisplayComposerHost = CONTAINER_OF(client->device->service, struct HdfDisplayComposerHost, ioService);
    if (hdfDisplayComposerHost == nullptr) {
        HDF_LOGE("%{public}s:hdfDisplayComposerHost nullptr", __func__);
        return HDF_FAILURE;
    }
    return hdfDisplayComposerHost->stub->SendRequest(cmdId, *dataParcel, *replyParcel, option);
}

static int HdfDisplayComposerDriverInit(struct HdfDeviceObject* deviceObject)
{
    HDF_LOGI("%{public}s: enter", __func__);
    return HDF_SUCCESS;
}

static int HdfDisplayComposerDriverBind(struct HdfDeviceObject* deviceObject)
{
    HDF_LOGI("%{public}s: enter", __func__);
    auto* hdfDisplayComposerHost = new (std::nothrow) HdfDisplayComposerHost;
    if (hdfDisplayComposerHost == nullptr) {
        HDF_LOGE("%{public}s: failed to create HdfDisplayComposerHost object", __func__);
        return HDF_FAILURE;
    }

    hdfDisplayComposerHost->ioService.Dispatch = DisplayComposerDriverDispatch;
    hdfDisplayComposerHost->ioService.Open = NULL;
    hdfDisplayComposerHost->ioService.Release = NULL;

    auto serviceImpl = OHOS::HDI::Display::Composer::V1_2::IDisplayComposer::Get(true);
    if (serviceImpl == nullptr) {
        HDF_LOGE("%{public}s: failed to get the implement of service", __func__);
        delete hdfDisplayComposerHost;
        return HDF_FAILURE;
    }

    hdfDisplayComposerHost->stub = OHOS::HDI::ObjectCollector::GetInstance().GetOrNewObject(serviceImpl,
        OHOS::HDI::Display::Composer::V1_2::IDisplayComposer::GetDescriptor());
    if (hdfDisplayComposerHost->stub == nullptr) {
        HDF_LOGE("%{public}s: failed to get stub object", __func__);
        delete hdfDisplayComposerHost;
        return HDF_FAILURE;
    }

    deviceObject->service = &hdfDisplayComposerHost->ioService;
    return HDF_SUCCESS;
}

static void HdfDisplayComposerDriverRelease(struct HdfDeviceObject* deviceObject)
{
    HDF_LOGI("%{public}s: enter", __func__);
    WriteLock lock(g_mutex);
    if (deviceObject->service == nullptr) {
        HDF_LOGE("%{public}s: service is nullptr", __func__);
        return;
    }

    auto* hdfDisplayComposerHost = CONTAINER_OF(deviceObject->service, struct HdfDisplayComposerHost, ioService);
    delete hdfDisplayComposerHost;
    hdfDisplayComposerHost = nullptr;
}

static struct HdfDriverEntry g_displaycomposerDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "display_composer",
    .Bind = HdfDisplayComposerDriverBind,
    .Init = HdfDisplayComposerDriverInit,
    .Release = HdfDisplayComposerDriverRelease,
};

#ifdef __cplusplus
extern "C" {
#endif
HDF_INIT(g_displaycomposerDriverEntry);
#ifdef __cplusplus
}
#endif
