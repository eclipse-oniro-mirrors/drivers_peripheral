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

#include "hdf_audio_server_common.h"
#include "audio_adapter_info_common.h"
#include "audio_events.h"
#include "audio_hal_log.h"
#include "hdf_audio_events.h"
#include "hdf_audio_server.h"
#include "hdf_audio_server_capture.h"
#include "hdf_audio_server_render.h"

#define HDF_LOG_TAG HDF_AUDIO_HAL_STUB

struct AudioAdapterDescriptor *g_descs = NULL;
struct AudioManager *g_serverManager = NULL;

#define MANAGER_ADAPTER_NAME_LEN        32
#define SERVER_INFO_LEN 128

int32_t g_serverAdapterNum = 0;
struct AudioInfoInAdapter *g_renderAndCaptureManage = NULL;

static struct AudioEvent g_audioEventPnp = {
    .eventType = HDF_AUDIO_EVENT_UNKOWN,
    .deviceType = HDF_AUDIO_DEVICE_UNKOWN,
};

static struct AudioEvent g_audioEventLoad = {
    .eventType = HDF_AUDIO_EVENT_UNKOWN,
    .deviceType = HDF_AUDIO_DEVICE_UNKOWN,
};

static struct AudioEvent g_audioEventService = {
    .eventType = HDF_AUDIO_EVENT_UNKOWN,
    .deviceType = HDF_AUDIO_DEVICE_UNKOWN,
};

static int32_t AdapterManageInit(struct AudioInfoInAdapter *adapterManage,
    const char *adapterName)
{
    int32_t ret;

    if (adapterManage == NULL || adapterName == NULL) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return HDF_FAILURE;
    }

    adapterManage->adapterName = (char *)calloc(1, MANAGER_ADAPTER_NAME_LEN);
    if (adapterManage->adapterName == NULL) {
        HDF_LOGE("%{public}s: calloc adapter name failed!", __func__);
        return HDF_FAILURE;
    }

    ret = memcpy_s((void *)adapterManage->adapterName, MANAGER_ADAPTER_NAME_LEN,
        adapterName, MANAGER_ADAPTER_NAME_LEN);
    if (ret != EOK) {
        HDF_LOGE("%{public}s: memcpy adapter name fail!", __func__);
        AudioMemFree((void **)&adapterManage->adapterName);

        return HDF_FAILURE;
    }

    adapterManage->adapter = NULL;
    adapterManage->adapterUserNum = 0;
    adapterManage->renderStatus = 0;
    adapterManage->renderPriority = -1;
    adapterManage->render = NULL;
    adapterManage->renderBusy = false;
    adapterManage->renderDestory = false;
    adapterManage->renderPid = 0;
    adapterManage->captureStatus = 0;
    adapterManage->capturePriority = -1;
    adapterManage->capture = NULL;
    adapterManage->captureBusy = false;
    adapterManage->captureDestory = false;
    adapterManage->capturePid = 0;

    return HDF_SUCCESS;
}

int32_t ServerManageGetAdapterNum(void)
{
    return ((g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum);
}

struct AudioInfoInAdapter *ServerManageGetAdapters(void)
{
    return g_renderAndCaptureManage;
}


void AdaptersServerManageRelease(
    const struct AudioInfoInAdapter *adaptersManage, int32_t num)
{
    int32_t i;

    if (adaptersManage == NULL || num <= 0) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return;
    }

    num = (num > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : num;
    for (i = 0; i < num; i++) {
        if (adaptersManage[i].adapterName != NULL) {
            AudioMemFree((void **)&adaptersManage[i].adapterName);
        }
    }
    AudioMemFree((void **)&adaptersManage);
}

void AdaptersServerManageInfomationRecycle(void)
{
    AdaptersServerManageRelease(g_renderAndCaptureManage, g_serverAdapterNum);
    g_renderAndCaptureManage = NULL;
    g_serverAdapterNum = 0;
}

int32_t AdaptersServerManageInit(const struct AudioAdapterDescriptor *descs, int32_t num)
{
    int32_t i, ret;
    struct AudioInfoInAdapter *adaptersManage = NULL;

    if (descs == NULL || num <= 0) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return HDF_FAILURE;
    }

    num = (num > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : num;
    adaptersManage = (struct AudioInfoInAdapter *)calloc(1,
        num * sizeof(struct AudioInfoInAdapter));
    if (adaptersManage == NULL) {
        HDF_LOGE("%{public}s: calloc adaptersManage failed! ", __func__);

        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        ret = AdapterManageInit(&adaptersManage[i], descs[i].adapterName);
        if (ret != HDF_SUCCESS) {
            AdaptersServerManageRelease(adaptersManage, num);

            return ret;
        }
    }
    g_serverAdapterNum = num;
    g_renderAndCaptureManage = adaptersManage;

    return HDF_SUCCESS;
}

int32_t HdiServiceRenderCaptureReadData(struct HdfSBuf *data, const char **adapterName, uint32_t *pid)
{
    if (adapterName == NULL || data == NULL || pid == NULL) {
        return HDF_FAILURE;
    }
    if ((*adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterName Is NULL ", __func__);
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, pid)) {
        HDF_LOGE("%{public}s: read buf fail ", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioAdapterListGetAdapterCapture(const char *adapterName,
    struct AudioAdapter **adapter, struct AudioCapture **capture)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (adapterName == NULL || adapter == NULL || capture == NULL) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            HDF_LOGE("%{public}s: The pointer is null ", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            *adapter = g_renderAndCaptureManage[i].adapter;
            *capture = g_renderAndCaptureManage[i].capture;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioDestroyCaptureInfoInAdapter(const char *adapterName)
{
    int32_t i;

    LOG_FUN_INFO();
    if (adapterName == NULL) {
        HDF_LOGE("%{public}s: Param Is NULL ", __func__);
        return HDF_FAILURE;
    }

    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].captureStatus = 0;
            g_renderAndCaptureManage[i].capturePriority = -1;
            g_renderAndCaptureManage[i].capture = NULL;
            g_renderAndCaptureManage[i].capturePid = 0;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t AudioDestroyFormerCapture(struct AudioInfoInAdapter *captureManage)
{
    LOG_FUN_INFO();
    if (captureManage == NULL || captureManage->adapter == NULL || captureManage->capture == NULL) {
        HDF_LOGE("%{public}s: input para is NULL ", __func__);
        return HDF_FAILURE;
    }
    int count = 0;
    captureManage->captureDestory = true;
    while (captureManage->captureBusy) {
        if (count > 1000) { // Less than 1000
            HDF_LOGE("%{public}s: , count = %{public}d", __func__, count);
            captureManage->captureDestory = false;
            return AUDIO_HAL_ERR_AI_BUSY; // capture is busy now
        }
        usleep(500); // sleep 500us
        count++;
    }
    captureManage->capturePid = 0;
    if (captureManage->adapter->DestroyCapture(captureManage->adapter, captureManage->capture)) {
        captureManage->captureDestory = false;
        return HDF_FAILURE;
    }
    captureManage->capture = NULL;
    captureManage->captureStatus = 0;
    captureManage->captureBusy = false;
    captureManage->captureDestory = false;
    captureManage->renderPriority = -1;
    return HDF_SUCCESS;
}

int32_t AudioJudgeCapturePriority(const int32_t priority, int which)
{
    int num;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (which < 0 || which >= num) {
        HDF_LOGE("%{public}s: invalid value! ", __func__);
        return HDF_FAILURE;
    }
    if (!(g_renderAndCaptureManage[which].captureStatus)) {
        return HDF_SUCCESS;
    } else {
        if (g_renderAndCaptureManage[which].capturePriority <= priority) {
            return AudioDestroyFormerCapture(&g_renderAndCaptureManage[which]);
        } else {
            return AUDIO_HAL_ERR_AI_BUSY; // capture is busy now
        }
    }
    return HDF_FAILURE;
}

int32_t AudioCreatCaptureCheck(const char *adapterName, const int32_t priority)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (adapterName == NULL) {
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            return AudioJudgeCapturePriority(priority, i);
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t AudioAddCaptureInfoInAdapter(const char *adapterName,
    struct AudioCapture *capture,
    const struct AudioAdapter *adapter,
    const int32_t priority,
    uint32_t capturePid)
{
    int32_t i, num;

    if (adapterName == NULL || adapter == NULL || capture == NULL) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage == NULL || g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].captureStatus = 1;
            g_renderAndCaptureManage[i].capturePriority = priority;
            g_renderAndCaptureManage[i].capture = capture;
            g_renderAndCaptureManage[i].capturePid = capturePid;
            HDF_LOGE("%{public}s: , (uint64_t)g_renderAndCaptureManage[i].capture = %{public}p",
                __func__, g_renderAndCaptureManage[i].capture);
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t WriteAudioSampleAttributes(struct HdfSBuf *reply, const struct AudioSampleAttributes *attrs)
{
    if (reply == NULL || attrs == NULL) {
        return HDF_FAILURE;
    }
    uint32_t tempAttrParam = (uint32_t)attrs->type;
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = (uint32_t)attrs->interleaved;
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = (uint32_t)attrs->format;
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->sampleRate)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->channelCount)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->period)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->frameSize)) {
        return HDF_FAILURE;
    }
    tempAttrParam = (uint32_t)(attrs->isBigEndian);
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = (uint32_t)(attrs->isSignedData);
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->startThreshold)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->stopThreshold)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint32(reply, attrs->silenceThreshold)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t ReadAudioSapmleAttrbutes(struct HdfSBuf *data, struct AudioSampleAttributes *attrs)
{
    if (data == NULL || attrs == NULL) {
        return HDF_FAILURE;
    }
    uint32_t tempAttrParam = 0;
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->type = (enum AudioCategory)tempAttrParam;
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->interleaved = (bool)tempAttrParam;
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->format = (enum AudioFormat)tempAttrParam;
    if (!HdfSbufReadUint32(data, &(attrs->sampleRate))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &(attrs->channelCount))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &(attrs->period))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &(attrs->frameSize))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->isBigEndian = (bool)tempAttrParam;
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->isSignedData = (bool)tempAttrParam;
    if (!HdfSbufReadUint32(data, &(attrs->startThreshold))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &(attrs->stopThreshold))) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &(attrs->silenceThreshold))) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioAdapterListGetAdapter(const char *adapterName, struct AudioAdapter **adapter)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == NULL || adapter == NULL) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            *adapter = g_renderAndCaptureManage[i].adapter;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioDestroyFormerRender(struct AudioInfoInAdapter *renderManage)
{
    LOG_FUN_INFO();
    if (renderManage == NULL || renderManage->adapter == NULL || renderManage->render == NULL) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    int count = 0;
    renderManage->renderDestory = true;
    while (renderManage->renderBusy) {
        if (count > 1000) { // Less than 1000
            HDF_LOGE("%{public}s: , count = %{public}d", __func__, count);
            renderManage->renderDestory = false;
            return AUDIO_HAL_ERR_AO_BUSY; // render is busy now
        }
        usleep(500); // sleep 500us
        count++;
    }
    renderManage->renderPid = 0;
    if (renderManage->adapter->DestroyRender(renderManage->adapter, renderManage->render)) {
        renderManage->renderDestory = false;
        return HDF_FAILURE;
    }
    renderManage->render = NULL;
    renderManage->renderStatus = 0;
    renderManage->renderBusy = false;
    renderManage->renderDestory = false;
    renderManage->renderPriority = -1;
    return HDF_SUCCESS;
}

int32_t AudioJudgeRenderPriority(const int32_t priority, int which)
{
    int32_t num;

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (which < 0 || which >= num) {
        HDF_LOGE("%{public}s: invalid value! ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage[which].renderPriority <= priority) {
        if (AudioDestroyFormerRender(&g_renderAndCaptureManage[which])) {
            HDF_LOGE("%{public}s: AudioDestroyFormerRender: Fail. ", __func__);
            return HDF_FAILURE;
        }
        return HDF_SUCCESS;
    } else {
        return AUDIO_HAL_ERR_AO_BUSY; // render is busy now
    }
    return HDF_FAILURE;
}

int32_t AudioCreatRenderCheck(const char *adapterName, const int32_t priority)
{
    int32_t i;

    LOG_FUN_INFO();
    if (adapterName == NULL || g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }

    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (!(g_renderAndCaptureManage[i].renderStatus)) {
                return HDF_SUCCESS;
            } else {
                return AudioJudgeRenderPriority(priority, i);
            }
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t AudioAddRenderInfoInAdapter(const char *adapterName,
    struct AudioRender *render,
    const struct AudioAdapter *adapter,
    const int32_t priority,
    uint32_t renderPid)
{
    int32_t i, num;

    if (adapterName == NULL || adapter == NULL || render == NULL) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].renderStatus = 1;
            g_renderAndCaptureManage[i].renderPriority = priority;
            g_renderAndCaptureManage[i].render = render;
            g_renderAndCaptureManage[i].renderPid = renderPid;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

void AudioSetRenderStatus(const char *adapterName, bool renderStatus)
{
    int32_t i, num;
    if (g_renderAndCaptureManage == NULL) {
        return;
    }
    LOG_FUN_INFO();
    if (adapterName == NULL) {
        HDF_LOGE("%{public}s: adapterName is null ", __func__);
        return;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].renderBusy = renderStatus;
            return;
        }
    }
    HDF_LOGE("%{public}s: AudioDestroyRenderInfoInAdapter: Can not find Adapter! ", __func__);
    return;
}

int32_t AudioGetRenderStatus(const char *adapterName)
{
    int32_t i;
    int32_t num;
    LOG_FUN_INFO();
    if (adapterName == NULL || g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum >
        MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (!g_renderAndCaptureManage[i].renderDestory) {
                return HDF_SUCCESS;
            } else {
                g_renderAndCaptureManage[i].renderBusy = false;
                return HDF_FAILURE;
            }
        }
    }
    HDF_LOGE("%{public}s: AudioDestroyRenderInfoInAdapter: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t AudioDestroyRenderInfoInAdapter(const char *adapterName)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (adapterName == NULL) {
        HDF_LOGE("%{public}s: adapterName is null ", __func__);
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].renderStatus = 0;
            g_renderAndCaptureManage[i].renderPriority = -1;
            g_renderAndCaptureManage[i].render = NULL;
            g_renderAndCaptureManage[i].renderPid = 0;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

int32_t AudioAdapterListGetPid(const char *adapterName, uint32_t *pid)
{
    LOG_FUN_INFO();
    int32_t i, num;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == NULL || pid == NULL) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            HDF_LOGE("%{public}s: i = %{public}d", __func__, i);
            *pid = g_renderAndCaptureManage[i].renderPid;
            HDF_LOGE("%{public}s: pid = %{public}u", __func__, *pid);
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioAdapterListGetAdapterRender(const char *adapterName,
    struct AudioAdapter **adapter, struct AudioRender **render)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == NULL || adapter == NULL || render == NULL) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            *adapter = g_renderAndCaptureManage[i].adapter;
            *render = g_renderAndCaptureManage[i].render;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioAdapterListGetRender(const char *adapterName, struct AudioRender **render, uint32_t pid)
{
    int32_t num;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == NULL || render == NULL) {
        HDF_LOGE("%{public}s: pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (int32_t i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].renderPid != pid) {
                HDF_LOGE("%{public}s: renderPid != pid ", __func__);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            *render = g_renderAndCaptureManage[i].render;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioAdapterListGetCapture(const char *adapterName, struct AudioCapture **capture, uint32_t pid)
{
    int32_t i, num;
    if (adapterName == NULL || capture == NULL) {
        HDF_LOGE("%{public}s: The pointer is null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].capturePid != pid) {
                HDF_LOGE("%{public}s: capturePid != pid ", __func__);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            *capture = g_renderAndCaptureManage[i].capture;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioAdapterFrameGetCapture(const char *adapterName,
    struct AudioCapture **capture, uint32_t pid, uint32_t *index)
{
    int32_t i, num;
    if (adapterName == NULL || capture == NULL || index == NULL || g_renderAndCaptureManage == NULL) {
        HDF_LOGE("%{public}s: The pointer is null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    *index = MAX_AUDIO_ADAPTER_NUM_SERVER;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].capturePid != pid) {
                HDF_LOGE("%{public}s: capturePid != pid ", __func__);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            if (!g_renderAndCaptureManage[i].captureDestory) {
                *capture = g_renderAndCaptureManage[i].capture;
                *index = i;
                return HDF_SUCCESS;
            } else {
                g_renderAndCaptureManage[i].captureBusy = false;
                return HDF_FAILURE;
            }
        }
    }
    return HDF_FAILURE;
}

int32_t AudioAdapterListCheckAndGetRender(struct AudioRender **render, struct HdfSBuf *data)
{
    if (render == NULL || data == NULL) {
        return HDF_FAILURE;
    }
    struct AudioRender *renderTemp = NULL;
    const char *adapterName = NULL;
    uint32_t pid = 0;
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        HDF_LOGE("%{public}s: HdiServiceRenderStart: HdiServiceRenderCaptureReadData fail ", __func__);
        return HDF_FAILURE;
    }
    int ret = AudioAdapterListGetRender(adapterName, &renderTemp, pid);
    if (ret < 0) {
        return ret;
    }
    if (renderTemp == NULL) {
        return HDF_FAILURE;
    }
    *render = renderTemp;
    return HDF_SUCCESS;
}

int32_t AudioAdapterListCheckAndGetCapture(struct AudioCapture **capture, struct HdfSBuf *data)
{
    if (capture == NULL || data == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *captureTemp = NULL;
    const char *adapterName = NULL;
    uint32_t pid = 0;
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        HDF_LOGE("%{public}s: HdiServiceCaptureStart: HdiServiceRenderCaptureReadData fail ", __func__);
        return HDF_FAILURE;
    }
    int ret = AudioAdapterListGetCapture(adapterName, &captureTemp, pid);
    if (ret < 0) {
        return ret;
    }
    if (captureTemp == NULL) {
        return HDF_FAILURE;
    }
    *capture = captureTemp;
    return HDF_SUCCESS;
}

int32_t AudioAdapterCheckListExist(const char *adapterName)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (adapterName == NULL) {
        HDF_LOGE("%{public}s: The pointer is null. ", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return AUDIO_HAL_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].adapterUserNum == 0) {
                return AUDIO_HAL_ERR_INTERNAL;
            } else if (g_renderAndCaptureManage[i].adapterUserNum > 0) {
                g_renderAndCaptureManage[i].adapterUserNum++;
                return AUDIO_HAL_SUCCESS;
            }
        }
    }
    return AUDIO_HAL_ERR_INVALID_PARAM;
}

int32_t AudioAdapterListDestory(const char *adapterName, struct AudioAdapter **adapter)
{
    int32_t i, num;
    if (adapter == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    LOG_FUN_INFO();
    if (adapterName == NULL) {
        HDF_LOGE("%{public}s: adapterName is NULL. ", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (g_renderAndCaptureManage == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return AUDIO_HAL_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].adapterUserNum == 1) {
                g_renderAndCaptureManage[i].adapterUserNum--;
                *adapter = g_renderAndCaptureManage[i].adapter;
                g_renderAndCaptureManage[i].adapter = NULL;
                return AUDIO_HAL_SUCCESS;
            } else if (g_renderAndCaptureManage[i].adapterUserNum > 1) {
                g_renderAndCaptureManage[i].adapterUserNum--;
                return AUDIO_HAL_ERR_INTERNAL;
            }
        }
    }
    return AUDIO_HAL_ERR_INVALID_PARAM;
}

int32_t AudioAdapterListAdd(const char *adapterName, struct AudioAdapter *adapter)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (adapterName == NULL || adapter == NULL) {
        HDF_LOGE("%{public}s: adapterName is NULL. ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].adapter = adapter;
            g_renderAndCaptureManage[i].adapterUserNum = 1;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

void AudioSetCaptureStatus(const char *adapterName, bool captureStatus)
{
    int32_t i, num;
    if (adapterName == NULL || g_renderAndCaptureManage == NULL) {
        return;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].captureBusy = captureStatus;
            return;
        }
    }
    return;
}

void AudioSetCaptureBusy(uint32_t index, bool captureStatus)
{
    if (index < MAX_AUDIO_ADAPTER_NUM_SERVER && g_renderAndCaptureManage != NULL) {
        g_renderAndCaptureManage[index].captureBusy = captureStatus;
    }
    return;
}

int32_t AudioGetCaptureStatus(const char *adapterName)
{
    int32_t i, num;
    if (adapterName == NULL || g_renderAndCaptureManage == NULL) {
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (!g_renderAndCaptureManage[i].captureDestory) {
                return HDF_SUCCESS;
            } else {
                g_renderAndCaptureManage[i].captureBusy = false;
                return HDF_FAILURE;
            }
        }
    }
    return HDF_FAILURE;
}

int32_t HdiServicePositionWrite(struct HdfSBuf *reply,
    uint64_t frames, struct AudioTimeStamp time)
{
    if (reply == NULL) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, frames)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteInt64(reply, time.tvSec)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteInt64(reply, time.tvNSec)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceReqMmapBuffer(struct AudioMmapBufferDescripter *desc, struct HdfSBuf *data)
{
    int32_t ret;
    if (desc == NULL || data == NULL) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    uint64_t memAddr = 0;
    if (!HdfSbufReadUint64(data, &memAddr)) {
        HDF_LOGE("%{public}s: memAddr Is NULL", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    desc->memoryAddress = (void *)(uintptr_t)memAddr;
    ret = HdfSbufReadFileDescriptor(data);
    if (ret < 0) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    desc->memoryFd = ret;
    if (!HdfSbufReadInt32(data, &desc->totalBufferFrames)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (!HdfSbufReadInt32(data, &desc->transferFrameSize)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (!HdfSbufReadInt32(data, &desc->isShareable)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (!HdfSbufReadUint32(data, &desc->offset)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

/**************************public************************/
int32_t HdiServiceGetFuncs()
{
    HDF_LOGI("%{public}s: enter", __func__);
    if (g_serverManager != NULL) {
        return AUDIO_HAL_SUCCESS;
    }
    g_serverManager = GetAudioManagerFuncs();
    if (g_serverManager == NULL) {
        HDF_LOGE("%{public}s: GetAudioManagerFuncs FAIL!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    HDF_LOGI("%{public}s: end", __func__);
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceGetAllAdapter(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGI("%{public}s: enter", __func__);
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    static bool getAdaptersFlag = true;
    struct AudioAdapterDescriptor *descs = NULL;
    struct AudioManager *manager = g_serverManager;
    int32_t size = 0;

    if (manager == NULL) {
        HDF_LOGE("%{public}s: Manager is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    int32_t ret = manager->GetAllAdapters(manager, &descs, &size);
    if (ret < 0) {
        HDF_LOGE("%{public}s: g_manager->GetAllAdapters error", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (size > MAX_AUDIO_ADAPTER_NUM_SERVER || size == 0 || descs == NULL || ret < 0) {
        HDF_LOGE("%{public}s: size or g_descs is error", __func__);
        return AUDIO_HAL_ERR_NOT_SUPPORT;
    }
    AudioSetFuzzCheckFlag(false);
    g_descs = descs;
    if (getAdaptersFlag) {  // Initialize only once
        ret = AdaptersServerManageInit(descs, size);
        if (ret != AUDIO_HAL_SUCCESS) {
            HDF_LOGE("%{public}s: AdapterServerManageInit fail", __func__);
            return ret;
        }
        getAdaptersFlag = false;
    }
    HDF_LOGI("%{public}s: end", __func__);

    return AUDIO_HAL_SUCCESS;
}

static int SwitchAdapter(struct AudioAdapterDescriptor *descs, const char *adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort *renderPort, const int size)
{
    if (descs == NULL || adapterNameCase == NULL || renderPort == NULL) {
        return HDF_FAILURE;
    }
    for (int index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == NULL) {
            continue;
        }
        if (desc->adapterName == NULL) {
            return HDF_FAILURE;
        }
        if (strcmp(desc->adapterName, adapterNameCase) != 0) {
            continue;
        }
        for (uint32_t port = 0; port < desc->portNum; port++) {
            if (desc->ports[port].dir == portFlag) {
                *renderPort = desc->ports[port];
                HDF_LOGI("%{public}s: port=%{public}d index=%{public}d success!", __func__, portFlag, index);
                return index;
            }
        }
    }
    HDF_LOGE("%{public}s: out!", __func__);
    return HDF_FAILURE;
}

/* Adapter Check */
static enum AudioServerType g_loadServerFlag = AUDIO_SERVER_BOTTOM;
enum AudioServerType AudioHdiGetLoadServerFlag(void)
{
    return g_loadServerFlag;
}

void AudioHdiSetLoadServerFlag(enum AudioServerType serverType)
{
    g_loadServerFlag = serverType;
}

void AudioHdiClearLoadServerFlag(void)
{
    g_loadServerFlag = AUDIO_SERVER_BOTTOM;
}

static int32_t MatchAppropriateAdapter(enum AudioAdapterType adapterType)
{
    switch (adapterType) {
        case AUDIO_ADAPTER_PRIMARY:
        case AUDIO_ADAPTER_PRIMARY_EXT:
            if (AudioHdiGetLoadServerFlag() != AUDIO_SERVER_PRIMARY) {
                LOGE("%s: Can't loadAdapterPrimary.", __func__);
                return AUDIO_HAL_ERR_INTERNAL;
            }
            break;
        case AUDIO_ADAPTER_USB:
            if (AudioHdiGetLoadServerFlag() != AUDIO_SERVER_USB) {
                LOGE("%s: Can't loadAdapterUsb.", __func__);
                return AUDIO_HAL_ERR_INTERNAL;
            }
            break;
        case AUDIO_ADAPTER_A2DP:
            if (AudioHdiGetLoadServerFlag() != AUDIO_SERVER_A2DP) {
                LOGE("%s: Can't loadAdapterA2dp.", __func__);
                return AUDIO_HAL_ERR_INTERNAL;
            }
            break;
        default:
            LOGE("%s: An unsupported Adapter.", __func__);
            return AUDIO_HAL_ERR_NOT_SUPPORT;
    }

    return AUDIO_HAL_SUCCESS;
}

static int AudioServiceUpateDevice(struct HdfDeviceObject *device, const char *servInfo)
{
    if (device == NULL || servInfo == NULL) {
        HDF_LOGE("%{public}s: device or servInfo is null!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (HdfDeviceObjectSetServInfo(device, servInfo) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: HdfDeviceObjectSetServInfo failed!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (HdfDeviceObjectUpdate(device) != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: HdfDeviceObjectUpdate failed!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }

    return AUDIO_HAL_SUCCESS;
}

int32_t AudioServiceStateChange(struct HdfDeviceObject *device, struct AudioEvent *audioSrvEvent)
{
    if (device == NULL || audioSrvEvent == NULL) {
        HDF_LOGE("%{public}s: device or audioSrvEvent is null!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    g_audioEventService.eventType = audioSrvEvent->eventType;
    g_audioEventService.deviceType = audioSrvEvent->deviceType;
    char strMsg[AUDIO_PNP_MSG_LEN_MAX] = {0};
    int ret = snprintf_s(strMsg, AUDIO_PNP_MSG_LEN_MAX, AUDIO_PNP_MSG_LEN_MAX - 1,
                         "EVENT_SERVICE_TYPE=0x%x;EVENT_LOAD_TYPE=0x%x;DEVICE_TYPE=0x%x",
                         g_audioEventService.eventType,
                         g_audioEventLoad.eventType,
                         g_audioEventService.deviceType);
    if (ret >= 0) {
        if (AudioServiceUpateDevice(device, (const char *)strMsg) != AUDIO_HAL_SUCCESS) {
            HDF_LOGE("%{public}s: AudioServiceUpate fail!", __func__);
            return AUDIO_HAL_ERR_INTERNAL;
        }
    }
    return AUDIO_HAL_SUCCESS;
}

static int32_t AudioLoadStateChange(struct HdfDeviceObject *device, struct AudioEvent *audioLoadEvent)
{
    if (device == NULL || audioLoadEvent == NULL) {
        HDF_LOGE("%{public}s: device or audioLoadEvent is null!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    g_audioEventLoad.eventType = audioLoadEvent->eventType;
    g_audioEventLoad.deviceType = audioLoadEvent->deviceType;
    char strMsg[AUDIO_PNP_MSG_LEN_MAX] = {0};
    int ret = snprintf_s(strMsg, AUDIO_PNP_MSG_LEN_MAX, AUDIO_PNP_MSG_LEN_MAX - 1,
                         "EVENT_SERVICE_TYPE=0x%x;EVENT_LOAD_TYPE=0x%x;DEVICE_TYPE=0x%x",
                         g_audioEventService.eventType,
                         g_audioEventLoad.eventType,
                         g_audioEventLoad.deviceType);
    if (ret >= 0) {
        if (AudioServiceUpateDevice(device, (const char *)strMsg) != AUDIO_HAL_SUCCESS) {
            HDF_LOGE("%{public}s: AudioLoadUpate fail!", __func__);
            return AUDIO_HAL_ERR_INTERNAL;
        }
    }
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceDevOnLine(struct HdfDeviceObject *device, struct AudioManager *manager,
    const struct AudioAdapterDescriptor *desc, struct AudioAdapter **adapter, const char* adapterName)
{
    if (device == NULL || manager == NULL || desc == NULL || adapter == NULL || adapterName == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    int32_t ret = manager->LoadAdapter(manager, desc, adapter);
    if (ret < 0) {
        g_audioEventLoad.eventType = HDF_AUDIO_LOAD_FAILURE;
    } else {
        g_audioEventLoad.eventType = HDF_AUDIO_LOAD_SUCCESS;
    }
    if (AudioLoadStateChange(device, &g_audioEventLoad) != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: AudioLoadStateChange fail!", __func__);
    }
    if (*adapter == NULL) {
        HDF_LOGE("%{public}s: load audio device failed", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (AudioAdapterListAdd(adapterName, *adapter)) {
        HDF_LOGE("%{public}s: AudioAdapterListAdd error!", __func__);
        manager->UnloadAdapter(manager, *adapter);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceDevOffLine(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    g_audioEventLoad.eventType = HDF_AUDIO_LOAD_FAILURE;
    if (AudioLoadStateChange(device, &g_audioEventLoad) != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: AudioLoadStateChange fail!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceLoadAdapterSubUsb(struct HdfDeviceObject *device, struct AudioManager *manager,
    const struct AudioAdapterDescriptor *desc, struct AudioAdapter **adapter, const char* adapterName)
{
    if (device == NULL || manager == NULL || desc == NULL || adapter == NULL || adapterName == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }

    if (g_audioEventPnp.eventType == HDF_AUDIO_DEVICE_REMOVE || g_audioEventPnp.eventType == HDF_AUDIO_EVENT_UNKOWN) {
        HdiServiceDevOffLine(device);
        return AUDIO_HAL_ERR_NOT_SUPPORT;
    } else if (g_audioEventPnp.eventType == HDF_AUDIO_DEVICE_ADD) {
        return HdiServiceDevOnLine(device, manager, desc, adapter, adapterName);
    } else {
        HDF_LOGE("%{public}s: eventType=0x%{public}x nothing", __func__, g_audioEventPnp.eventType);
        return AUDIO_HAL_ERR_INTERNAL;
    }
}

static int32_t HdiServiceLoadAdapterSub(struct HdfDeviceObject *device, struct AudioManager *manager,
    const struct AudioAdapterDescriptor *desc, struct AudioAdapter **adapter, const char* adapterName)
{
    HDF_LOGI("%{public}s: enter", __func__);
    if (device == NULL || manager == NULL || desc == NULL || adapter == NULL || adapterName == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    enum AudioAdapterType sndCardType = MatchAdapterType(adapterName, desc->ports[0].portId);
    int32_t ret = MatchAppropriateAdapter(sndCardType);
    if (ret != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: load audio device not matched", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    switch (sndCardType) {
        case AUDIO_ADAPTER_PRIMARY:
        case AUDIO_ADAPTER_PRIMARY_EXT:
            g_audioEventService.eventType = HDF_AUDIO_SERVICE_VALID;
            g_audioEventLoad.deviceType = HDF_AUDIO_PRIMARY_DEVICE;
            return HdiServiceDevOnLine(device, manager, desc, adapter, adapterName);
        case AUDIO_ADAPTER_USB:
            g_audioEventLoad.deviceType = HDF_AUDIO_USB_DEVICE;
            return HdiServiceLoadAdapterSubUsb(device, manager, desc, adapter, adapterName);
        case AUDIO_ADAPTER_A2DP:
            return AUDIO_HAL_ERR_NOT_SUPPORT;
        default:
            return AUDIO_HAL_ERR_NOT_SUPPORT;
    }
}

int32_t HdiServiceLoadAdapter(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGI("%{public}s: enter", __func__);
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    struct AudioAdapter *adapter = NULL;
    struct AudioPort renderPort;
    const char *adapterName = NULL;
    uint32_t tempDir = 0;
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    int32_t ret = AudioAdapterCheckListExist(adapterName);
    if (ret == AUDIO_HAL_ERR_INVALID_PARAM) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (ret == AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: adapte already exist !", __func__);
        return AUDIO_HAL_SUCCESS;
    }
    if (!HdfSbufReadUint32(data, &tempDir)) {
        HDF_LOGE("%{public}s: adapter need Load!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    enum AudioPortDirection port = (enum AudioPortDirection)tempDir;
    struct AudioManager *manager = g_serverManager;
    if (adapterName == NULL || manager == NULL || g_descs == NULL) {
        HDF_LOGE("%{public}s: Point is NULL!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    int index = SwitchAdapter(g_descs, adapterName, port,
        &renderPort, ServerManageGetAdapterNum());
    if (index < 0) {
        return AUDIO_HAL_ERR_NOT_SUPPORT;
    }
    struct AudioAdapterDescriptor *desc = &g_descs[index];
    ret = HdiServiceLoadAdapterSub(client->device, manager, desc, &adapter, adapterName);
    return ret;
}

int32_t HdiServiceInitAllPorts(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGI("%{public}s: enter!", __func__);
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    const char *adapterName = NULL;
    struct AudioAdapter *adapter = NULL;
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (AudioAdapterListGetAdapter(adapterName, &adapter)) {
        HDF_LOGE("%{public}s: AudioAdapterListGetAdapter fail", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (adapter->InitAllPorts(adapter)) {
        HDF_LOGE("%{public}s: InitAllPorts fail", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceUnloadAdapter(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    struct AudioAdapter *adapter = NULL;
    const char *adapterName = NULL;
    int ret;
    struct AudioManager *manager = g_serverManager;
    if (manager == NULL) {
        HDF_LOGE("%{public}s: Point is NULL!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    ret = AudioAdapterListDestory(adapterName, &adapter);
    if (ret == AUDIO_HAL_ERR_INTERNAL) {
        HDF_LOGI("%{public}s: Other dev Use the adapter", __func__);
        return AUDIO_HAL_SUCCESS;
    } else if (ret == AUDIO_HAL_ERR_INVALID_PARAM) {
        HDF_LOGE("%{public}s: param invalid!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (adapter == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    manager->UnloadAdapter(manager, adapter);
    g_audioEventLoad.eventType = HDF_AUDIO_UNLOAD;
    if (AudioLoadStateChange(client->device, &g_audioEventLoad) != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: AudioLoadStateChange fail!", __func__);
    }
    HDF_LOGI("%{public}s: Unload the adapter!", __func__);
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceGetPortCapability(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGE("%{public}s: enter", __func__);
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    struct AudioPort port;
    struct AudioPortCapability capability;
    struct AudioAdapter *adapter = NULL;
    const char *adapterName = NULL;
    uint32_t tempDir = 0;
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (!HdfSbufReadUint32(data, &tempDir)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    port.dir = (enum AudioPortDirection)tempDir;
    if (!HdfSbufReadUint32(data, &port.portId)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if ((port.portName = HdfSbufReadString(data)) == NULL) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    HDF_LOGD("port.portName = %{public}s", port.portName);
    if (AudioAdapterListGetAdapter(adapterName, &adapter)) {
        HDF_LOGE("%{public}s: AudioAdapterListGetAdapter fail", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (adapter == NULL) {
        HDF_LOGE("%{public}s: HdiServiceCreatRender adapter is NULL!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    int32_t ret = adapter->GetPortCapability(adapter, &port, &capability);
    if (ret < 0) {
        HDF_LOGE("%{public}s: ret failed", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

int32_t HdiServiceSetPassthroughMode(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    HDF_LOGE("%{public}s: enter", __func__);
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    struct AudioPort port;
    enum AudioPortPassthroughMode mode;
    struct AudioAdapter *adapter = NULL;
    const char *adapterName = NULL;
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    uint32_t tempDir = 0;
    if (!HdfSbufReadUint32(data, &tempDir)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    port.dir = (enum AudioPortDirection)tempDir;
    HDF_LOGE("port.dir = %{public}d", port.dir);
    if (!HdfSbufReadUint32(data, &port.portId)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if ((port.portName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("port.portName = %{public}s", port.portName);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    HDF_LOGD("port.portName = %{public}s", port.portName);
    uint32_t tempMode = 0;
    if (!HdfSbufReadUint32(data, &tempMode)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    mode = (enum AudioPortPassthroughMode)tempMode;
    HDF_LOGE("%{public}s: ready in,mode = %{public}d", __func__, mode);
    if (AudioAdapterListGetAdapter(adapterName, &adapter)) {
        HDF_LOGE("%{public}s: AudioAdapterListGetAdapter fail", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (adapter == NULL) {
        HDF_LOGE("%{public}s: HdiServiceCreatRender adapter is NULL!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    int ret = adapter->SetPassthroughMode(adapter, &port, mode);
    return ret;
}

int32_t HdiServiceGetPassthroughMode(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    enum AudioPortPassthroughMode mode = PORT_PASSTHROUGH_AUTO;
    struct AudioAdapter *adapter = NULL;
    const char *adapterName = NULL;
    struct AudioPort port;
    int32_t ret = memset_s(&port, sizeof(struct AudioPort), 0, sizeof(struct AudioPort));
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: memset_s failed", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: adapterNameCase Is NULL", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    uint32_t tempDir = port.dir;
    if (!HdfSbufReadUint32(data, &tempDir)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    port.dir = (enum AudioPortDirection)tempDir;
    if (!HdfSbufReadUint32(data, &port.portId)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if ((port.portName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("port.portName = %{public}s", port.portName);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (AudioAdapterListGetAdapter(adapterName, &adapter)) {
        HDF_LOGE("%{public}s: AudioAdapterListGetAdapter fail", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (adapter == NULL) {
        HDF_LOGE("%{public}s: adapter is NULL!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    ret = adapter->GetPassthroughMode(adapter, &port, &mode);
    if (ret < 0) {
        HDF_LOGE("%{public}s: GetPassthroughMode ret failed", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    uint32_t tempMode = (uint32_t)mode;
    if (!HdfSbufWriteUint32(reply, tempMode)) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

static int32_t HdiServiceGetDevStatusByPnp(const struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    (void)reply;
    const char *strDevPlugMsg = NULL;
    if (client == NULL || data == NULL) {
        HDF_LOGE("%{public}s: client || data is  null!", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if ((strDevPlugMsg = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s: data is null!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }

    if ((AudioPnpMsgDeSerialize(strDevPlugMsg, "EVENT_TYPE", &(g_audioEventPnp.eventType)) != HDF_SUCCESS) ||
        (AudioPnpMsgDeSerialize(strDevPlugMsg, "DEVICE_TYPE", &(g_audioEventPnp.deviceType)) != HDF_SUCCESS)) {
        HDF_LOGE("%{public}s: DeSerialize fail!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (g_audioEventPnp.deviceType == HDF_AUDIO_USB_HEADSET ||
        g_audioEventPnp.deviceType == HDF_AUDIO_USB_HEADPHONE ||
        g_audioEventPnp.deviceType == HDF_AUDIO_USBA_HEADSET ||
        g_audioEventPnp.deviceType == HDF_AUDIO_USBA_HEADPHONE) {
        g_audioEventService.deviceType = HDF_AUDIO_USB_DEVICE;
        if (g_audioEventPnp.eventType == HDF_AUDIO_DEVICE_ADD) {
            g_audioEventService.eventType = HDF_AUDIO_SERVICE_VALID;
        } else if (g_audioEventPnp.eventType == HDF_AUDIO_DEVICE_REMOVE) {
            g_audioEventService.eventType = HDF_AUDIO_SERVICE_INVALID;
        }
    }
    if (AudioServiceStateChange(client->device, &g_audioEventService) != AUDIO_HAL_SUCCESS) {
        HDF_LOGE("%{public}s: AudioServiceStateChange fail!", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    return AUDIO_HAL_SUCCESS;
}

struct HdiServiceDispatchCmdHandleList g_hdiServiceDispatchCmdHandleList[] = {
    {AUDIO_HDI_MGR_GET_FUNCS, HdiServiceGetFuncs},
    {AUDIO_HDI_MGR_GET_ALL_ADAPTER, HdiServiceGetAllAdapter},
    {AUDIO_HDI_MGR_LOAD_ADAPTER, HdiServiceLoadAdapter},
    {AUDIO_HDI_MGR_UNLOAD_ADAPTER, HdiServiceUnloadAdapter},
    {AUDIO_HDI_ADT_INIT_PORTS, HdiServiceInitAllPorts},
    {AUDIO_HDI_ADT_GET_PORT_CAPABILITY, HdiServiceGetPortCapability},
    {AUDIO_HDI_ADT_SET_PASS_MODE, HdiServiceSetPassthroughMode},
    {AUDIO_HDI_ADT_GET_PASS_MODE, HdiServiceGetPassthroughMode},
    {AUDIO_HDI_PNP_DEV_STATUS, HdiServiceGetDevStatusByPnp},
    {AUDIO_HDI_RENDER_CREATE_RENDER, HdiServiceCreatRender},
    {AUDIO_HDI_RENDER_DESTROY, HdiServiceRenderDestory},
    {AUDIO_HDI_RENDER_START, HdiServiceRenderStart},
    {AUDIO_HDI_RENDER_STOP, HdiServiceRenderStop},
    {AUDIO_HDI_RENDER_PAUSE, HdiServiceRenderPause},
    {AUDIO_HDI_RENDER_RESUME, HdiServiceRenderResume},
    {AUDIO_HDI_RENDER_FLUSH, HdiServiceRenderFlush},
    {AUDIO_HDI_RENDER_GET_FRAME_SIZE, HdiServiceRenderGetFrameSize},
    {AUDIO_HDI_RENDER_GET_FRAME_COUNT, HdiServiceRenderGetFrameCount},
    {AUDIO_HDI_RENDER_SET_SAMPLE_ATTR, HdiServiceRenderSetSampleAttr},
    {AUDIO_HDI_RENDER_GET_SAMPLE_ATTR, HdiServiceRenderGetSampleAttr},
    {AUDIO_HDI_RENDER_GET_CUR_CHANNEL_ID, HdiServiceRenderGetCurChannelId},
    {AUDIO_HDI_RENDER_CHECK_SCENE_CAPABILITY, HdiServiceRenderCheckSceneCapability},
    {AUDIO_HDI_RENDER_SELECT_SCENE, HdiServiceRenderSelectScene},
    {AUDIO_HDI_RENDER_GET_MUTE, HdiServiceRenderGetMute},
    {AUDIO_HDI_RENDER_SET_MUTE, HdiServiceRenderSetMute},
    {AUDIO_HDI_RENDER_SET_VOLUME, HdiServiceRenderSetVolume},
    {AUDIO_HDI_RENDER_GET_VOLUME, HdiServiceRenderGetVolume},
    {AUDIO_HDI_RENDER_GET_GAIN_THRESHOLD, HdiServiceRenderGetGainThreshold},
    {AUDIO_HDI_RENDER_GET_GAIN, HdiServiceRenderGetGain},
    {AUDIO_HDI_RENDER_SET_GAIN, HdiServiceRenderSetGain},
    {AUDIO_HDI_RENDER_GET_LATENCY, HdiServiceRenderGetLatency},
    {AUDIO_HDI_RENDER_RENDER_FRAME, HdiServiceRenderRenderFrame},
    {AUDIO_HDI_RENDER_GET_RENDER_POSITION, HdiServiceRenderGetRenderPosition},
    {AUDIO_HDI_RENDER_GET_SPEED, HdiServiceRenderGetSpeed},
    {AUDIO_HDI_RENDER_SET_SPEED, HdiServiceRenderSetSpeed},
    {AUDIO_HDI_RENDER_SET_CHANNEL_MODE, HdiServiceRenderSetChannelMode},
    {AUDIO_HDI_RENDER_GET_CHANNEL_MODE, HdiServiceRenderGetChannelMode},
    {AUDIO_HDI_RENDER_SET_EXTRA_PARAMS, HdiServiceRenderSetExtraParams},
    {AUDIO_HDI_RENDER_GET_EXTRA_PARAMS, HdiServiceRenderGetExtraParams},
    {AUDIO_HDI_RENDER_REQ_MMAP_BUFFER, HdiServiceRenderReqMmapBuffer},
    {AUDIO_HDI_RENDER_GET_MMAP_POSITION, HdiServiceRenderGetMmapPosition},
    {AUDIO_HDI_RENDER_TURN_STAND_BY_MODE, HdiServiceRenderTurnStandbyMode},
    {AUDIO_HDI_RENDER_DEV_DUMP, HdiServiceRenderDevDump},
    {AUDIO_HDI_RENDER_REG_CALLBACK, HdiServiceRenderRegCallback},
    {AUDIO_HDI_RENDER_DRAIN_BUFFER, HdiServiceRenderDrainBuffer},
};

static struct HdiServiceDispatchCmdHandleList g_hdiServiceDispatchCmdHandleCapList[] = {
    {AUDIO_HDI_CAPTURE_CREATE_CAPTURE, HdiServiceCreatCapture},
    {AUDIO_HDI_CAPTURE_DESTROY, HdiServiceCaptureDestory},
    {AUDIO_HDI_CAPTURE_START, HdiServiceCaptureStart},
    {AUDIO_HDI_CAPTURE_STOP, HdiServiceCaptureStop},
    {AUDIO_HDI_CAPTURE_PAUSE, HdiServiceCapturePause},
    {AUDIO_HDI_CAPTURE_RESUME, HdiServiceCaptureResume},
    {AUDIO_HDI_CAPTURE_FLUSH, HdiServiceCaptureFlush},
    {AUDIO_HDI_CAPTURE_GET_FRAME_SIZE, HdiServiceCaptureGetFrameSize},
    {AUDIO_HDI_CAPTURE_GET_FRAME_COUNT, HdiServiceCaptureGetFrameCount},
    {AUDIO_HDI_CAPTURE_SET_SAMPLE_ATTR, HdiServiceCaptureSetSampleAttr},
    {AUDIO_HDI_CAPTURE_GET_SAMPLE_ATTR, HdiServiceCaptureGetSampleAttr},
    {AUDIO_HDI_CAPTURE_GET_CUR_CHANNEL_ID, HdiServiceCaptureGetCurChannelId},
    {AUDIO_HDI_CAPTURE_CHECK_SCENE_CAPABILITY, HdiServiceCaptureCheckSceneCapability},
    {AUDIO_HDI_CAPTURE_SELECT_SCENE, HdiServiceCaptureSelectScene},
    {AUDIO_HDI_CAPTURE_GET_MUTE, HdiServiceCaptureGetMute},
    {AUDIO_HDI_CAPTURE_SET_MUTE, HdiServiceCaptureSetMute},
    {AUDIO_HDI_CAPTURE_SET_VOLUME, HdiServiceCaptureSetVolume},
    {AUDIO_HDI_CAPTURE_GET_VOLUME, HdiServiceCaptureGetVolume},
    {AUDIO_HDI_CAPTURE_GET_GAIN_THRESHOLD, HdiServiceCaptureGetGainThreshold},
    {AUDIO_HDI_CAPTURE_GET_GAIN, HdiServiceCaptureGetGain},
    {AUDIO_HDI_CAPTURE_SET_GAIN, HdiServiceCaptureSetGain},
    {AUDIO_HDI_CAPTURE_CAPTURE_FRAME, HdiServiceCaptureCaptureFrame},
    {AUDIO_HDI_CAPTURE_GET_CAPTURE_POSITION, HdiServiceCaptureGetCapturePosition},
    {AUDIO_HDI_CAPTURE_SET_EXTRA_PARAMS, HdiServiceCaptureSetExtraParams},
    {AUDIO_HDI_CAPTURE_GET_EXTRA_PARAMS, HdiServiceCaptureGetExtraParams},
    {AUDIO_HDI_CAPTURE_REQ_MMAP_BUFFER, HdiServiceCaptureReqMmapBuffer},
    {AUDIO_HDI_CAPTURE_GET_MMAP_POSITION, HdiServiceCaptureGetMmapPosition},
    {AUDIO_HDI_CAPTURE_TURN_STAND_BY_MODE, HdiServiceCaptureTurnStandbyMode},
    {AUDIO_HDI_CAPTURE_DEV_DUMP, HdiServiceCaptureDevDump},
};

int32_t HdiServiceDispatch(struct HdfDeviceIoClient *client, int cmdId, struct HdfSBuf *data,
    struct HdfSBuf *reply)
{
    unsigned int i;
    HDF_LOGD("%{public}s: valid cmdId = %{public}d", __func__, cmdId);
    if (client == NULL) {
        HDF_LOGE("%{public}s: ControlDispatch: input para is NULL.", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }

    if (!HdfDeviceObjectCheckInterfaceDesc(client->device, data)) {
        HDF_LOGE("check interface token failed");
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (cmdId > AUDIO_HDI_CAPTURE_DEV_DUMP || cmdId < 0) {
        HDF_LOGE("ControlDispatch: invalid cmdId = %{public}d", cmdId);
        return AUDIO_HAL_ERR_INTERNAL;
    } else if (cmdId <= AUDIO_HDI_RENDER_DRAIN_BUFFER) {
        for (i = 0; i < sizeof(g_hdiServiceDispatchCmdHandleList) /
            sizeof(g_hdiServiceDispatchCmdHandleList[0]); ++i) {
            if ((cmdId == (int)(g_hdiServiceDispatchCmdHandleList[i].cmd)) &&
                (g_hdiServiceDispatchCmdHandleList[i].func != NULL)) {
                return g_hdiServiceDispatchCmdHandleList[i].func(client, data, reply);
            }
        }
    } else {
        for (i = 0; i < sizeof(g_hdiServiceDispatchCmdHandleCapList) /
            sizeof(g_hdiServiceDispatchCmdHandleCapList[0]); ++i) {
            if ((cmdId == (int)(g_hdiServiceDispatchCmdHandleCapList[i].cmd)) &&
                (g_hdiServiceDispatchCmdHandleCapList[i].func != NULL)) {
                return g_hdiServiceDispatchCmdHandleCapList[i].func(client, data, reply);
            }
        }
    }
    return AUDIO_HAL_ERR_INTERNAL;
}
