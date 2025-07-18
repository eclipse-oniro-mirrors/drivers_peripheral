/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "usb_raw_api_library.h"
#include "linux_adapter.h"
#include "usbd_wrapper.h"
#include "usb_interface_pool.h"

#define HDF_LOG_TAG USB_RAW_API_LIBRARY

struct UsbSession *g_usbRawDefaultSession = NULL;

static void SyncRequestCallback(const void *requestArg)
{
    struct UsbHostRequest *request = (struct UsbHostRequest *)requestArg;
    if (request == NULL || request->userData == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param requestArg.", __func__, __LINE__);
        return;
    }

    int32_t *completed = request->userData;
    *completed = 1;
    OsalSemPost(&request->sem);
}

static inline unsigned char *ControlRequestGetData(const struct UsbHostRequest *request)
{
    return request->buffer + USB_RAW_CONTROL_SETUP_SIZE;
}

static int32_t HandleSyncRequestCompletion(const struct UsbHostRequest *request, struct UsbRequestData *requestData)
{
    int32_t ret;
    uint32_t waitTime;

    if (request->timeout == USB_RAW_REQUEST_TIME_ZERO_MS) {
        waitTime = HDF_WAIT_FOREVER;
    } else {
        waitTime = request->timeout;
    }

    ret = OsalSemWait((struct OsalSem *)&request->sem, waitTime);
    if (ret == HDF_ERR_TIMEOUT) {
        RawCancelRequest(request);
        RawHandleRequestCompletion((struct UsbHostRequest *)request, USB_REQUEST_TIMEOUT);
    } else if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d OsalSemWait failed, ret = %{public}d ", __func__, __LINE__, ret);
        goto OUT;
    }

    if (requestData->requested) {
        *(requestData->requested) = request->actualLength;
    }

    switch (request->status) {
        case USB_REQUEST_COMPLETED:
            ret = HDF_SUCCESS;
            break;
        case USB_REQUEST_TIMEOUT:
            ret = HDF_ERR_TIMEOUT;
            break;
        case USB_REQUEST_NO_DEVICE:
            ret = HDF_DEV_ERR_NO_DEVICE;
            break;
        case USB_REQUEST_STALL:
        case USB_REQUEST_OVERFLOW:
        case USB_REQUEST_ERROR:
        case USB_REQUEST_CANCELLED:
            ret = HDF_ERR_IO;
            break;
        default:
            HDF_LOGW("%{public}s: unrecognised status code %{public}d", __func__, request->status);
            ret = HDF_FAILURE;
            break;
    }

OUT:
    OsalSemDestroy((struct OsalSem *)&request->sem);
    return ret;
}

static int32_t HandleSyncRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbRequestData *requestData, unsigned char type)
{
    int32_t ret;
    static int32_t completed = 0;

    if (UsbEndpointDirOut(requestData->endPoint)) {
        ret = memcpy_s(request->buffer, request->bufLen, requestData->data, requestData->length);
        if (ret != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s fail!", __func__, __LINE__);
            return ret;
        }
    }

    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = requestData->endPoint;
    request->requestType = type;
    request->timeout = requestData->timeout;
    request->length = requestData->length;
    request->userData = &completed;
    request->callback = SyncRequestCallback;
    request->userCallback = NULL;

    ret = OsalSemInit(&request->sem, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d OsalSemInit failed, ret=%{public}d ", __func__, __LINE__, ret);
        return ret;
    }

    ret = RawSubmitRequest(request);
    if (ret < 0) {
        OsalSemDestroy(&request->sem);
        return ret;
    }

    return HandleSyncRequestCompletion(request, (struct UsbRequestData *)requestData);
}

static void GetInterfaceNumberDes(
    const struct UsbDescriptorHeader *header, uint8_t nIntf[], uint8_t nAlts[], int32_t *num)
{
    uint8_t inum;
    int32_t i;
    struct UsbInterfaceDescriptor *desc = NULL;

    desc = (struct UsbInterfaceDescriptor *)header;
    if (desc->bLength < USB_DDK_DT_INTERFACE_SIZE) {
        HDF_LOGW("%{public}s: invalid interface descriptor length %{public}d, skipping", __func__, desc->bLength);
        return;
    }

    inum = desc->bInterfaceNumber;
    for (i = 0; i < *num; ++i) {
        if (nIntf[i] == inum) {
            break;
        }
    }
    if (i < *num) {
        if (nAlts[i] < USB_MAXALTSETTING) {
            ++nAlts[i];
        }
    } else if (*num < USB_MAXINTERFACES) {
        nIntf[*num] = inum;
        nAlts[*num] = 1;
        ++*num;
    }
}

static int32_t GetInterfaceNumber(const uint8_t *buffer, size_t size, uint8_t nIntf[], uint8_t nAlts[])
{
    struct UsbDescriptorHeader *header = NULL;
    const uint8_t *buffer2;
    size_t size2;
    int32_t num = 0;

    for ((buffer2 = buffer, size2 = size); size2 > 0; (buffer2 += header->bLength, size2 -= header->bLength)) {
        if (size2 < sizeof(struct UsbDescriptorHeader)) {
            HDF_LOGW("%{public}s: descriptor has %{public}zu excess bytes", __func__, size2);
            break;
        }
        header = (struct UsbDescriptorHeader *)buffer2;
        if ((header->bLength > size2) || (header->bLength < sizeof(struct UsbDescriptorHeader))) {
            HDF_LOGW("%{public}s: invalid descriptor length %{public}hhu, skipping remainder",
                __func__, header->bLength);
            break;
        }

        if (header->bDescriptorType == USB_DDK_DT_INTERFACE) {
            GetInterfaceNumberDes(header, nIntf, nAlts, &num);
        }
    }

    return num;
}

static int32_t FindNextDescriptor(const uint8_t *buffer, size_t size)
{
    struct UsbDescriptorHeader *h = NULL;
    const uint8_t *buffer0 = buffer;

    while (size > 0) {
        h = (struct UsbDescriptorHeader *)buffer;
        if (h->bDescriptorType == USB_DDK_DT_INTERFACE || h->bDescriptorType == USB_DDK_DT_ENDPOINT) {
            break;
        }
        if (h->bLength <= 0) {
            break;
        }
        buffer += h->bLength;
        size -= h->bLength;
    }

    return buffer - buffer0;
}

int32_t GetDeviceFd(struct UsbDevice *dev, mode_t mode)
{
    if (dev == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (!osAdapterOps || !osAdapterOps->getDeviceFd) {
        HDF_LOGE("%{public}s: not supported", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->getDeviceFd(dev, mode);
}

static int32_t GetConfigDescriptor(const struct UsbDevice *dev, uint8_t configIdx, uint8_t *buffer, size_t size)
{
    int32_t ret;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (dev == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!osAdapterOps->getConfigDescriptor) {
        HDF_LOGE("%{public}s: getConfigDescriptor is null", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = osAdapterOps->getConfigDescriptor(dev, configIdx, buffer, size);
    if (ret < 0) {
        HDF_LOGE("%{public}s: getConfigDescriptor error = %{public}d", __func__, ret);
        return ret;
    }

    if (ret < USB_DDK_DT_CONFIG_SIZE) {
        HDF_LOGE("%{public}s: short config descriptor read error = %{public}d", __func__, ret);
        return HDF_ERR_IO;
    } else if (ret != (int)size) {
        HDF_LOGE("%{public}s: short config descriptor read size = %{public}zu, ret = %{public}d", __func__, size, ret);
    }

    return ret;
}

int32_t GetRawConfigDescriptor(
    const UsbRawHandle *rawHandle, uint8_t configIndex, uint8_t *configDesc, uint32_t configDescLen)
{
    if (rawHandle == NULL || configDesc == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct UsbDeviceHandle *devHandle = (struct UsbDeviceHandle *)rawHandle;
    return GetConfigDescriptor(devHandle->dev, configIndex, configDesc, configDescLen);
}

static void ParseDescriptor(const void *source, enum UsbRawDescriptorType bDescriptorType, void *dest)
{
    int32_t ret;

    if (source == NULL || dest == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return;
    }

    switch (bDescriptorType) {
        case USB_RAW_CONFIG_DESCRIPTOR_TYPE: {
            struct UsbConfigDescriptor *desc = (struct UsbConfigDescriptor *)dest;
            ret = memcpy_s(dest, sizeof(struct UsbConfigDescriptor), source, USB_DDK_DT_CONFIG_SIZE);
            if (ret != EOK) {
                HDF_LOGE("%{public}s:%{public}d memcpy_s failed, ret = %{public}d", __func__, __LINE__, ret);
                break;
            }
            desc->wTotalLength = LE16_TO_CPU(desc->wTotalLength);
            break;
        }
        case USB_RAW_INTERFACE_DESCRIPTOR_TYPE: {
            ret = memcpy_s(dest, sizeof(struct UsbInterfaceDescriptor), source, USB_DDK_DT_INTERFACE_SIZE);
            if (ret != EOK) {
                HDF_LOGE("%{public}s:%{public}d memcpy_s failed, ret = %{public}d", __func__, __LINE__, ret);
            }
            break;
        }
        case USB_RAW_ENDPOINT_DESCRIPTOR_TYPE: {
            struct UsbEndpointDescriptor *desc = (struct UsbEndpointDescriptor *)dest;
            ret = memcpy_s(dest, sizeof(struct UsbEndpointDescriptor), source, USB_DDK_DT_ENDPOINT_SIZE);
            if (ret != EOK) {
                HDF_LOGE("%{public}s:%{public}d memcpy_s failed, ret = %{public}d", __func__, __LINE__, ret);
                break;
            }
            desc->wMaxPacketSize = LE16_TO_CPU(desc->wMaxPacketSize);
            break;
        }
        case USB_RAW_AUDIO_ENDPOINT_DESCRIPTOR_TYPE: {
            struct UsbEndpointDescriptor *desc = (struct UsbEndpointDescriptor *)dest;
            ret = memcpy_s(dest, sizeof(struct UsbEndpointDescriptor), source, USB_DDK_DT_ENDPOINT_AUDIO_SIZE);
            if (ret != EOK) {
                HDF_LOGE("%{public}s:%{public}d memcpy_s failed, ret = %{public}d", __func__, __LINE__, ret);
                break;
            }
            desc->wMaxPacketSize = LE16_TO_CPU(desc->wMaxPacketSize);
            break;
        }
        default:
            HDF_LOGE("%{public}s: error bDescriptorType = %{public}d", __func__, bDescriptorType);
            break;
    }
}

static void ClearEndpoint(struct UsbRawEndpointDescriptor *endPoint)
{
    if ((endPoint != NULL) && (endPoint->extra != NULL)) {
        RawUsbMemFree((void *)endPoint->extra);
        endPoint->extra = NULL;
    }
}

static int32_t ParseEndpoint(struct UsbRawEndpointDescriptor *endPoint, const uint8_t *buffer, int32_t size)
{
    const uint8_t *buffer0 = buffer;
    const struct UsbDescriptorHeader *header = NULL;
    void *extra = NULL;
    int32_t len;
    int32_t ret;

    if (size < DESC_HEADER_LENGTH) {
        HDF_LOGE("%{public}s:size = %{public}d is short endPoint descriptor ", __func__, size);
        return HDF_ERR_IO;
    }

    header = (const struct UsbDescriptorHeader *)buffer;
    if ((header->bDescriptorType != USB_DDK_DT_ENDPOINT) || (header->bLength > size)) {
        HDF_LOGE("%{public}s:%{public}d unexpected descriptor, type = 0x%{public}x, length = %{public}hhu",
            __func__, __LINE__, header->bDescriptorType, header->bLength);
        return buffer - buffer0;
    } else if (header->bLength < USB_DDK_DT_ENDPOINT_SIZE) {
        HDF_LOGE("%{public}s:%{public}d invalid endpoint length = %{public}hhu", __func__, __LINE__, header->bLength);
        return HDF_ERR_IO;
    }

    if (header->bLength >= USB_DDK_DT_ENDPOINT_AUDIO_SIZE) {
        ParseDescriptor(buffer, USB_RAW_AUDIO_ENDPOINT_DESCRIPTOR_TYPE, endPoint);
    } else {
        ParseDescriptor(buffer, USB_RAW_ENDPOINT_DESCRIPTOR_TYPE, endPoint);
    }

    buffer += header->bLength;
    size -= header->bLength;

    len = FindNextDescriptor(buffer, size);
    if (!len) {
        return buffer - buffer0;
    }

    extra = RawUsbMemAlloc((size_t)len);
    if (extra == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = memcpy_s(extra, len + endPoint->extraLength, buffer, len);
    if (ret != EOK) {
        HDF_LOGE("%{public}s:%{public}d memcpy_s failed!", __func__, __LINE__);
        RawUsbMemFree(extra);
        return HDF_ERR_IO;
    }
    endPoint->extra = extra;
    endPoint->extraLength = len;

    return buffer + len - buffer0;
}

static void ClearInterface(const struct UsbRawInterface *usbInterface)
{
    struct UsbRawInterfaceDescriptor *infPtr = NULL;
    uint8_t i;
    uint8_t j;

    if (usbInterface == NULL) {
        HDF_LOGE("%{public}s:%{public}d usbInterface is null", __func__, __LINE__);
        return;
    }

    if (usbInterface->numAltsetting > USB_MAXALTSETTING) {
        HDF_LOGE("%{public}s:%{public}d numAltsetting = %{public}hhu is error",
            __func__, __LINE__, usbInterface->numAltsetting);
        return;
    }

    for (i = 0; i < usbInterface->numAltsetting; i++) {
        infPtr = (struct UsbRawInterfaceDescriptor *)(usbInterface->altsetting + i);
        if (infPtr == NULL) {
            HDF_LOGE("%{public}s:%{public}d altsetting is null", __func__, __LINE__);
            continue;
        }

        if (infPtr->extra != NULL) {
            RawUsbMemFree((void *)infPtr->extra);
            infPtr->extra = NULL;
        }

        if (infPtr->endPoint != NULL) {
            for (j = 0; j < infPtr->interfaceDescriptor.bNumEndpoints; j++) {
                ClearEndpoint((struct UsbRawEndpointDescriptor *)infPtr->endPoint + j);
            }

            RawUsbMemFree((void *)infPtr->endPoint);
            infPtr->endPoint = NULL;
        }
    }

    RawUsbMemFree((void *)usbInterface);
}

static int32_t RawParseDescriptor(int32_t size, const uint8_t *buffer, enum UsbRawDescriptorType bDescriptorType,
    const struct UsbRawInterfaceDescriptor *ifp)
{
    int32_t ret = HDF_SUCCESS;

    ParseDescriptor(buffer, bDescriptorType, (void *)ifp);
    if ((ifp->interfaceDescriptor.bDescriptorType != USB_DDK_DT_INTERFACE) ||
        (ifp->interfaceDescriptor.bLength > size)) {
        HDF_LOGE("%{public}s: unexpected descriptor: type = 0x%{public}x, size = %{public}d", __func__,
            ifp->interfaceDescriptor.bDescriptorType, size);
        ret = HDF_FAILURE;
    } else if ((ifp->interfaceDescriptor.bLength < USB_DDK_DT_INTERFACE_SIZE) ||
        (ifp->interfaceDescriptor.bNumEndpoints > USB_MAXENDPOINTS)) {
        HDF_LOGE("%{public}s: invalid descriptor: length = %{public}u, numEndpoints = %{public}u ", __func__,
            ifp->interfaceDescriptor.bLength, ifp->interfaceDescriptor.bNumEndpoints);
        ret = HDF_ERR_IO;
    }

    return ret;
}

static int32_t ParseInterfaceCopy(struct UsbRawInterfaceDescriptor * const ifp, int32_t len, const uint8_t *buffer)
{
    int32_t ret;

    ifp->extra = RawUsbMemAlloc((size_t)len);
    if (!ifp->extra) {
        ret = HDF_ERR_MALLOC_FAIL;
        return ret;
    }

    ret = memcpy_s((void *)ifp->extra, len + ifp->extraLength, buffer, len);
    if (ret != EOK) {
        HDF_LOGE("%{public}s:%{public}d memcpy_s failed, ret = %{public}d", __func__, __LINE__, ret);
        RawUsbMemFree((void *)ifp->extra);
        ifp->extra = NULL;
        return ret;
    }
    ifp->extraLength = len;
    return ret;
}

static int32_t ParseInterfaceEndpoint(struct UsbRawInterfaceDescriptor *ifp, const uint8_t **buffer, int32_t *size)
{
    struct UsbRawEndpointDescriptor *endPoint = NULL;
    int32_t ret = HDF_SUCCESS;

    if (ifp->interfaceDescriptor.bNumEndpoints > 0) {
        endPoint = RawUsbMemCalloc(ifp->interfaceDescriptor.bNumEndpoints * sizeof(*endPoint));
        if (endPoint == NULL) {
            ret = HDF_ERR_MALLOC_FAIL;
            return ret;
        }

        ifp->endPoint = endPoint;
        for (uint8_t i = 0; i < ifp->interfaceDescriptor.bNumEndpoints; i++) {
            ret = ParseEndpoint(endPoint + i, *buffer, *size);
            if (ret == 0) {
                ifp->interfaceDescriptor.bNumEndpoints = i;
                break;
            } else if (ret < 0) {
                return ret;
            }

            *buffer += ret;
            *size -= ret;
        }
    }
    return ret;
}

static int32_t ParseInterface(struct UsbRawInterface *usbInterface, const uint8_t *buffer, int32_t size)
{
    const uint8_t *buffer0 = buffer;
    int32_t interfaceNumber = -1; // initial value of interfaceNumber is -1
    const struct UsbInterfaceDescriptor *ifDesc = NULL;
    struct UsbRawInterfaceDescriptor *ifp = NULL;

    if (usbInterface == NULL || usbInterface->numAltsetting > USB_MAXALTSETTING) {
        HDF_LOGE("%{public}s: usbInterface is null or numAltsetting is invalid", __func__);
        return HDF_DEV_ERR_NORANGE;
    }

    while (size >= USB_DDK_DT_INTERFACE_SIZE) {
        ifp = (struct UsbRawInterfaceDescriptor *)(usbInterface->altsetting + usbInterface->numAltsetting);
        int32_t ret = RawParseDescriptor(size, buffer, USB_RAW_INTERFACE_DESCRIPTOR_TYPE, ifp);
        if (ret == HDF_FAILURE) {
            return buffer - buffer0;
        } else if (ret == HDF_ERR_IO) {
            HDF_LOGE("%{public}s: RawParseDescriptor failed", __func__);
            return ret;
        }

        usbInterface->numAltsetting++;
        ifp->extra = NULL;
        ifp->extraLength = 0;
        ifp->endPoint = NULL;
        if (interfaceNumber == -1) {
            interfaceNumber = ifp->interfaceDescriptor.bInterfaceNumber;
        }

        buffer += ifp->interfaceDescriptor.bLength;
        size -= (int)ifp->interfaceDescriptor.bLength;
        int32_t len = FindNextDescriptor(buffer, size);
        if (len != 0) {
            if (ParseInterfaceCopy(ifp, len, buffer) != EOK) {
                HDF_LOGE("%{public}s: ParseInterfaceCopy failed", __func__);
                return HDF_FAILURE;
            }
            buffer += len;
            size -= len;
        }

        ret = ParseInterfaceEndpoint(ifp, &buffer, &size);
        if (ret < HDF_SUCCESS) {
            HDF_LOGE("%{public}s: ParseInterfaceEndpoint, ret less than zero", __func__);
            return ret;
        }

        ifDesc = (const struct UsbInterfaceDescriptor *)buffer;
        bool tempFlag = (size < USB_DDK_DT_INTERFACE_SIZE) || (ifDesc->bDescriptorType != USB_DDK_DT_INTERFACE) ||
            (ifDesc->bInterfaceNumber != interfaceNumber);
        if (tempFlag == true) {
            return buffer - buffer0;
        }
    }

    return buffer - buffer0;
}

static int32_t ParseConfigurationDes(struct UsbRawConfigDescriptor *config, const uint8_t *buffer, int32_t size,
    struct UsbRawInterface *usbInterface, const uint8_t *nIntf)
{
    int32_t ret, len;
    uint8_t i;

    len = FindNextDescriptor(buffer, size);
    if (len != 0) {
        config->extra = RawUsbMemAlloc(len);
        if (config->extra == NULL) {
            ret = HDF_ERR_MALLOC_FAIL;
            RawClearConfiguration(config);
            return ret;
        }

        ret = memcpy_s((void *)config->extra, len + config->extraLength, buffer, len);
        if (ret != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s failed! ret = %{public}d", __func__, __LINE__, ret);
            RawClearConfiguration(config);
            return ret;
        }
        config->extraLength = len;
        buffer += len;
        size -= len;
    }

    while (size > 0) {
        struct UsbInterfaceDescriptor *ifDesc = (struct UsbInterfaceDescriptor *)buffer;
        if (config->configDescriptor.bNumInterfaces >= USB_MAXINTERFACES) {
            HDF_LOGE("%{public}d: bNumInterfaces overlong.", config->configDescriptor.bNumInterfaces);
            RawClearConfiguration(config);
            return HDF_FAILURE;
        }
        for (i = 0; i < config->configDescriptor.bNumInterfaces; ++i) {
            if (nIntf[i] == ifDesc->bInterfaceNumber) {
                usbInterface = (struct UsbRawInterface *)config->interface[i];
                break;
            }
        }
        ret = ParseInterface(usbInterface, buffer, size);
        if (ret < 0) {
            RawClearConfiguration(config);
            return ret;
        }
        buffer += ret;
        size -= ret;
    }

    return size;
}

static int32_t ParseConfiguration(struct UsbRawConfigDescriptor *config, const uint8_t *buffer, int32_t size)
{
    struct UsbRawInterface *usbInterface = NULL;
    uint8_t nIntf[USB_MAXINTERFACES] = {0};
    uint8_t nAlts[USB_MAXINTERFACES] = {0};
    int32_t intfNum;

    if (size < USB_DDK_DT_CONFIG_SIZE || config == NULL) {
        HDF_LOGE("%{public}s:%{public}d size = %{public}d is short, or config is null!", __func__, __LINE__, size);
        return HDF_ERR_IO;
    }

    ParseDescriptor(buffer, USB_RAW_CONFIG_DESCRIPTOR_TYPE, config);
    if ((config->configDescriptor.bDescriptorType != USB_DDK_DT_CONFIG) ||
        (config->configDescriptor.bLength != USB_DDK_DT_CONFIG_SIZE) ||
        (config->configDescriptor.bNumInterfaces > USB_MAXINTERFACES)) {
        HDF_LOGE("%{public}s:%{public}d invalid descriptor: type = 0x%{public}x, length = %{public}u",
            __func__, __LINE__, config->configDescriptor.bDescriptorType, config->configDescriptor.bLength);
        return HDF_ERR_IO;
    }

    intfNum = GetInterfaceNumber(buffer, size, nIntf, nAlts);
    config->configDescriptor.bNumInterfaces = (uint8_t)intfNum;

    for (int32_t i = 0; i < intfNum; ++i) {
        uint8_t j = nAlts[i];
        if (j > USB_MAXALTSETTING) {
            HDF_LOGW("%{public}s: too many alternate settings: %{public}hhu", __func__, j);
            nAlts[i] = USB_MAXALTSETTING;
            j = USB_MAXALTSETTING;
        }
        usbInterface = RawUsbMemCalloc(sizeof(struct UsbRawInterface) + sizeof(struct UsbRawInterfaceDescriptor) * j);
        config->interface[i] = usbInterface;
        if (usbInterface == NULL) {
            return HDF_ERR_MALLOC_FAIL;
        }
    }

    buffer += config->configDescriptor.bLength;
    size -= (int32_t)config->configDescriptor.bLength;

    return ParseConfigurationDes(config, buffer, size, usbInterface, nIntf);
}

static int32_t DescToConfig(const uint8_t *buf, int32_t size, struct UsbRawConfigDescriptor ** const config)
{
    struct UsbRawConfigDescriptor *tmpConfig = RawUsbMemCalloc(sizeof(struct UsbRawConfigDescriptor));
    int32_t ret;

    if (tmpConfig == NULL) {
        HDF_LOGE("%{public}s: RawUsbMemCalloc failed", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = ParseConfiguration(tmpConfig, buf, size);
    if (ret < 0) {
        HDF_LOGE("%{public}s: ParseConfiguration failed with error = %{public}d", __func__, ret);
        RawUsbMemFree(tmpConfig);
        tmpConfig = NULL;
        return ret;
    } else if (ret > 0) {
        HDF_LOGW("%{public}s: still %{public}d bytes of descriptor data left", __func__, ret);
    }

    *config = tmpConfig;

    return ret;
}

static int32_t ControlRequestCompletion(const struct UsbHostRequest *request, struct UsbControlRequestData *requestData)
{
    int32_t ret;
    uint32_t waitTime;

    if (request->timeout == USB_RAW_REQUEST_TIME_ZERO_MS) {
        waitTime = HDF_WAIT_FOREVER;
    } else {
        waitTime = request->timeout;
    }

    ret = OsalSemWait((struct OsalSem *)&request->sem, waitTime);
    if (ret == HDF_ERR_TIMEOUT) {
        RawCancelRequest(request);
        RawHandleRequestCompletion((struct UsbHostRequest *)request, USB_REQUEST_TIMEOUT);
    } else if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d OsalSemWait failed, ret=%{public}d ", __func__, __LINE__, ret);
        goto OUT;
    }

    if ((requestData->requestType & USB_DDK_ENDPOINT_DIR_MASK) == USB_PIPE_DIRECTION_IN) {
        ret = memcpy_s(requestData->data, request->actualLength + requestData->length, ControlRequestGetData(request),
            request->actualLength);
        if (ret != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s failed! ret = %{public}d", __func__, __LINE__, ret);
            goto OUT;
        }
    }

    switch (request->status) {
        case USB_REQUEST_COMPLETED:
            ret = request->actualLength;
            break;
        case USB_REQUEST_TIMEOUT:
            ret = HDF_ERR_TIMEOUT;
            break;
        case USB_REQUEST_NO_DEVICE:
            ret = HDF_DEV_ERR_NO_DEVICE;
            break;
        case USB_REQUEST_STALL:
        case USB_REQUEST_OVERFLOW:
        case USB_REQUEST_ERROR:
        case USB_REQUEST_CANCELLED:
            ret = HDF_ERR_IO;
            break;
        default:
            HDF_LOGW("%{public}s: status = %{public}d is unrecognised", __func__, request->status);
            ret = HDF_FAILURE;
    }

OUT:
    OsalSemDestroy((struct OsalSem *)&request->sem);
    return ret;
}

struct UsbSession *RawGetSession(const struct UsbSession *session)
{
    return (struct UsbSession *)(session ? session : g_usbRawDefaultSession);
}

int32_t RawInit(struct UsbSession **session)
{
    int32_t ret;
    struct UsbSession *tempSession = NULL;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (session == NULL && g_usbRawDefaultSession != NULL) {
        AdapterAtomicInc(&g_usbRawDefaultSession->refCount);
        return HDF_SUCCESS;
    }

    tempSession = (struct UsbSession *)RawUsbMemCalloc(sizeof(*tempSession));
    if (tempSession == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }
    OsalAtomicSet(&tempSession->refCount, 1);
    HdfSListInit(&tempSession->usbDevs);
    DListHeadInit(&tempSession->ifacePoolList);
    OsalMutexInit(&tempSession->lock);
    if (session == NULL && g_usbRawDefaultSession == NULL) {
        g_usbRawDefaultSession = tempSession;
        HDF_LOGI("%{public}s: created default context", __func__);
    }

    if (osAdapterOps->init) {
        ret = osAdapterOps->init(tempSession);
        if (ret < 0) {
            HDF_LOGE("%{public}s: init error, return %{public}d", __func__, ret);
            goto ERR_FREE_SESSION;
        }
    } else {
        ret = HDF_ERR_NOT_SUPPORT;
        goto ERR_FREE_SESSION;
    }

    if (session != NULL) {
        *session = tempSession;
    }

    return HDF_SUCCESS;

ERR_FREE_SESSION:
    if (tempSession == g_usbRawDefaultSession) {
        g_usbRawDefaultSession = NULL;
    }

    RawUsbMemFree(tempSession);
    return ret;
}

int32_t RawExit(const struct UsbSession *session)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    struct UsbSession *realSession = RawGetSession(session);
    if (realSession == NULL || AdapterAtomicDec(&realSession->refCount) > 0) {
        return HDF_SUCCESS;
    }

    if (osAdapterOps->exit) {
        osAdapterOps->exit(realSession);
    }
    if (realSession == g_usbRawDefaultSession) {
        g_usbRawDefaultSession = NULL;
    }

    OsalMutexDestroy(&realSession->lock);
    RawUsbMemFree(realSession);

    return HDF_SUCCESS;
}

struct UsbDeviceHandle *RawOpenDevice(const struct UsbSession *session, uint8_t busNum, uint8_t usbAddr)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    struct UsbSession *realSession = NULL;

    if (osAdapterOps->openDevice == NULL) {
        HDF_LOGE("%{public}s: openDevice is null", __func__);
        return NULL;
    }

    realSession = RawGetSession(session);
    if (realSession == NULL) {
        return NULL;
    }

    return osAdapterOps->openDevice(realSession, busNum, usbAddr);
}

int32_t RawCloseDevice(const struct UsbDeviceHandle *devHandle)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL) {
        HDF_LOGE("%{public}s devHandle is null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (osAdapterOps->closeDevice) {
        osAdapterOps->closeDevice((struct UsbDeviceHandle *)devHandle);
    } else {
        return HDF_ERR_NOT_SUPPORT;
    }

    return HDF_SUCCESS;
}

int32_t RawClaimInterface(struct UsbDeviceHandle *devHandle, int32_t interfaceNumber)
{
    int32_t ret;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL || interfaceNumber < 0 || interfaceNumber >= USB_MAXINTERFACES) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    OsalMutexLock(&devHandle->lock);
    if ((devHandle->claimedInterfaces) & (1U << (uint32_t)interfaceNumber)) {
        ret = HDF_SUCCESS;
        goto OUT;
    }

    if (!osAdapterOps->claimInterface) {
        ret = HDF_ERR_NOT_SUPPORT;
        goto OUT;
    }

    ret = osAdapterOps->claimInterface(devHandle, (unsigned int)interfaceNumber);
    if (ret == HDF_SUCCESS) {
        devHandle->claimedInterfaces |= 1U << (uint32_t)interfaceNumber;
    }

OUT:
    OsalMutexUnlock(&devHandle->lock);

    return ret;
}

int32_t RawClaimInterfaceForce(struct UsbDeviceHandle *devHandle, uint32_t interfaceNumber)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL || interfaceNumber >= USB_MAXINTERFACES || osAdapterOps->claimInterface == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (((devHandle->claimedInterfaces) & (1U << interfaceNumber)) != 0) {
        return HDF_SUCCESS;
    }

    OsalMutexLock(&devHandle->lock);
    int32_t ret = osAdapterOps->detachKernelDriverAndClaim(devHandle, interfaceNumber);
    if (ret == HDF_SUCCESS) {
        devHandle->claimedInterfaces |= 1U << interfaceNumber;
    }
    OsalMutexUnlock(&devHandle->lock);
    return ret;
}

int32_t RawDetachInterface(struct UsbDeviceHandle *devHandle, uint32_t interfaceNumber)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL || interfaceNumber >= USB_MAXINTERFACES || osAdapterOps->claimInterface == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    HDF_LOGI("interfaceNumber = %{public}u", interfaceNumber);
    if (((devHandle->detachedInterfaces) & (1U << interfaceNumber)) != 0) {
        return HDF_SUCCESS;
    }

    OsalMutexLock(&devHandle->lock);
    int32_t ret = osAdapterOps->detachKernelDriver(devHandle, interfaceNumber);
    if (ret >= 0) {
        devHandle->detachedInterfaces |= 1U << interfaceNumber;
        devHandle->attachedInterfaces &= ~(1U << interfaceNumber);
        OsalMutexUnlock(&devHandle->lock);
        return HDF_SUCCESS;
    }
    OsalMutexUnlock(&devHandle->lock);
    return HDF_FAILURE;
}

int32_t RawAttachInterface(struct UsbDeviceHandle *devHandle, uint32_t interfaceNumber)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL || interfaceNumber >= USB_MAXINTERFACES || osAdapterOps->claimInterface == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    HDF_LOGI("interfaceNumber = %{public}u", interfaceNumber);
    if (((devHandle->attachedInterfaces) & (1U << interfaceNumber)) != 0) {
        return HDF_SUCCESS;
    }

    OsalMutexLock(&devHandle->lock);
    int32_t ret = osAdapterOps->attachKernelDriver(devHandle, interfaceNumber);
    if (ret >= 0) {
        devHandle->attachedInterfaces |= 1U << interfaceNumber;
        devHandle->detachedInterfaces &= ~(1U << interfaceNumber);
        OsalMutexUnlock(&devHandle->lock);
        return HDF_SUCCESS;
    }
    OsalMutexUnlock(&devHandle->lock);
    return HDF_FAILURE;
}

struct UsbHostRequest *AllocRequest(const struct UsbDeviceHandle *devHandle, int32_t isoPackets, size_t length)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (osAdapterOps->allocRequest == NULL) {
        return NULL;
    }

    return osAdapterOps->allocRequest(devHandle, isoPackets, length);
}

struct UsbHostRequest *AllocRequestByMmap(const struct UsbDeviceHandle *devHandle, int32_t isoPackets, size_t length)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (osAdapterOps->allocRequestByMmap == NULL) {
        return NULL;
    }

    return osAdapterOps->allocRequestByMmap(devHandle, isoPackets, length);
}

int32_t FreeRequest(const struct UsbHostRequest *request)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (osAdapterOps->freeRequest == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->freeRequest((struct UsbHostRequest *)request);
}

int32_t FreeRequestByMmap(const struct UsbHostRequest *request)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (osAdapterOps->freeRequestByMmap == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->freeRequestByMmap((struct UsbHostRequest *)request);
}

int32_t RawFillBulkRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbFillRequestData *fillRequestData)
{
    if (request == NULL || request->buffer == NULL || devHandle == NULL || fillRequestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (UsbEndpointDirOut(fillRequestData->endPoint)) {
        if (fillRequestData->buffer == NULL) {
            HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
            return HDF_ERR_INVALID_PARAM;
        }
        if (memcpy_s(request->buffer, request->bufLen, fillRequestData->buffer, fillRequestData->length) != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s failed!", __func__, __LINE__);
            return HDF_FAILURE;
        }
    }
    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = fillRequestData->endPoint;
    request->requestType = USB_PIPE_TYPE_BULK;
    request->timeout = fillRequestData->timeout;
    request->length = fillRequestData->length;
    request->userData = fillRequestData->userData;
    request->callback = fillRequestData->callback;
    request->userCallback = fillRequestData->userCallback;

    return HDF_SUCCESS;
}

int32_t RawFillControlSetup(const unsigned char *setup, const struct UsbControlRequestData *requestData)
{
    struct UsbRawControlSetup *setupData = (struct UsbRawControlSetup *)setup;

    if (setup == NULL || requestData == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    setupData->requestType = requestData->requestType;
    setupData->request = requestData->requestCmd;
    setupData->value = CPU_TO_LE16(requestData->value);
    setupData->index = CPU_TO_LE16(requestData->index);
    setupData->length = CPU_TO_LE16(requestData->length);

    return HDF_SUCCESS;
}

int32_t RawFillControlRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbFillRequestData *fillRequestData)
{
    if (request == NULL || devHandle == NULL || fillRequestData == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = fillRequestData->endPoint;
    request->requestType = USB_PIPE_TYPE_CONTROL;
    request->timeout = fillRequestData->timeout;
    request->userData = fillRequestData->userData;
    request->callback = fillRequestData->callback;
    request->userCallback = fillRequestData->userCallback;
    request->length = fillRequestData->length;

    return HDF_SUCCESS;
}

int32_t RawFillInterruptRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbFillRequestData *fillRequestData)
{
    if (request == NULL || devHandle == NULL || fillRequestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d param is null!", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (UsbEndpointDirOut(fillRequestData->endPoint)) {
        if (memcpy_s(request->buffer, request->bufLen, fillRequestData->buffer, fillRequestData->length) != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s failed!", __func__, __LINE__);
            return HDF_ERR_IO;
        }
    }
    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = fillRequestData->endPoint;
    request->requestType = USB_PIPE_TYPE_INTERRUPT;
    request->timeout = fillRequestData->timeout;
    request->length = fillRequestData->length;
    request->userData = fillRequestData->userData;
    request->callback = fillRequestData->callback;
    request->userCallback = fillRequestData->userCallback;

    return HDF_SUCCESS;
}

int32_t RawFillInterruptRequestByMmap(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbFillRequestData *fillRequestData)
{
    if (request == NULL || devHandle == NULL || fillRequestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d param is null!", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = fillRequestData->endPoint;
    request->requestType = USB_PIPE_TYPE_INTERRUPT;
    request->timeout = fillRequestData->timeout;
    request->length = fillRequestData->length;
    request->userData = fillRequestData->userData;
    request->callback = fillRequestData->callback;
    request->userCallback = fillRequestData->userCallback;

    return HDF_SUCCESS;
}

int32_t RawFillIsoRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbFillRequestData *fillRequestData)
{
    if (request == NULL || devHandle == NULL || fillRequestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d param is NULL!", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (UsbEndpointDirOut(fillRequestData->endPoint)) {
        int32_t ret = memcpy_s(request->buffer, request->bufLen, fillRequestData->buffer, fillRequestData->length);
        if (ret != EOK) {
            HDF_LOGE("%{public}s:%{public}d memcpy_s fail!", __func__, __LINE__);
            return ret;
        }
    }
    request->devHandle = (struct UsbDeviceHandle *)devHandle;
    request->endPoint = fillRequestData->endPoint;
    request->requestType = USB_PIPE_TYPE_ISOCHRONOUS;
    request->timeout = fillRequestData->timeout;
    request->length = fillRequestData->length;
    request->numIsoPackets = fillRequestData->numIsoPackets;
    request->userData = fillRequestData->userData;
    request->callback = fillRequestData->callback;
    request->userCallback = fillRequestData->userCallback;

    return HDF_SUCCESS;
}

int32_t RawSendControlRequest(struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbControlRequestData *requestData)
{
    struct UsbFillRequestData fillRequestData = {0};
    unsigned char *setup = NULL;
    int32_t completed = 0;
    int32_t ret;

    if (request == NULL || request->buffer == NULL || devHandle == NULL ||
        requestData == NULL || requestData->data == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (USB_RAW_CONTROL_SETUP_SIZE > (size_t)requestData->length) {
        HDF_LOGE("%{public}s:%{public}d oversize", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    setup = request->buffer;
    RawFillControlSetup(setup, requestData);
    if ((requestData->requestType & USB_DDK_ENDPOINT_DIR_MASK) == USB_PIPE_DIRECTION_OUT) {
        fillRequestData.endPoint = 0;
        fillRequestData.length = requestData->length;
        if (requestData->length > 0) {
            ret = memcpy_s(request->buffer + USB_RAW_CONTROL_SETUP_SIZE, fillRequestData.length, requestData->data,
                requestData->length);
            if (ret != EOK) {
                HDF_LOGE("%{public}s:%{public}d memcpy_s fail, requestData.length=%{public}d",
                    __func__, __LINE__, requestData->length);
                return ret;
            }
        }
        fillRequestData.length = USB_RAW_CONTROL_SETUP_SIZE + requestData->length;
    } else {
        fillRequestData.endPoint = (0x1 << USB_DIR_OFFSET);
    }
    fillRequestData.userCallback = NULL;
    fillRequestData.callback = SyncRequestCallback;
    fillRequestData.userData = &completed;
    fillRequestData.timeout = requestData->timeout;
    RawFillControlRequest(request, devHandle, &fillRequestData);

    ret = OsalSemInit(&request->sem, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d OsalSemInit failed, ret = %{public}d ", __func__, __LINE__, ret);
        return ret;
    }

    ret = RawSubmitRequest(request);
    if (ret < 0) {
        OsalSemDestroy(&request->sem);
        return ret;
    }

    return ControlRequestCompletion(request, (struct UsbControlRequestData *)requestData);
}

int32_t RawSendBulkRequest(const struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbRequestData *requestData)
{
    if (request == NULL || devHandle == NULL || requestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    return HandleSyncRequest((struct UsbHostRequest *)request, devHandle, requestData, USB_PIPE_TYPE_BULK);
}

int32_t RawSendInterruptRequest(const struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbRequestData *requestData)
{
    if (request == NULL || devHandle == NULL || requestData == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    return HandleSyncRequest((struct UsbHostRequest *)request, devHandle, requestData, USB_PIPE_TYPE_INTERRUPT);
}

struct UsbHostRequest *RawAllocRequest(const struct UsbDeviceHandle *devHandle, int32_t isoPackets, int32_t length)
{
    struct UsbHostRequest *request = NULL;
    request = (struct UsbHostRequest *)AllocRequest(devHandle, isoPackets, length);
    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d RawMemAlloc failed", __func__, __LINE__);
        return NULL;
    }
    return request;
}

struct UsbHostRequest *RawAllocRequestByMmap(
    const struct UsbDeviceHandle *devHandle, int32_t isoPackets, int32_t length)
{
    struct UsbHostRequest *request = NULL;
    request = (struct UsbHostRequest *)AllocRequestByMmap(devHandle, isoPackets, length);
    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d RawMemAlloc failed", __func__, __LINE__);
        return NULL;
    }
    return request;
}

int32_t RawFreeRequest(const struct UsbHostRequest *request)
{
    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    return FreeRequest(request);
}

int32_t RawFreeRequestByMmap(const struct UsbHostRequest *request)
{
    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }
    return FreeRequestByMmap(request);
}

int32_t RawGetConfigDescriptor(
    const struct UsbDevice *dev, uint8_t configIndex, struct UsbRawConfigDescriptor ** const config)
{
    int32_t ret;
    union UsbiConfigDescBuf tmpConfig;
    uint16_t configLen;
    uint8_t *buf = NULL;

    if (dev == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    ret = GetConfigDescriptor(dev, configIndex, tmpConfig.buf, sizeof(tmpConfig.buf));
    if (ret < HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d ret=%{public}d", __func__, __LINE__, ret);
        return ret;
    }
    configLen = LE16_TO_CPU(tmpConfig.desc.wTotalLength);
    buf = RawUsbMemAlloc(configLen);
    if (buf == NULL) {
        HDF_LOGE("%{public}s:%{public}d RawUsbMemAlloc failed", __func__, __LINE__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = GetConfigDescriptor(dev, configIndex, buf, configLen);
    if (ret >= HDF_SUCCESS) {
        ret = DescToConfig(buf, ret, config);
    }

    RawUsbMemFree(buf);
    buf = NULL;

    return ret;
}

void RawClearConfiguration(struct UsbRawConfigDescriptor *config)
{
    uint8_t i;

    if (config == NULL) {
        HDF_LOGE("%{public}s:%{public}d config is NULL", __func__, __LINE__);
        return;
    }

    for (i = 0; i < config->configDescriptor.bNumInterfaces; i++) {
        ClearInterface((const struct UsbRawInterface *)(config->interface[i]));
        config->interface[i] = NULL;
    }

    if (config->extra != NULL) {
        RawUsbMemFree((void *)config->extra);
        config->extra = NULL;
    }
}

int32_t RawGetConfiguration(const struct UsbDeviceHandle *devHandle, int32_t *config)
{
    int32_t ret;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    uint8_t tmp = 0;

    if (devHandle == NULL || config == NULL) {
        HDF_LOGE("%{public}s:%{public}d param is null", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!osAdapterOps->getConfiguration) {
        HDF_LOGE("%{public}s:%{public}d adapter don't support getConfiguration", __func__, __LINE__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = osAdapterOps->getConfiguration(devHandle, &tmp);
    *config = tmp;
    return ret;
}

int32_t RawUsbControlMsg(const struct UsbDeviceHandle *devHandle,  struct UsbControlRequestData *ctrlData)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (devHandle == NULL || ctrlData == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!osAdapterOps->usbControlMsg) {
        HDF_LOGE("%{public}s:%{public}d not support control msg operation", __func__, __LINE__);
        return HDF_ERR_NOT_SUPPORT;
    }
    return osAdapterOps->usbControlMsg(devHandle, ctrlData);
}

int32_t RawUsbGetUsbSpeed(const struct UsbDeviceHandle *devHandle)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (devHandle == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!osAdapterOps->getUsbSpeed) {
        HDF_LOGE("%{public}s:%{public}d not support get usb speed operation", __func__, __LINE__);
        return HDF_ERR_NOT_SUPPORT;
    }
    return osAdapterOps->getUsbSpeed(devHandle);
}

int32_t RawSetConfiguration(const struct UsbDeviceHandle *devHandle, int32_t configuration)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (configuration < -1 || configuration > (int)0xFF) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (!osAdapterOps->setConfiguration) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->setConfiguration((struct UsbDeviceHandle *)devHandle, configuration);
}

int32_t RawGetDescriptor(const struct UsbHostRequest *request, const struct UsbDeviceHandle *devHandle,
    const struct UsbRawDescriptorParam *param, const unsigned char *data)
{
    int32_t ret;
    struct UsbControlRequestData requestData;

    if (request == NULL || devHandle == NULL || param == NULL || data == NULL) {
        HDF_LOGE("%{public}s:%{public}d param is NULL!", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    requestData.requestType = USB_PIPE_DIRECTION_IN;
    requestData.requestCmd = USB_REQUEST_GET_DESCRIPTOR;
    requestData.value = (uint16_t)((param->descType << BYTE_LENGTH) | param->descIndex);
    requestData.index = 0;
    requestData.data = (unsigned char *)data;
    requestData.length = (uint16_t)param->length;
    requestData.timeout = USB_RAW_REQUEST_DEFAULT_TIMEOUT;
    ret = RawSendControlRequest((struct UsbHostRequest *)request, devHandle, &requestData);

    return ret;
}

struct UsbDevice *RawGetDevice(const struct UsbDeviceHandle *devHandle)
{
    if (devHandle == NULL) {
        HDF_LOGE("%{public}s: invalid param", __func__);
        return NULL;
    }

    return devHandle->dev;
}

int32_t RawGetDeviceDescriptor(const struct UsbDevice *dev, struct UsbDeviceDescriptor *desc)
{
    if (dev == NULL || desc == NULL || sizeof(dev->deviceDescriptor) != USB_DDK_DT_DEVICE_SIZE) {
        HDF_LOGE("%{public}s: struct UsbDeviceDescriptor is not expected size", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    *desc = dev->deviceDescriptor;
    return HDF_SUCCESS;
}

void RawAttachKernelDriver(struct UsbDeviceHandle *devHandle, uint8_t interfaceNumber)
{
    if (devHandle == NULL || interfaceNumber >= USB_MAXINTERFACES) {
        HDF_LOGE("%{public}s param is NULL or interfaceNumber = %{public}d is out of range", __func__, interfaceNumber);
        return;
    }

    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (osAdapterOps->attachKernelDriver == NULL) {
        HDF_LOGE("%{public}s: releaseInterface not support", __func__);
        return;
    }

    OsalMutexLock(&devHandle->lock);
    osAdapterOps->attachKernelDriver(devHandle, interfaceNumber);
    OsalMutexUnlock(&devHandle->lock);
    return;
}

int32_t RawReleaseInterface(struct UsbDeviceHandle *devHandle, int32_t interfaceNumber)
{
    int32_t ret;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (devHandle == NULL || interfaceNumber < 0 || interfaceNumber >= USB_MAXINTERFACES) {
        HDF_LOGE(
            "%{public}s:%{public}d param is NULL or interfaceNumber = %{public}d is out of range",
            __func__, __LINE__, interfaceNumber);
        return HDF_ERR_INVALID_PARAM;
    }

    OsalMutexLock(&devHandle->lock);
    if (!(devHandle->claimedInterfaces & (1U << (uint32_t)interfaceNumber))) {
        ret = HDF_ERR_BAD_FD;
        goto OUT;
    }

    if (!osAdapterOps->releaseInterface) {
        ret = HDF_ERR_NOT_SUPPORT;
        goto OUT;
    }

    ret = osAdapterOps->releaseInterface(devHandle, (unsigned int)interfaceNumber);
    if (ret == HDF_SUCCESS) {
        devHandle->claimedInterfaces &= ~(1U << (uint32_t)interfaceNumber);
    }

OUT:
    OsalMutexUnlock(&devHandle->lock);

    return ret;
}

int32_t RawResetDevice(const struct UsbDeviceHandle *devHandle)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (!osAdapterOps->resetDevice) {
        return HDF_ERR_NOT_SUPPORT;
    }

    struct UsbDeviceHandle *constDevHandle = (struct UsbDeviceHandle *)devHandle;

    return osAdapterOps->resetDevice(constDevHandle);
}

int32_t RawSubmitRequest(const struct UsbHostRequest *request)
{
    int32_t ret;
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!osAdapterOps->submitRequest) {
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = osAdapterOps->submitRequest((struct UsbHostRequest *)request);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d ret = %{public}d", __func__, __LINE__, ret);
    }

    return ret;
}

int32_t RawCancelRequest(const struct UsbHostRequest *request)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (!osAdapterOps->cancelRequest) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->cancelRequest((struct UsbHostRequest *)request);
}

int32_t RawHandleRequest(const struct UsbDeviceHandle *devHandle)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    int32_t ret;

    if (!osAdapterOps->urbCompleteHandle) {
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = osAdapterOps->urbCompleteHandle(devHandle);
    if (ret < 0) {}

    return ret;
}

int32_t RawClearHalt(const struct UsbDeviceHandle *devHandle, uint8_t pipeAddress)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    unsigned int endPoint = pipeAddress;

    if (osAdapterOps->clearHalt == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->clearHalt(devHandle, endPoint);
}

int32_t RawHandleRequestCompletion(struct UsbHostRequest *request, UsbRequestStatus status)
{
    if (request == NULL) {
        HDF_LOGE("%{public}s:%{public}d request is NULL!", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    request->status = status;
    if (request->callback) {
        request->callback((void *)request);
    }

    return HDF_SUCCESS;
}

int32_t RawSetInterfaceAltsetting(
    const struct UsbDeviceHandle *devHandle, uint8_t interfaceNumber, uint8_t settingIndex)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();

    if (osAdapterOps->setInterfaceAltsetting == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    return osAdapterOps->setInterfaceAltsetting(devHandle, interfaceNumber, settingIndex);
}

UsbRawTidType RawGetTid(void)
{
    return UsbAdapterGetTid();
}

int32_t RawRegisterSignal(void)
{
    return UsbAdapterRegisterSignal();
}

int32_t RawKillSignal(struct UsbDeviceHandle *devHandle, UsbRawTidType tid)
{
    return UsbAdapterKillSignal(devHandle, tid);
}

int32_t RawInitPnpService(enum UsbPnpNotifyServiceCmd cmdType, struct UsbPnpAddRemoveInfo infoData)
{
    if (cmdType != USB_PNP_NOTIFY_ADD_INTERFACE && cmdType != USB_PNP_NOTIFY_REMOVE_INTERFACE) {
        HDF_LOGE("%{public}s:%{public}d invalid param cmdType", __func__, __LINE__);
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t ret;
    struct HdfIoService *serv = HdfIoServiceBind(USB_HOST_PNP_SERVICE_NAME);
    if (serv == NULL || serv->dispatcher == NULL || serv->dispatcher->Dispatch == NULL) {
        ret = HDF_FAILURE;
        HDF_LOGE("%{public}s:%d failed to get service %s", __func__, __LINE__, USB_HOST_PNP_SERVICE_NAME);
        return ret;
    }

    struct HdfSBuf *pnpData = HdfSbufObtainDefaultSize();
    struct HdfSBuf *pnpReply = HdfSbufObtainDefaultSize();
    if (pnpData == NULL || pnpReply == NULL) {
        ret = HDF_FAILURE;
        HDF_LOGE("%{public}s:%{public}d GetService err", __func__, __LINE__);
        goto ERR_SBUF;
    }

    if (!HdfSbufWriteBuffer(pnpData, (const void *)(&infoData), sizeof(struct UsbPnpAddRemoveInfo))) {
        HDF_LOGE("%{public}s:%{public}d sbuf write infoData failed", __func__, __LINE__);
        ret = HDF_FAILURE;
        goto OUT;
    }

    ret = serv->dispatcher->Dispatch(&serv->object, cmdType, pnpData, pnpReply);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s:%{public}d Dispatch USB_PNP_NOTIFY_REMOVE_TEST failed ret = %{public}d",
            __func__, __LINE__, ret);
        goto OUT;
    }

    int32_t replyData = 0;
    if (!HdfSbufReadInt32(pnpReply, &replyData)) {
        HDF_LOGE("%{public}s:HdfSbufReadInt32 failed", __func__);
        ret = HDF_FAILURE;
        goto OUT;
    }
    if (replyData != INT32_MAX) {
        HDF_LOGE("%{public}s:%{public}d cmdType = %{public}d reply failed", __func__, __LINE__, cmdType);
        ret = HDF_FAILURE;
        goto OUT;
    }
    ret = HDF_SUCCESS;
    HDF_LOGI("%{public}s:%{public}d cmdType = %{public}d reply success", __func__, __LINE__, cmdType);

OUT:
    HdfSbufRecycle(pnpData);
    HdfSbufRecycle(pnpReply);
ERR_SBUF:
    HdfIoServiceRecycle(serv);

    return ret;
}

void RawRequestListInit(struct UsbDevice *deviceObj)
{
    if (deviceObj == NULL) {
        HDF_LOGE("%{public}s:%{public}d deviceObj is NULL!", __func__, __LINE__);
        return;
    }

    OsalMutexInit(&deviceObj->requestLock);
    HdfSListInit(&deviceObj->requestList);
}

void *RawUsbMemAlloc(size_t size)
{
    return RawUsbMemCalloc(size);
}

void *RawUsbMemCalloc(size_t size)
{
    if (size == 0) {
        HDF_LOGE("%{public}s:%{public}d size is 0", __func__, __LINE__);
        return NULL;
    }

    void *buf = OsalMemCalloc(size);
    if (buf == NULL) {
        HDF_LOGE("%{public}s: %{public}d, OsalMemCalloc failed", __func__, __LINE__);
        return NULL;
    }
    return buf;
}

void RawUsbMemFree(void *mem)
{
    if (mem == NULL) {
        HDF_LOGE("%{public}s:%{public}d mem is null.", __func__, __LINE__);
        return;
    }

    OsalMemFree(mem);
    mem = NULL;
}

bool RawGetInterfaceActiveStatus(struct UsbDeviceHandle *devHandle, uint32_t interfaceNumber)
{
    struct UsbOsAdapterOps *osAdapterOps = UsbAdapterGetOps();
    if (devHandle == NULL || interfaceNumber >= USB_MAXINTERFACES || osAdapterOps->claimInterface == NULL) {
        HDF_LOGE("%{public}s:%{public}d HDF_ERR_INVALID_PARAM", __func__, __LINE__);
        return false;
    }
    HDF_LOGI("interfaceNumber = %{public}u", interfaceNumber);

    OsalMutexLock(&devHandle->lock);
    bool ret = osAdapterOps->getInterfaceActiveStatus(devHandle, interfaceNumber);
    OsalMutexUnlock(&devHandle->lock);
    return ret;
}

int32_t RawUsbCloseCtlProcess(const UsbInterfaceHandle *interfaceHandle)
{
    return UsbCloseCtlProcess(interfaceHandle);
}
