/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "usbfn_io_mgr.h"
#include "usbd_wrapper.h"

#define HDF_LOG_TAG usbfn_io_mgr

static int32_t ReqToIoData(struct UsbFnRequest *req, struct IoData *ioData, uint32_t aio, uint32_t timeout)
{
    if (req == NULL || ioData == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    struct ReqList *reqList = (struct ReqList *)req;
    ioData->aio = aio;
    if (req->type == USB_REQUEST_TYPE_PIPE_WRITE) {
        ioData->read = 0;
    } else if (req->type == USB_REQUEST_TYPE_PIPE_READ) {
        ioData->read = 1;
    }
    ioData->buf = reqList->buf;
    ioData->len = req->length;
    ioData->timeout = timeout;

    return 0;
}

int32_t OpenEp0AndMapAddr(struct UsbFnFuncMgr *funcMgr)
{
    int32_t ret;
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    funcMgr->fd = fnOps->openPipe(funcMgr->name, 0);
    if (funcMgr->fd <= 0) {
        HDF_LOGE("%{public}s:%{public}d openPipe failed", __func__, __LINE__);
        return HDF_ERR_IO;
    }

    ret = fnOps->queueInit(funcMgr->fd);
    if (ret) {
        HDF_LOGE("%{public}s:%{public}d queueInit failed", __func__, __LINE__);
        return HDF_ERR_IO;
    }
    return 0;
}

static UsbFnRequestType GetReqType(struct UsbHandleMgr *handle, uint8_t pipe)
{
    struct UsbFnPipeInfo info = {0};
    UsbFnRequestType type = USB_REQUEST_TYPE_INVALID;
    if (pipe > 0) {
        int32_t ret = UsbFnIoMgrInterfaceGetPipeInfo(&(handle->intfMgr->interface), pipe - 1, &info);
        if (ret) {
            HDF_LOGE("%{public}s:%{public}d UsbFnMgrInterfaceGetPipeInfo err", __func__, __LINE__);
            type = USB_REQUEST_TYPE_INVALID;
        }
        if (info.dir == USB_PIPE_DIRECTION_IN) {
            type = USB_REQUEST_TYPE_PIPE_WRITE;
        } else if (info.dir == USB_PIPE_DIRECTION_OUT) {
            type = USB_REQUEST_TYPE_PIPE_READ;
        }
    }
    return type;
}

struct UsbFnRequest *UsbFnIoMgrRequestAlloc(struct UsbHandleMgr *handle, uint8_t pipe, uint32_t len)
{
    int32_t ep;
    struct UsbFnInterfaceMgr *intfMgr = handle->intfMgr;
    struct UsbFnFuncMgr *funcMgr = intfMgr->funcMgr;
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    if (pipe == 0) {
        if (funcMgr->fd <= 0) {
            int32_t ret = OpenEp0AndMapAddr(funcMgr);
            if (ret) {
                return NULL;
            }
        }
        ep = funcMgr->fd;
    } else if (pipe <= MAX_EP) {
        ep = handle->fds[pipe - 1];
    } else {
        return NULL;
    }
    uint8_t *mapAddr = fnOps->mapAddr(ep, len);
    if (mapAddr == NULL || mapAddr == (uint8_t *)-1) {
        HDF_LOGE("%{public}s:%{public}d mapAddr %{public}d failed", __func__, __LINE__, ep);
        return NULL;
    }

    struct ReqList *reqList = UsbFnMemCalloc(sizeof(struct ReqList));
    if (reqList == NULL) {
        int32_t ret = fnOps->unmapAddr(mapAddr, len);
        HDF_LOGE("%{public}s:%{public}d UsbFnMemCalloc err, unmap:%{public}d", __func__, __LINE__, ret);
        return NULL;
    }
    struct UsbFnRequest *req = &reqList->req;

    if (pipe == 0) {
        DListInsertTail(&reqList->entry, &funcMgr->reqEntry);
    } else {
        DListInsertTail(&reqList->entry, &handle->reqEntry);
    }
    reqList->handle = handle;
    reqList->fd = ep;
    reqList->buf = (uintptr_t)mapAddr;
    reqList->pipe = pipe;
    reqList->bufLen = len;
    req->length = len;
    req->obj = handle->intfMgr->interface.object;
    req->buf = mapAddr;
    req->type = GetReqType(handle, pipe);

    return req;
}

int32_t UsbFnIoMgrRequestFree(struct UsbFnRequest *req)
{
    struct GenericMemory mem;
    int32_t ret;
    if (req == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct ReqList *reqList = (struct ReqList *)req;
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();

    ret = fnOps->unmapAddr(req->buf, reqList->bufLen);
    if (ret) {
        HDF_LOGE("%{public}s:%{public}d ummapAddr failed, ret=%{public}d ", __func__, __LINE__, ret);
        return HDF_ERR_DEVICE_BUSY;
    }
    mem.size = reqList->bufLen;
    mem.buf = (uint64_t)req->buf;
    ret = fnOps->releaseBuf(reqList->fd, &mem);
    if (ret) {
        HDF_LOGE("%{public}s releaseBuf err::%{public}d", __func__, ret);
        return HDF_ERR_INVALID_PARAM;
    }

    if (reqList->entry.prev != NULL && reqList->entry.next != NULL) {
        DListRemove(&reqList->entry);
    } else {
        HDF_LOGE("%{public}s: The node prev or next is NULL", __func__);
    }
    UsbFnMemFree(reqList);
    return 0;
}

int32_t UsbFnIoMgrRequestSubmitAsync(struct UsbFnRequest *req)
{
    int32_t ret;
    struct IoData ioData = {0};
    struct ReqList *reqList = NULL;
    if (req == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    reqList = (struct ReqList *)req;
    if (ReqToIoData(req, &ioData, 1, 0)) {
        return HDF_ERR_IO;
    }

    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    ret = fnOps->pipeIo(reqList->fd, &ioData);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s pipeIo failed fd:%{public}d read:%{public}u", __func__, reqList->fd, ioData.read);
    }

    return ret;
}

int32_t UsbFnIoMgrRequestCancel(struct UsbFnRequest *req)
{
    int32_t ret;
    struct IoData ioData = {0};
    struct ReqList *reqList = NULL;
    if (req == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    reqList = (struct ReqList *)req;
    if (ReqToIoData(req, &ioData, 1, 0)) {
        return HDF_ERR_IO;
    }
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    ret = fnOps->cancelIo(reqList->fd, &ioData);

    return ret;
}

int32_t UsbFnIoMgrRequestGetStatus(struct UsbFnRequest *req, UsbRequestStatus *status)
{
    struct IoData ioData = {0};
    struct ReqList *reqList;
    if (req == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    reqList = (struct ReqList *)req;
    if (ReqToIoData(req, &ioData, 1, 0)) {
        return HDF_ERR_IO;
    }
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    *status = -(fnOps->getReqStatus(reqList->fd, &ioData));

    return 0;
}

int32_t UsbFnIoMgrRequestSubmitSync(struct UsbFnRequest *req, uint32_t timeout)
{
    int32_t ret;
    struct IoData ioData = {0};
    struct ReqList *reqList;

    if (req == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    reqList = (struct ReqList *)req;
    if (ReqToIoData(req, &ioData, 0, timeout)) {
        return HDF_ERR_IO;
    }
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    ret = fnOps->pipeIo(reqList->fd, &ioData);
    if (ret > 0) {
        req->status = USB_REQUEST_COMPLETED;
        req->actual = (uint32_t)ret;
        return 0;
    }

    return ret;
}

static int32_t HandleInit(struct UsbHandleMgr *handle, struct UsbFnInterfaceMgr *interfaceMgr)
{
    int32_t ret;
    uint32_t i, j;
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();

    DListHeadInit(&handle->reqEntry);
    handle->numFd = interfaceMgr->interface.info.numPipes;
    for (i = 0; i < handle->numFd; i++) {
        handle->fds[i] = fnOps->openPipe(interfaceMgr->funcMgr->name, interfaceMgr->startEpId + i);
        if (handle->fds[i] <= 0) {
            return HDF_ERR_IO;
        }

        ret = fnOps->queueInit(handle->fds[i]);
        if (ret) {
            HDF_LOGE("%{public}s: queueInit failed ret = %{public}d", __func__, ret);
            return HDF_ERR_IO;
        }

        handle->reqEvent[i] = UsbFnMemCalloc(sizeof(struct UsbFnReqEvent) * MAX_REQUEST);
        if (handle->reqEvent[i] == NULL) {
            HDF_LOGE("%{public}s: UsbFnMemCalloc failed", __func__);
            goto FREE_EVENT;
        }
    }
    handle->intfMgr = interfaceMgr;
    return 0;

FREE_EVENT:
    for (j = 0; j < i; j++) {
        UsbFnMemFree(handle->reqEvent[j]);
    }
    return HDF_ERR_IO;
}

struct UsbHandleMgr *UsbFnIoMgrInterfaceOpen(struct UsbFnInterface *interface)
{
    int32_t ret;
    if (interface == NULL) {
        return NULL;
    }
    struct UsbFnInterfaceMgr *interfaceMgr = (struct UsbFnInterfaceMgr *)interface;
    if (interfaceMgr->isOpen) {
        HDF_LOGE("%{public}s: interface has opened", __func__);
        return NULL;
    }
    struct UsbHandleMgr *handle = UsbFnMemCalloc(sizeof(struct UsbHandleMgr));
    if (handle == NULL) {
        HDF_LOGE("%{public}s: malloc UsbHandleMgr failed", __func__);
        return NULL;
    }

    ret = HandleInit(handle, interfaceMgr);
    if (ret) {
        HDF_LOGE("%{public}s: HandleInit failed", __func__);
        UsbFnMemFree(handle);
        return NULL;
    }

    interfaceMgr->isOpen = true;
    interfaceMgr->handle = handle;
    return handle;
}

int32_t UsbFnIoMgrInterfaceClose(struct UsbHandleMgr *handle)
{
    if (handle == NULL) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    struct UsbFnInterfaceMgr *interfaceMgr = handle->intfMgr;
    if (interfaceMgr == NULL || interfaceMgr->isOpen == false) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    for (uint32_t i = 0; i < handle->numFd; i++) {
        int32_t ret = fnOps->queueDel(handle->fds[i]);
        if (ret) {
            HDF_LOGE("%{public}s:%{public}d queueDel failed, ret=%{public}d ", __func__, __LINE__, ret);
            return HDF_ERR_DEVICE_BUSY;
        }

        ret = fnOps->closePipe(handle->fds[i]);
        if (ret) {
            HDF_LOGE("%{public}s:%{public}d closePipe failed, ret=%{public}d ", __func__, __LINE__, ret);
            return HDF_ERR_DEVICE_BUSY;
        }
        handle->fds[i] = -1;
        UsbFnMemFree(handle->reqEvent[i]);
        handle->reqEvent[i] = NULL;
    }

    UsbFnMemFree(handle);
    handle = NULL;
    interfaceMgr->isOpen = false;
    interfaceMgr->handle = NULL;
    return 0;
}

int32_t UsbFnIoMgrInterfaceGetPipeInfo(struct UsbFnInterface *interface, uint8_t pipeId, struct UsbFnPipeInfo *info)
{
    int32_t ret;
    int32_t fd;
    if (info == NULL || interface == NULL || pipeId >= interface->info.numPipes) {
        HDF_LOGE("%{public}s invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    struct UsbFnAdapterOps *fnOps = UsbFnAdapterGetOps();
    struct UsbFnInterfaceMgr *interfaceMgr = (struct UsbFnInterfaceMgr *)interface;
    if (interfaceMgr->isOpen) {
        if (pipeId >= MAX_EP) {
            HDF_LOGE("%{public}s pipeId overflow", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        fd = interfaceMgr->handle->fds[pipeId];
        ret = fnOps->getPipeInfo(fd, info);
        if (ret) {
            HDF_LOGE("%{public}s: getPipeInfo failed", __func__);
            return HDF_ERR_DEVICE_BUSY;
        }
    } else {
        fd = fnOps->openPipe(interfaceMgr->funcMgr->name, interfaceMgr->startEpId + pipeId);
        if (fd <= 0) {
            HDF_LOGE("%{public}s: openPipe failed", __func__);
            return HDF_ERR_IO;
        }
        ret = fnOps->getPipeInfo(fd, info);
        if (ret) {
            fnOps->closePipe(fd);
            HDF_LOGE("%{public}s: getPipeInfo failed", __func__);
            return HDF_ERR_DEVICE_BUSY;
        }
        fnOps->closePipe(fd);
    }

    info->id = pipeId;
    return ret;
}
