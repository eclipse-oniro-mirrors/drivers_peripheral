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

#include "adapter_if.h"
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "usb_handle.h"
#include "usbd_wrapper.h"

#define HDF_LOG_TAG adapter_if
#define SLEEP_TIME  100000
#define OPEN_CNT    30

static int32_t UsbFnAdapterOpenFn(void)
{
    int32_t i;
    int32_t ep;
    for (i = 0; i < OPEN_CNT; i++) {
        ep = handle_open("/dev/fconfig");
        if (ep > 0) {
            break;
        }
        usleep(SLEEP_TIME);
    }
    if (ep < 0) {
        HDF_LOGE("func not alloc!");
    }
    return ep;
}

static int32_t UsbFnAdapterClosefn(int32_t fd)
{
    if (fd < 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    return handle_close(fd);
}

static int32_t UsbFnAdapterCreateFconfigString(struct FconfigString * const configString, const char *name)
{
    if (configString == NULL || name == NULL) {
        HDF_LOGE("%{public}s: configName is NULL", __func__);
        return HDF_ERR_IO;
    }

    size_t strLen = strlen(name);
    if (strLen >= SIZE_MAX) {
        HDF_LOGE("%{public}s: name length is too long", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    configString->len = (uint32_t)strLen;
    configString->s = UsbFnMemCalloc(strLen + 1);
    if (configString->s == NULL) {
        HDF_LOGE("%{public}s: UsbFnMemCalloc failed!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    int32_t ret = memcpy_s(configString->s, (strLen + 1), name, strLen);
    if (ret != EOK) {
        HDF_LOGE("%{public}s: memcpy_s failed!", __func__);
        UsbFnMemFree(configString->s);
        configString->s = NULL;
        return HDF_ERR_MALLOC_FAIL;
    }

    *(configString->s + configString->len) = '\0';
    return 0;
}

static int32_t UsbFnAdapterWriteGadget(int32_t fd, int32_t cmd, struct FconfigString *gadgetName)
{
    int32_t ret;

    if (gadgetName == NULL) {
        HDF_LOGE("%{public}s: udcName is NULL", __func__);
        return HDF_ERR_IO;
    }
    ret = handle_ioctl(fd, cmd, gadgetName);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
        return HDF_ERR_IO;
    }
    return 0;
}

static int32_t UsbFnAdapterWriteDevDesc(
    int32_t fd, struct FconfigString * const gadgetName, const struct UsbFnDeviceDesc * const descriptor)
{
    struct FconfigDevDesc devDesc;

    if (gadgetName == NULL || descriptor == NULL) {
        HDF_LOGE("%{public}s: udcName is NULL", __func__);
        return HDF_ERR_IO;
    }

    devDesc.gadgetName.len = gadgetName->len;
    devDesc.gadgetName.s = gadgetName->s;
    int32_t ret = memcpy_s(&devDesc.devDesc, sizeof(devDesc.devDesc), descriptor->deviceDesc, sizeof(devDesc.devDesc));
    if (ret != EOK) {
        HDF_LOGE("%{public}s: memcpy_s failed!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = handle_ioctl(fd, FCONFIG_CMD_WRITE_DEV_DESC, &devDesc);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    return 0;
}

static int32_t UsbFnAdapterWriteDevString(
    int32_t fd, struct FconfigDevStrings * const devStrings, const struct UsbFnStrings * const usbFnString)
{
    struct UsbString *usbString = NULL;
    int32_t jCount;
    int32_t ret;

    devStrings->language = usbFnString->language;
    devStrings->strCount = 0;
    usbString = usbFnString->strings;
    while (usbString->s) {
        devStrings->strCount++;
        usbString++;
    }
    devStrings->strings = UsbFnMemCalloc((devStrings->strCount + 1) * sizeof(struct FconfigUsbString));
    if (devStrings->strings == NULL) {
        HDF_LOGE("%{public}s: UsbFnMemCalloc failed!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    devStrings->strings[devStrings->strCount].str.len = 0;
    devStrings->strings[devStrings->strCount].str.s = NULL;
    usbString = usbFnString->strings;
    for (jCount = 0; jCount < (int)devStrings->strCount; jCount++) {
        devStrings->strings[jCount].id = usbString[jCount].id;
        ret = UsbFnAdapterCreateFconfigString(&devStrings->strings[jCount].str, usbString[jCount].s);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: create string failed!", __func__);
            UsbFnMemFree(devStrings->strings[jCount].str.s);
            goto FAIL;
        }
    }
    ret = handle_ioctl(fd, FCONFIG_CMD_WRITE_STRINGS, devStrings);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
        goto FAIL;
    }
    for (jCount = 0; jCount < (int)devStrings->strCount; jCount++) {
        UsbFnMemFree(devStrings->strings[jCount].str.s);
    }
    UsbFnMemFree(devStrings->strings);
    return 0;
FAIL:
    while ((--jCount) >= 0) {
        UsbFnMemFree(devStrings->strings[jCount].str.s);
    }
    UsbFnMemFree(devStrings->strings);
    return -1;
}

static int32_t UsbFnAdapterWriteDevStrings(
    int32_t fd, struct FconfigString * const gadgetName, const struct UsbFnDeviceDesc *descriptor)
{
    if (gadgetName == NULL || descriptor == NULL) {
        HDF_LOGE("%{public}s: udcName is NULL", __func__);
        return HDF_ERR_IO;
    }

    struct FconfigDevStrings devStrings;
    devStrings.gadgetName.len = gadgetName->len;
    devStrings.gadgetName.s = gadgetName->s;
    for (uint32_t iCount = 0; descriptor->deviceStrings[iCount]; iCount++) {
        int32_t ret = UsbFnAdapterWriteDevString(fd, &devStrings, descriptor->deviceStrings[iCount]);
        if (ret != 0) {
            HDF_LOGE("%{public}s: UsbFnAdapterWriteDevString failed", __func__);
            return HDF_ERR_IO;
        }
    }
    return 0;
}

static int32_t UsbFnAdapterFillConfigDesc(
    struct UsbConfigDescriptor * const cfgDesc, struct UsbFnConfiguration * const usbFnConfig)
{
    if (cfgDesc == NULL || usbFnConfig == NULL) {
        HDF_LOGE("%{public}s: name is NULL", __func__);
        return HDF_ERR_IO;
    }
    cfgDesc->bConfigurationValue = usbFnConfig->configurationValue;
    cfgDesc->bmAttributes = usbFnConfig->attributes;
    cfgDesc->bMaxPower = usbFnConfig->maxPower;
    cfgDesc->iConfiguration = usbFnConfig->iConfiguration;
    return 0;
}

static int32_t UsbFnAdapterOpenPipe(const char *funcName, int32_t epIndex)
{
    int32_t ret;
    char epName[MAX_NAMELEN];
    const char *pName = &epName[0];
    int32_t i;
    int32_t ep;
    if (funcName == NULL || epIndex < 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    ret = snprintf_s(epName, MAX_NAMELEN, MAX_NAMELEN - 1, "/dev/%s/ep%d", funcName, epIndex);
    if (ret < 0) {
        HDF_LOGE("%{public}s: snprintf_s failed", __func__);
        return HDF_ERR_IO;
    }

    for (i = 0; i < OPEN_CNT; i++) {
        ep = handle_open(pName);
        if (ep > 0) {
            break;
        }
        usleep(SLEEP_TIME);
    }
    if (ep < 0) {
        HDF_LOGE("unable to open %{public}s", epName);
        return HDF_ERR_IO;
    }
    return ep;
}

static int32_t UsbFnAdapterClosePipe(int32_t ep)
{
    if (ep < 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    return handle_close(ep);
}

static void GetHeaderStr(struct UsbFnStrings ** const strings, struct UsbFunctionfsStringsHead *headerStr)
{
    uint32_t i, j;
    uint32_t langCount = 0;
    uint32_t strCount = 0;
    uint32_t len = 0;
    for (i = 0; strings[i] != NULL; i++) {
        langCount++;
        for (j = 0; strings[i]->strings[j].s; j++) {
            len += strlen(strings[i]->strings[j].s) + sizeof(char);
        }
        strCount = j;
    }
    headerStr->magic = htole32(FUNCTIONFS_STRINGS_MAGIC);
    headerStr->length = htole32(sizeof(struct UsbFunctionfsStringsHead) + langCount * sizeof(uint16_t) + len);
    headerStr->strCount = strCount;
    headerStr->langCount = langCount;
}

static int32_t UsbFnWriteStrings(int32_t ep0, struct UsbFnStrings ** const strings)
{
    uint8_t *str = NULL;
    uint8_t *whereDec = NULL;
    uint32_t i, j;
    int32_t ret;
    struct UsbFunctionfsStringsHead headerStr = {0};

    GetHeaderStr(strings, &headerStr);
    str = UsbFnMemCalloc(headerStr.length);
    if (str == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }

    whereDec = str;
    ret = memcpy_s(whereDec, headerStr.length, &headerStr, sizeof(struct UsbFunctionfsStringsHead));
    if (ret != EOK) {
        goto ERR;
    }
    whereDec += sizeof(struct UsbFunctionfsStringsHead);

    for (i = 0; i < headerStr.langCount; i++) {
        ret = memcpy_s(whereDec, headerStr.length - (whereDec - str), &strings[i]->language, sizeof(uint16_t));
        if (ret != EOK) {
            goto ERR;
        }
        whereDec += sizeof(uint16_t);
        for (j = 0; j < headerStr.strCount; j++) {
            if (strlen(strings[i]->strings[j].s)) {
                ret = memcpy_s(whereDec, headerStr.length - (whereDec - str), strings[i]->strings[j].s,
                    strlen(strings[i]->strings[j].s));
                whereDec += strlen(strings[i]->strings[j].s) + sizeof(char);
            } else {
                break;
            }
            if (ret != EOK) {
                goto ERR;
            }
        }
    }

    if (handle_write(ep0, str, headerStr.length) < 0) {
        goto ERR;
    }
    UsbFnMemFree(str);
    return 0;
ERR:
    UsbFnMemFree(str);
    return HDF_FAILURE;
}

static void GetCountAndHead(struct UsbFunctionfsDescsHeadV2 *header, uint32_t *fsCount, uint32_t *hsCount,
    uint32_t *ssCount, const struct UsbFnFunction *func)
{
    int32_t i;
    uint32_t lenCount = 0;
    uint32_t lenDes = 0;
    *fsCount = 0;
    *hsCount = 0;
    *ssCount = 0;

    for (i = 0; func->fsDescriptors[i] != NULL; i++) {
        (*fsCount)++;
        lenDes += func->fsDescriptors[i]->bLength;
    }
    for (i = 0; func->hsDescriptors[i] != NULL; i++) {
        (*hsCount)++;
        lenDes += func->hsDescriptors[i]->bLength;
    }
    for (i = 0; func->ssDescriptors[i] != NULL; i++) {
        (*ssCount)++;
        lenDes += func->ssDescriptors[i]->bLength;
    }

    if (*fsCount) {
        lenCount += sizeof(uint32_t);
        header->flags |= htole32(FUNCTIONFS_HAS_FS_DESC);
    }
    if (*hsCount) {
        lenCount += sizeof(uint32_t);
        header->flags |= htole32(FUNCTIONFS_HAS_HS_DESC);
    }
    if (*ssCount) {
        lenCount += sizeof(uint32_t);
        header->flags |= htole32(FUNCTIONFS_HAS_SS_DESC);
    }

    header->magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
    header->length = htole32(sizeof(struct UsbFunctionfsDescsHeadV2) + lenCount + lenDes);
}

static int32_t WriteFuncDescriptors(uint8_t ** const whereDec, struct UsbDescriptorHeader ** const headDes)
{
    for (uint32_t i = 0; headDes[i] != NULL; i++) {
        if (memcpy_s(*whereDec, headDes[i]->bLength, headDes[i], headDes[i]->bLength) != EOK) {
            HDF_LOGE("%{public}s: memcpy_s failed!", __func__);
            return HDF_FAILURE;
        }
        *whereDec += headDes[i]->bLength;
    }
    return 0;
}

static int32_t CopyCount(uint8_t **whereDec, uint32_t fsCount, uint32_t hsCount, uint32_t ssCount)
{
    int32_t ret;
    if (fsCount) {
        ret = memcpy_s(*whereDec, sizeof(uint32_t), &fsCount, sizeof(uint32_t));
        if (ret != EOK) {
            return HDF_FAILURE;
        }
        *whereDec += sizeof(uint32_t);
    }
    if (hsCount) {
        ret = memcpy_s(*whereDec, sizeof(uint32_t), &hsCount, sizeof(uint32_t));
        if (ret != EOK) {
            return HDF_FAILURE;
        }
        *whereDec += sizeof(uint32_t);
    }
    if (ssCount) {
        ret = memcpy_s(*whereDec, sizeof(uint32_t), &ssCount, sizeof(uint32_t));
        if (ret != EOK) {
            return HDF_FAILURE;
        }
        *whereDec += sizeof(uint32_t);
    }

    return 0;
}

static int32_t UsbFnAdapterCreatPipes(int32_t ep0, const struct UsbFnFunction *func)
{
    uint8_t *dec = NULL;
    uint8_t *whereDec = NULL;
    int32_t ret;
    uint32_t fsCount;
    uint32_t hsCount;
    uint32_t ssCount;
    struct UsbFunctionfsDescsHeadV2 header = {0};

    GetCountAndHead(&header, &fsCount, &hsCount, &ssCount, func);

    dec = UsbFnMemCalloc(header.length);
    if (dec == NULL) {
        HDF_LOGE("%{public}s: UsbFnMemCalloc failed!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    whereDec = dec;

    ret = memcpy_s(whereDec, header.length, &header, sizeof(struct UsbFunctionfsDescsHeadV2));
    if (ret != EOK) {
        UsbFnMemFree(dec);
        return HDF_FAILURE;
    }
    whereDec += sizeof(struct UsbFunctionfsDescsHeadV2);

    ret = CopyCount(&whereDec, fsCount, hsCount, ssCount);
    if (ret != EOK) {
        UsbFnMemFree(dec);
        return HDF_FAILURE;
    }

    ret = WriteFuncDescriptors(&whereDec, func->fsDescriptors);
    if (ret != EOK) {
        UsbFnMemFree(dec);
        return HDF_FAILURE;
    }

    ret = WriteFuncDescriptors(&whereDec, func->hsDescriptors);
    if (ret != EOK) {
        UsbFnMemFree(dec);
        return HDF_FAILURE;
    }

    ret = WriteFuncDescriptors(&whereDec, func->ssDescriptors);
    if (ret != EOK) {
        UsbFnMemFree(dec);
        return HDF_FAILURE;
    }

    if (handle_write(ep0, dec, header.length) < 0) {
        HDF_LOGE("unable do write descriptors");
        UsbFnMemFree(dec);
        return HDF_ERR_IO;
    }

    UsbFnMemFree(dec);
    ret = UsbFnWriteStrings(ep0, func->strings);
    return ret;
}

void UsbFnAdapterPipeCreateAndClose(int32_t fdEp0, struct UsbFnConfiguration * const usbFnConfig, int32_t iCount)
{
    if (UsbFnAdapterCreatPipes(fdEp0, usbFnConfig->functions[iCount]) != HDF_SUCCESS) {
        goto FAIL2;
    }

    if (UsbFnAdapterClosePipe(fdEp0) != HDF_SUCCESS) {
        goto FAIL2;
    }
FAIL2:
    UsbFnAdapterClosePipe(fdEp0);
}

static int32_t UsbFnAdapterWriteFunctions(int32_t fd, struct UsbFnConfiguration * const usbFnConfig, int32_t cmd,
    struct FconfigString * const gadgetName, struct FconfigString * const configName)
{
    if (usbFnConfig == NULL || gadgetName == NULL || configName == NULL) {
        HDF_LOGE("%{public}s: usbFnConfig is NULL", __func__);
        return HDF_ERR_IO;
    }

    int32_t fdEp0;
    struct FconfigFuncInfo funcInfo;
    funcInfo.gadgetName.len = gadgetName->len;
    funcInfo.gadgetName.s = gadgetName->s;
    funcInfo.configName.len = configName->len;
    funcInfo.configName.s = configName->s;
    for (uint32_t iCount = 0; usbFnConfig->functions[iCount] != NULL; iCount++) {
        char tmp[MAX_PATHLEN];
        if (memset_s(tmp, MAX_PATHLEN, 0, MAX_PATHLEN) != EOK) {
            HDF_LOGE("%{public}s:%{public}d memset_s failed", __func__, __LINE__);
            return HDF_FAILURE;
        }
        int32_t ret =
            snprintf_s(tmp, MAX_PATHLEN, MAX_PATHLEN - 1, "generic.%s", usbFnConfig->functions[iCount]->funcName);
        if (ret < 0) {
            return HDF_ERR_IO;
        }
        ret = UsbFnAdapterCreateFconfigString(&funcInfo.funcName, tmp);
        if (ret != HDF_SUCCESS) {
            return HDF_ERR_MALLOC_FAIL;
        }
        if (handle_ioctl(fd, cmd, &funcInfo) != 0) {
            goto FAIL;
        }
        UsbFnMemFree(funcInfo.funcName.s);
        if (cmd == FCONFIG_CMD_DROP_FUNCTION) {
            continue;
        }

        fdEp0 = UsbFnAdapterOpenPipe(usbFnConfig->functions[iCount]->funcName, 0);
        if (fd < 0) {
            goto FAIL;
        }
        UsbFnAdapterPipeCreateAndClose(fdEp0, usbFnConfig, iCount);
    }
    return 0;
FAIL:
    UsbFnMemFree(funcInfo.funcName.s);
    return -1;
}

static int32_t UsbFnAdapterWriteConfigs(
    int32_t fd, struct FconfigString * const gadgetName, const struct UsbFnDeviceDesc * const descriptor)
{
    struct FconfigCfgDesc configDesc;
    char tmp[MAX_PATHLEN];

    if (gadgetName == NULL || descriptor == NULL || descriptor->configs == NULL) {
        HDF_LOGE("%{public}s: name is NULL", __func__);
        return HDF_ERR_IO;
    }
    for (uint32_t iCount = 0; descriptor->configs[iCount]; iCount++) {
        configDesc.gadgetName.len = gadgetName->len;
        configDesc.gadgetName.s = gadgetName->s;
        uint8_t confVal = descriptor->configs[iCount]->configurationValue;
        if (memset_s(tmp, MAX_PATHLEN, 0, MAX_PATHLEN) != EOK) {
            HDF_LOGE("%{public}s:%{public}d memset_s failed", __func__, __LINE__);
            return HDF_FAILURE;
        }
        int32_t ret = snprintf_s(tmp, MAX_PATHLEN, MAX_PATHLEN - 1, "b.%u", confVal);
        if (ret < 0) {
            return HDF_ERR_IO;
        }
        ret = UsbFnAdapterCreateFconfigString(&configDesc.configName, tmp);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: create config name failed!", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        ret = UsbFnAdapterFillConfigDesc(&configDesc.cfgDesc, descriptor->configs[iCount]);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: UsbFnMemCalloc failed!", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        ret = handle_ioctl(fd, FCONFIG_CMD_ADD_CONFIG, &configDesc);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: ioctl failed!", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        ret = UsbFnAdapterWriteFunctions(
            fd, descriptor->configs[iCount], FCONFIG_CMD_MAKE_FUNCTION, gadgetName, &configDesc.configName);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: write func failed!", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        UsbFnMemFree(configDesc.configName.s);
    }
    return 0;
}

static int32_t UsbFnAdapterWriteFcofnigUDC(
    int32_t fd, int32_t cmd, struct FconfigString * const gadgetName, const char *udcName)
{
    int32_t ret;
    struct FconfigUdcInfo udcInfo;

    if (gadgetName == NULL || udcName == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    udcInfo.gadgetName.len = gadgetName->len;
    udcInfo.gadgetName.s = gadgetName->s;
    ret = UsbFnAdapterCreateFconfigString(&udcInfo.udcName, udcName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create udc_name failed!", __func__);
        return HDF_ERR_IO;
    }
    ret = handle_ioctl(fd, cmd, &udcInfo);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
    }
    UsbFnMemFree(udcInfo.udcName.s);

    return ret;
}

static int32_t UsbFnAdapterCreateDevice(const char *udcName, const char *devName, struct UsbFnDeviceDesc *descriptor)
{
    int32_t fd;
    int32_t ret;
    struct FconfigString gadgetName;

    fd = UsbFnAdapterOpenFn();
    if (fd < 0) {
        return HDF_ERR_IO;
    }
    ret = UsbFnAdapterCreateFconfigString(&gadgetName, devName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create gadget name failed!", __func__);
        goto FAIL;
    }
    ret = UsbFnAdapterWriteGadget(fd, FCONFIG_CMD_MAKE_GADGET, &gadgetName);
    if (ret != HDF_SUCCESS) {
        dprintf("%s: UsbFnAdapterWriteGadget failed!", __func__);
        goto EXIT;
    }
    ret = UsbFnAdapterWriteDevDesc(fd, &gadgetName, descriptor);
    if (ret != HDF_SUCCESS) {
        dprintf("%s: UsbFnAdapterWriteDevDesc failed!", __func__);
        goto EXIT;
    }
    ret = UsbFnAdapterWriteDevStrings(fd, &gadgetName, descriptor);
    if (ret != HDF_SUCCESS) {
        dprintf("%s: UsbFnAdapterWriteDevStrings failed!", __func__);
        goto EXIT;
    }
    ret = UsbFnAdapterWriteConfigs(fd, &gadgetName, descriptor);
    if (ret != HDF_SUCCESS) {
        dprintf("%s: UsbFnAdapterWriteConfigs failed!", __func__);
        goto EXIT;
    }
    ret = UsbFnAdapterWriteFcofnigUDC(fd, FCONFIG_CMD_ENABLE_UDC, &gadgetName, udcName);
    if (ret != HDF_SUCCESS) {
        dprintf("%s: UsbFnAdapterWriteFcofnigUDC failed!", __func__);
        goto EXIT;
    }
    dprintf("%s: create device success!\n", __func__);
EXIT:
    UsbFnMemFree(gadgetName.s);
FAIL:
    if (UsbFnAdapterClosefn(fd) != 0) {
        dprintf("%s[%d] close fconfig failed\n", __func__, __LINE__);
    }

    return ret;
}

static int32_t UsbFnAdapterDelConfigs(
    int32_t configFd, struct FconfigString * const gadgetName, struct UsbFnDeviceDesc * const descriptor)
{
    struct FconfigCfgDesc configDesc;
    struct FconfigString configName;
    char tmp[MAX_PATHLEN];
    int32_t ret;
    int32_t iCount;
    uint8_t confVal;

    if (gadgetName == NULL || descriptor == NULL) {
        HDF_LOGE("%{public}s: name is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    for (iCount = 0; descriptor->configs[iCount]; iCount++) {
        configDesc.gadgetName.len = gadgetName->len;
        configDesc.gadgetName.s = gadgetName->s;
        confVal = descriptor->configs[iCount]->configurationValue;
        ret = snprintf_s(tmp, MAX_PATHLEN, MAX_PATHLEN - 1, "b.%u", confVal);
        if (ret < 0) {
            HDF_LOGE("%{public}s: snprintf_s failed", __func__);
            return HDF_ERR_IO;
        }
        ret = UsbFnAdapterCreateFconfigString(&configName, tmp);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: create config name failed!", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        configDesc.configName.len = configName.len;
        configDesc.configName.s = configName.s;
        ret = UsbFnAdapterWriteFunctions(
            configFd, descriptor->configs[iCount], FCONFIG_CMD_DROP_FUNCTION, gadgetName, &configName);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: write func failed!", __func__);
            goto FAIL;
        }
        ret = UsbFnAdapterFillConfigDesc(&configDesc.cfgDesc, descriptor->configs[iCount]);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: UsbFnMemCalloc failed!", __func__);
            goto FAIL;
        }
        ret = handle_ioctl(configFd, FCONFIG_CMD_REMOVE_CONFIG, &configDesc);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: ioctl failed!", __func__);
            goto FAIL;
        }
        UsbFnMemFree(configName.s);
    }
    return 0;
FAIL:
    UsbFnMemFree(configName.s);
    return -1;
}

static int32_t UsbFnAdapterDelDevice(const char *devName, const char *udcName, struct UsbFnDeviceDesc *descriptor)
{
    int32_t configFd;
    int32_t ret;
    struct FconfigString gadgetName;

    if (devName == NULL || udcName == NULL || descriptor == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    configFd = UsbFnAdapterOpenFn();
    if (configFd <= 0) {
        return HDF_ERR_IO;
    }
    ret = UsbFnAdapterCreateFconfigString(&gadgetName, devName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create gadget_name failed!", __func__);
        return HDF_ERR_IO;
    }
    ret = UsbFnAdapterWriteFcofnigUDC(configFd, FCONFIG_CMD_DISABLE_UDC, &gadgetName, udcName);
    if (ret != HDF_SUCCESS) {
        goto FAIL;
    }
    ret = UsbFnAdapterDelConfigs(configFd, &gadgetName, descriptor);
    if (ret != HDF_SUCCESS) {
        goto FAIL;
    }
    ret = UsbFnAdapterWriteGadget(configFd, FCONFIG_CMD_DROP_GADGET, &gadgetName);
    if (ret != HDF_SUCCESS) {
        goto FAIL;
    }
    ret = UsbFnAdapterClosefn(configFd);

FAIL:
    UsbFnMemFree(gadgetName.s);
    return ret;
}

static int32_t UsbFnAdapterGetPipeInfo(int32_t ep, struct UsbFnPipeInfo * const pipeInfo)
{
    if (ep <= 0 || pipeInfo == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    struct usb_endpoint_descriptor desc;
    if (memset_s(&desc, sizeof(desc), 0, sizeof(desc)) != EOK) {
        HDF_LOGE("%{public}s:%{public}d memset_s failed", __func__, __LINE__);
        return HDF_FAILURE;
    }

    int32_t ret = handle_ioctl(ep, GENERIC_CMD_GET_PIPE_INFO, &desc);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: FUNCTIONFS_ENDPOINT_DESC failed!", __func__);
        return HDF_ERR_IO;
    }

    pipeInfo->type = desc.bmAttributes;
    pipeInfo->dir = USB_PIPE_DIRECTION_OUT;
    if (desc.bEndpointAddress & 0x80) {
        pipeInfo->dir = USB_PIPE_DIRECTION_IN;
    }

    pipeInfo->maxPacketSize = (desc.wMaxPacketSize[0] | (desc.wMaxPacketSize[1] << 8));
    pipeInfo->interval = desc.bInterval;

    return 0;
}

static int32_t UsbFnAdapterQueueInit(int32_t ep)
{
    (void)ep;
    return 0;
}

static int32_t UsbFnAdapterQueueDel(int32_t ep)
{
    (void)ep;
    return 0;
}

static int32_t UsbFnAdapterReleaseBuf(int32_t ep, const struct GenericMemory *mem)
{
    return handle_ioctl(ep, GENERIC_CMD_FREE_MEM, &mem);
}

static int32_t UsbFnAdapterPipeIo(int32_t ep, struct IoData * const ioData)
{
    int32_t ret;
    if (ep <= 0 || ioData == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (ioData->aio) {
        ret = handle_write(ep, (void *)ioData->buf, ioData->len);
    } else {
        ret = handle_ioctl(ep, GENERIC_CMD_ENDPOINT_IO, ioData);
    }
    return ret;
}

static int32_t UsbFnAdapterCancelIo(int32_t ep, const struct IoData * const ioData)
{
    struct GenericMemory mem;

    mem.buf = ioData->buf;
    mem.size = ioData->len;

    return handle_ioctl(ep, GENERIC_CMD_CANCEL_REQUEST, &mem);
}

static int32_t UsbFnAdapterRequestGetStatus(int32_t ep, const struct IoData *ioData)
{
    if (ep <= 0 || ioData == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    return handle_ioctl(ep, GENERIC_CMD_GET_REQ_STATUS, (void *)ioData);
}

static uint8_t *UsbFnAdapterMapAddr(int32_t ep, uint32_t len)
{
    return handle_mmap(ep, len);
}

static int32_t UsbFnAdapterUnmapAddr(uint8_t *mapAddr, uint32_t len)
{
    (void)mapAddr;
    (void)len;
    return 0;
}

static int32_t Ep0Event(struct UsbFnEventAll * const event, struct FconfigPollFd * const pfds)
{
    int32_t ret;
    uint8_t i;
    for (i = 0; i < event->ep0Num; i++) {
        if (pfds[i].revents & POLLIN) {
            if (event->ep0[i] < 0) {
                HDF_LOGE("ep0[%{public}d] read after closed", i);
                continue;
            }
            ret = handle_read(event->ep0[i], &event->ep0Event[i].ctrlEvent, sizeof(struct UsbFnCtrlEvent));
            if (ret < 0) {
                HDF_LOGE("unable to read event from ep0");
                return ret;
            }
            event->ep0Event[i].type = USB_EP0_CTRL_EVENT;
        } else if (pfds[i].revents & POLLOUT) {
            ret = handle_ioctl(event->ep0[i], GENERIC_CMD_GET_EP0_EVENT, &event->ep0Event[i].reqEvent);
            if (ret < 0) {
                HDF_LOGE("unable to read reqEvent from ep0");
                return ret;
            }
            event->ep0Event[i].type = USB_EP0_IO_COMPLETED;
        }
    }
    return 0;
}

static int32_t EpEvent(struct UsbFnEventAll * const event, struct FconfigPollFd * const pfds)
{
    uint8_t i;
    int32_t ret;
    for (i = 0; i < event->epNum; i++) {
        if ((pfds[i + event->ep0Num].revents & POLLIN)) {
            if (event->epx[i] < 0) {
                HDF_LOGE("epx[%{public}d] read after closed", i);
                continue;
            }
            ret = handle_read(event->epx[i], event->reqEvent[i], MAX_REQUEST * sizeof(struct UsbFnReqEvent));
            if (ret < 0) {
                HDF_LOGE("unable to read event from eps");
                return ret;
            }
            event->numEvent[i] = (uint8_t)(ret / sizeof(struct UsbFnReqEvent));
        }
    }
    return 0;
}

static int32_t UsbFnAdapterPollEvent(struct UsbFnEventAll *event, int32_t timeout)
{
    uint8_t i;
    struct FconfigPollFd pfds[16] = {0};
    struct FconfigPollFd *pfd = &pfds[0];
    if (event == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if ((event->ep0Num + event->epNum) == 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (memset_s(&pfds, sizeof(pfds), 0, sizeof(pfds)) != EOK) {
        HDF_LOGE("%{public}s:%{public}d memset_s failed", __func__, __LINE__);
        return HDF_FAILURE;
    }

    for (i = 0; i < event->ep0Num; i++) {
        if (event->ep0[i] <= 0) {
            HDF_LOGE("%{public}s: ep[%{public}d] = %{public}d", __func__, i, event->ep0[i]);
            return HDF_ERR_INVALID_PARAM;
        }
        pfds[i].fd = event->ep0[i];
        pfds[i].events = POLLIN | POLLOUT;
    }
    for (i = 0; i < event->epNum; i++) {
        if (event->epx[i] <= 0) {
            HDF_LOGE("%{public}s: ep[%{public}d] = %{public}d", __func__, i, event->epx[i]);
            return HDF_ERR_INVALID_PARAM;
        }
        pfds[i + event->ep0Num].fd = event->epx[i];
        pfds[i + event->ep0Num].events = POLLIN;
    }
    for (i = 0; i < (event->ep0Num + event->epNum); i++) {
        pfds[i].revents = (uint32_t)handle_poll(pfds[i].fd, timeout);
        if (pfds[i].revents < 0) {
            HDF_LOGE("%{public}s: handle_poll failed", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    if (Ep0Event(event, pfd) < 0) {
        HDF_LOGE("%{public}s: handle_poll failed", __func__);
        return HDF_ERR_IO;
    }
    if (EpEvent(event, pfd) < 0) {
        HDF_LOGE("%{public}s: handle_poll failed", __func__);
        return HDF_ERR_IO;
    }
    return 0;
}

static int32_t UsbFnAdapterWriteUDC(const char *deviceName, const char *udcName, int32_t enable)
{
    struct FconfigUdcInfo udcInfo;

    int32_t configFd = UsbFnAdapterOpenFn();
    if (configFd <= 0) {
        return HDF_ERR_IO;
    }

    int32_t ret = UsbFnAdapterCreateFconfigString(&udcInfo.gadgetName, deviceName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create gadget_name failed!", __func__);
        return HDF_ERR_IO;
    }

    int32_t cmd = enable ? FCONFIG_CMD_ENABLE_UDC : FCONFIG_CMD_DISABLE_UDC;
    ret = UsbFnAdapterWriteFcofnigUDC(configFd, cmd, &udcInfo.gadgetName, udcName);
    if (ret != HDF_SUCCESS) {
        return HDF_ERR_IO;
    }

    ret = UsbFnAdapterClosefn(configFd);
    if (ret != 0) {
        HDF_LOGE("%{public}s: close failed!", __func__);
    }
    return 0;
}

static int32_t UsbFnWriteProp(const char *deviceName, const char *propName, uint32_t propValue)
{
    struct FconfigDevdescInfo info;
    if (deviceName == NULL || propName == NULL) {
        HDF_LOGE("%{public}s: param invail!", __func__);
        return HDF_ERR_IO;
    }
    int32_t ret = UsbFnAdapterCreateFconfigString(&info.gadgetName, deviceName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create gadget name failed!", __func__);
        return HDF_ERR_IO;
    }
    info.prop.propName = propName;
    info.prop.propValue = (uint32_t)propValue;
    int32_t configFd = UsbFnAdapterOpenFn();
    if (configFd <= 0) {
        ret = HDF_ERR_IO;
        goto FAIL;
    }
    ret = handle_ioctl(configFd, FCONFIG_CMD_CHAGE_DEVINFO, &info);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
        goto FAIL;
    }
    ret = UsbFnAdapterClosefn(configFd);
    if (ret != 0) {
        HDF_LOGE("%{public}s: close failed!", __func__);
    }
FAIL:
    UsbFnMemFree(info.gadgetName.s);

    return ret;
}

static int32_t UsbFnWriteDesString(
    const char *deviceName, uint16_t lang, const char *stringName, const char *stringValue)
{
    struct FconfigDevDescString info;
    if (deviceName == NULL || stringName == NULL || stringValue == NULL) {
        HDF_LOGE("%{public}s: param invail!", __func__);
        return HDF_ERR_IO;
    }
    int32_t ret = UsbFnAdapterCreateFconfigString(&info.gadgetName, deviceName);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: create gadget name failed!", __func__);
        return HDF_ERR_IO;
    }
    info.prop.lang = lang;
    info.prop.propName = stringName;
    info.prop.propValue = stringValue;
    int32_t configFd = UsbFnAdapterOpenFn();
    if (configFd <= 0) {
        dprintf("%s, %d\n", __func__, __LINE__);
        ret = HDF_ERR_IO;
        goto FAIL;
    }
    ret = handle_ioctl(configFd, FCONFIG_CMD_CHAGE_DEVSTRING, &info);
    if (ret != 0) {
        HDF_LOGE("%{public}s: ioctl failed!", __func__);
        goto FAIL;
    }
    ret = UsbFnAdapterClosefn(configFd);
    if (ret != 0) {
        HDF_LOGE("%{public}s: close failed!", __func__);
    }
FAIL:
    UsbFnMemFree(info.gadgetName.s);

    return ret;
}

static void *UsbFnMemAlloc(size_t size)
{
    return UsbFnMemCalloc(size);
}

static void *UsbFnMemCalloc(size_t size)
{
    void *buf = OsalMemCalloc(size);
    if (buf == NULL) {
        HDF_LOGE("%{public}s: %{public}d, OsalMemCalloc failed", __func__, __LINE__);
        return NULL;
    }

    return buf;
}

void UsbFnMemFree(const void *mem)
{
    if (mem == NULL) {
        HDF_LOGE("%{public}s:%{public}d invalid param mem.", __func__, __LINE__);
        return;
    }

    OsalMemFree((void *)mem);
    mem = NULL;
}

static struct UsbFnAdapterOps g_usbFnAdapter = {
    .createDevice = UsbFnAdapterCreateDevice,
    .delDevice = UsbFnAdapterDelDevice,

    .openPipe = UsbFnAdapterOpenPipe,
    .closePipe = UsbFnAdapterClosePipe,
    .getPipeInfo = UsbFnAdapterGetPipeInfo,

    .queueInit = UsbFnAdapterQueueInit,
    .queueDel = UsbFnAdapterQueueDel,
    .releaseBuf = UsbFnAdapterReleaseBuf,
    .pipeIo = UsbFnAdapterPipeIo,
    .cancelIo = UsbFnAdapterCancelIo,
    .getReqStatus = UsbFnAdapterRequestGetStatus,
    .mapAddr = UsbFnAdapterMapAddr,
    .unmapAddr = UsbFnAdapterUnmapAddr,
    .pollEvent = UsbFnAdapterPollEvent,
    .writeUDC = UsbFnAdapterWriteUDC,
    .writeProp = UsbFnWriteProp,
    .writeDesString = UsbFnWriteDesString,
};

struct UsbFnAdapterOps *UsbFnAdapterGetOps(void)
{
    return &g_usbFnAdapter;
}
