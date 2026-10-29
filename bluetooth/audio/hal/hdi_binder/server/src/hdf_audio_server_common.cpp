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
#include <new>

namespace OHOS::HDI::Audio_Bluetooth {
const int  MANAGER_ADAPTER_NAME_LEN = 32;

int32_t g_serverAdapterNum = 0;
struct AudioInfoInAdapter *g_renderAndCaptureManage = nullptr;
#ifdef MULTI_A2DP_ENABLED
static std::mutex g_dualRenderMapMutex;
#endif

static int32_t AdapterManageInit(struct AudioInfoInAdapter *adapterManage,
    const char *adapterName)
{
    int32_t ret;

    if (adapterManage == nullptr || adapterName == nullptr) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return HDF_FAILURE;
    }

    adapterManage->adapterName = static_cast<char *>(calloc(1, MANAGER_ADAPTER_NAME_LEN));
    if (adapterManage->adapterName == nullptr) {
        HDF_LOGE("%{public}s: calloc adapter name failed!", __func__);

        return HDF_FAILURE;
    }

    ret = memcpy_s(static_cast<void *>(const_cast<char *>(adapterManage->adapterName)), MANAGER_ADAPTER_NAME_LEN,
        adapterName, MANAGER_ADAPTER_NAME_LEN);
    if (ret != EOK) {
        HDF_LOGE("%{public}s: memcpy adapter name fail!", __func__);
        AudioMemFree(reinterpret_cast<void **>(const_cast<char **>(&adapterManage->adapterName)));

        return HDF_FAILURE;
    }

    adapterManage->adapter = nullptr;
    adapterManage->adapterUserNum = 0;
    adapterManage->renderStatus = 0;
    adapterManage->renderPriority = -1;
    adapterManage->render = nullptr;
    adapterManage->renderBusy = false;
    adapterManage->renderDestroy = false;
    adapterManage->renderPid = 0;

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
    struct AudioInfoInAdapter *adaptersManage, int32_t num)
{
    int32_t i;

    if (adaptersManage == nullptr || num <= 0) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return;
    }

    num = (num > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : num;
    for (i = 0; i < num; i++) {
        if (adaptersManage[i].adapterName != nullptr) {
            AudioMemFree(reinterpret_cast<void **>(const_cast<char **>(&adaptersManage[i].adapterName)));
        }
    }
#ifdef MULTI_A2DP_ENABLED
    delete[] adaptersManage;
#else
    AudioMemFree(reinterpret_cast<void **>(&adaptersManage));
#endif
}

void AdaptersServerManageInfomationRecycle(void)
{
    AdaptersServerManageRelease(g_renderAndCaptureManage, g_serverAdapterNum);
    g_renderAndCaptureManage = nullptr;
    g_serverAdapterNum = 0;
}

int32_t AdaptersServerManageInit(const struct AudioAdapterDescriptor *descs, int32_t num)
{
    int32_t i, ret;
    struct AudioInfoInAdapter *adaptersManage = nullptr;

    if (descs == nullptr || num <= 0) {
        HDF_LOGE("%{public}s: Parameter error! ", __func__);

        return HDF_FAILURE;
    }

    num = (num > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : num;
#ifdef MULTI_A2DP_ENABLED
    adaptersManage = new (std::nothrow) AudioInfoInAdapter[num]();
#else
    adaptersManage = reinterpret_cast<struct AudioInfoInAdapter *>(calloc(1, num * sizeof(struct AudioInfoInAdapter)));
#endif
    if (adaptersManage == nullptr) {
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
    if (adapterName == nullptr || data == nullptr || pid == nullptr) {
        return HDF_FAILURE;
    }
    if ((*adapterName = HdfSbufReadString(data)) == nullptr) {
        HDF_LOGE("%{public}s: adapterName Is NULL ", __func__);
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, pid)) {
        HDF_LOGE("%{public}s: read buf fail ", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

#ifdef MULTI_A2DP_ENABLED
int32_t HdiServiceRenderCaptureReadDataByAddr(struct HdfSBuf *data,
    const char **adapterName, uint32_t *pid, const char **macAddress)
{
    if (adapterName == nullptr || data == nullptr || pid == nullptr) {
        return HDF_FAILURE;
    }
    if ((*adapterName = HdfSbufReadString(data)) == nullptr) {
        HDF_LOGE("%{public}s: adapterName Is NULL ", __func__);
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, pid)) {
        HDF_LOGE("%{public}s: read buf fail ", __func__);
        return HDF_FAILURE;
    }
    if ((*macAddress = HdfSbufReadString(data)) == nullptr) {
        HDF_LOGE("%{public}s: read macAddress fail", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
#endif

int32_t WriteAudioSampleAttributes(struct HdfSBuf *reply, const struct AudioSampleAttributes *attrs)
{
    if (reply == nullptr || attrs == nullptr) {
        return HDF_FAILURE;
    }
    uint32_t tempAttrParam = static_cast<uint32_t>(attrs->type);
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = static_cast<uint32_t>(attrs->interleaved);
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = static_cast<uint32_t>(attrs->format);
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
    tempAttrParam = static_cast<uint32_t>(attrs->isBigEndian);
    if (!HdfSbufWriteUint32(reply, tempAttrParam)) {
        return HDF_FAILURE;
    }
    tempAttrParam = static_cast<uint32_t>(attrs->isSignedData);
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
    if (data == nullptr || attrs == nullptr) {
        return HDF_FAILURE;
    }
    uint32_t tempAttrParam = 0;
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->type = static_cast<AudioCategory>(tempAttrParam);
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->interleaved = static_cast<bool>(tempAttrParam);
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->format = static_cast<AudioFormat>(tempAttrParam);
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
    attrs->isBigEndian = static_cast<bool>(tempAttrParam);
    if (!HdfSbufReadUint32(data, &tempAttrParam)) {
        return HDF_FAILURE;
    }
    attrs->isSignedData = static_cast<bool>(tempAttrParam);
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
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || adapter == nullptr) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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
    if (renderManage == nullptr || renderManage->adapter == nullptr || renderManage->render == nullptr) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    int count = 0;
    renderManage->renderDestroy = true;
    while (renderManage->renderBusy) {
        if (count > 1000) { // Less than 1000
            HDF_LOGE("%{public}s: , count = %{public}d", __func__, count);
            renderManage->renderDestroy = false;
            return AUDIO_HAL_ERR_AO_BUSY; // render is busy now
        }
        usleep(500); // sleep 500us
        count++;
    }
    renderManage->renderPid = 0;
    if (renderManage->adapter->DestroyRender(renderManage->adapter, renderManage->render)) {
        renderManage->renderDestroy = false;
        return HDF_FAILURE;
    }
    renderManage->render = nullptr;
    renderManage->renderStatus = 0;
    renderManage->renderBusy = false;
    renderManage->renderDestroy = false;
    renderManage->renderPriority = -1;
    return HDF_SUCCESS;
}

int32_t AudioDestroyFormerCapture(struct AudioInfoInAdapter *captureManage)
{
    LOG_FUN_INFO();
    if (captureManage == nullptr || captureManage->adapter == nullptr || captureManage->capture == nullptr) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    int count = 0;
    captureManage->captureDestroy = true;
    while (captureManage->captureBusy) {
        if (count > 1000) { // Less than 1000
            HDF_LOGE("%{public}s: , count = %{public}d", __func__, count);
            captureManage->captureDestroy = false;
            return AUDIO_HAL_ERR_AO_BUSY; // render is busy now
        }
        usleep(500); // sleep 500us
        count++;
    }
    captureManage->capturePid = 0;
    if (captureManage->adapter->DestroyCapture(captureManage->adapter, captureManage->capture)) {
        captureManage->captureDestroy = false;
        return HDF_FAILURE;
    }
    captureManage->capture = nullptr;
    captureManage->captureStatus = 0;
    captureManage->captureBusy = false;
    captureManage->captureDestroy = false;
    captureManage->capturePriority = -1;
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
    if (g_renderAndCaptureManage == nullptr) {
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

int32_t AudioJudgeCapturePriority(const int32_t priority, int which)
{
    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER)
        ? MAX_AUDIO_ADAPTER_NUM_SERVER
        : g_serverAdapterNum;
    if (which < 0 || which >= num) {
        HDF_LOGE("%{public}s: invalid value! ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage[which].capturePriority <= priority) {
        if (AudioDestroyFormerCapture(&g_renderAndCaptureManage[which])) {
            HDF_LOGE("%{public}s: AudioDestroyFormerCapture: Fail. ", __func__);
            return HDF_FAILURE;
        }
        return HDF_SUCCESS;
    } else {
        return AUDIO_HAL_ERR_AO_BUSY; // render is busy now
    }
    return HDF_FAILURE;
}

int32_t AudioCreateRenderCheck(const char *adapterName, const int32_t priority)
{
    int32_t i;

    LOG_FUN_INFO();
    if (adapterName == nullptr || g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }

    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER)
        ? MAX_AUDIO_ADAPTER_NUM_SERVER
        : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

#ifdef MULTI_A2DP_ENABLED
int32_t AudioCreateRenderCheckByAddr(const char *adapterName, const int32_t priority, const std::string &macAddress)
{
    int32_t i;

    LOG_FUN_INFO();
    if (adapterName == nullptr || g_renderAndCaptureManage == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: invalid param! adapterName null:%{public}d, manage null:%{public}d, "
            "macAddress empty:%{public}d",
            __func__, adapterName == nullptr, g_renderAndCaptureManage == nullptr, macAddress.empty());
        return HDF_FAILURE;
    }

    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER)
        ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            auto& renderMap = g_renderAndCaptureManage[i].renderMap;
            if (renderMap.find(macAddress) != renderMap.end()) {
                HDF_LOGE("%{public}s: render for mac already exists", __func__);
                return AUDIO_HAL_ERR_AO_BUSY;
            }
            if (renderMap.size() < MAX_MULTI_A2DP_RENDER) {
                return HDF_SUCCESS;
            }
            HDF_LOGE("%{public}s: multi A2DP render map full", __func__);
            return AUDIO_HAL_ERR_AO_BUSY;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}
#endif

int32_t AudioCreateCaptureCheck(const char *adapterName, const int32_t priority)
{
    int32_t i;
    LOG_FUN_INFO();
    if (adapterName == nullptr || g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER)
        ? MAX_AUDIO_ADAPTER_NUM_SERVER
        : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (!(g_renderAndCaptureManage[i].captureStatus)) {
                return HDF_SUCCESS;
            } else {
                return AudioJudgeCapturePriority(priority, i);
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

    if (adapterName == nullptr || adapter == nullptr || render == nullptr) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

#ifdef MULTI_A2DP_ENABLED
int32_t AudioAddRenderInfoInAdapterByAddr(const char *adapterName,
    const RenderEntry &renderEntry, const std::string &macAddress)
{
    int32_t i;
    int32_t num;

    if (adapterName == nullptr || renderEntry.render == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            auto& renderMap = g_renderAndCaptureManage[i].renderMap;
            if (renderMap.find(macAddress) != renderMap.end()) {
                HDF_LOGE("%{public}s: render for mac already exists", __func__);
                return HDF_FAILURE;
            }
            if (renderMap.size() >= MAX_MULTI_A2DP_RENDER) {
                HDF_LOGE("%{public}s: render map full", __func__);
                return HDF_FAILURE;
            }
            renderMap[macAddress] = renderEntry;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}
#endif

int32_t AudioAddCaptureInfoInAdapter(const char *adapterName, struct AudioCapture *capture,
    const struct AudioAdapter *adapter, const int32_t priority, uint32_t capturePid)
{
    int32_t i;
    int32_t num;
    if (adapterName == nullptr || adapter == nullptr || capture == nullptr) {
        HDF_LOGE("%{public}s: input para is NULL. ", __func__);
        return HDF_FAILURE;
    }
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].captureStatus = 1;
            g_renderAndCaptureManage[i].capturePriority = priority;
            g_renderAndCaptureManage[i].capture = capture;
            g_renderAndCaptureManage[i].capturePid = capturePid;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

void AudioSetRenderStatus(const char *adapterName, bool renderStatus)
{
    int32_t i, num;
    if (g_renderAndCaptureManage == nullptr) {
        return;
    }
    LOG_FUN_INFO();
    if (adapterName == nullptr) {
        HDF_LOGE("%{public}s: adapterName is null ", __func__);
        return;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

#ifdef MULTI_A2DP_ENABLED
void AudioSetRenderStatusByAddr(const char *adapterName, bool renderStatus, const std::string &macAddress)
{
    int32_t i;
    int32_t num;
    if (g_renderAndCaptureManage == nullptr) {
        return;
    }
    LOG_FUN_INFO();
    if (adapterName == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: adapterName or macAddress is null ", __func__);
        return;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            auto it = g_renderAndCaptureManage[i].renderMap.find(macAddress);
            if (it != g_renderAndCaptureManage[i].renderMap.end()) {
                it->second.renderBusy = renderStatus;
            } else {
                HDF_LOGE("%{public}s: render not found for mac", __func__);
            }
            return;
        }
    }
    HDF_LOGE("%{public}s: AudioDestroyRenderInfoInAdapter: Can not find Adapter! ", __func__);
    return;
}
#endif

int32_t AudioGetRenderStatus(const char *adapterName)
{
    int32_t i;
    int32_t num;
    LOG_FUN_INFO();
    if (adapterName == nullptr || g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum >
        MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (!g_renderAndCaptureManage[i].renderDestroy) {
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

#ifdef MULTI_A2DP_ENABLED
static int32_t GetMultiRenderStatusInAdapter(struct AudioInfoInAdapter *info,
    const std::string &macAddress)
{
    auto it = info->renderMap.find(macAddress);
    if (it == info->renderMap.end()) {
        return HDF_FAILURE;
    }
    if (!it->second.renderDestroy) {
        return HDF_SUCCESS;
    }
    it->second.renderBusy = false;
    return HDF_FAILURE;
}

int32_t AudioGetRenderStatusByAddr(const char *adapterName, const std::string &macAddress)
{
    int32_t i;
    int32_t num;
    LOG_FUN_INFO();
    if (adapterName == nullptr || g_renderAndCaptureManage == nullptr || macAddress.empty()) {
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum >
        MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            return GetMultiRenderStatusInAdapter(&g_renderAndCaptureManage[i], macAddress);
        }
    }
    HDF_LOGE("%{public}s: AudioDestroyRenderInfoInAdapter: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}
#endif

int32_t AudioDestroyRenderInfoInAdapter(const char *adapterName)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (adapterName == nullptr) {
        HDF_LOGE("%{public}s: adapterName is null ", __func__);
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].renderStatus = 0;
            g_renderAndCaptureManage[i].renderPriority = -1;
            g_renderAndCaptureManage[i].render = nullptr;
            g_renderAndCaptureManage[i].renderPid = 0;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}

#ifdef MULTI_A2DP_ENABLED
int32_t AudioDestroyRenderInfoInAdapterByAddr(const char *adapterName, const std::string &macAddress)
{
    int32_t i;
    int32_t num;

    LOG_FUN_INFO();
    if (adapterName == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: adapterName or macAddress is null ", __func__);
        return HDF_FAILURE;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            auto& renderMap = g_renderAndCaptureManage[i].renderMap;
            auto it = renderMap.find(macAddress);
            if (it != renderMap.end()) {
                renderMap.erase(it);
                return HDF_SUCCESS;
            }
            return HDF_FAILURE;
        }
    }
    HDF_LOGE("%{public}s: Can not find Adapter! ", __func__);
    return HDF_FAILURE;
}
#endif

int32_t AudioDestroyCaptureInfoInAdapter(const char *adapterName)
{
    int32_t i;
    int32_t num;
    LOG_FUN_INFO();
    if (adapterName == nullptr) {
        HDF_LOGE("%{public}s: adapterName is null ", __func__);
        return HDF_FAILURE;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_FAILURE;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_FAILURE;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            g_renderAndCaptureManage[i].captureStatus = 0;
            g_renderAndCaptureManage[i].capturePriority = -1;
            g_renderAndCaptureManage[i].capture = nullptr;
            g_renderAndCaptureManage[i].capturePid = 0;
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
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || pid == nullptr) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || adapter == nullptr || render == nullptr) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

#ifdef MULTI_A2DP_ENABLED
int32_t AudioAdapterListGetAdapterRenderByAddr(const char *adapterName,
    struct AudioAdapter **adapter, struct AudioRender **render, const std::string &macAddress)
{
    int32_t i;
    int32_t num;

    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || adapter == nullptr || render == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            *adapter = g_renderAndCaptureManage[i].adapter;
            auto it = g_renderAndCaptureManage[i].renderMap.find(macAddress);
            if (it != g_renderAndCaptureManage[i].renderMap.end()) {
                *render = it->second.render;
                return HDF_SUCCESS;
            }
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}
#endif

int32_t AudioAdapterListGetAdapterCapture(const char *adapterName,
    struct AudioAdapter **adapter, struct AudioCapture **capture)
{
    int32_t i;
    int32_t num;
    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || adapter == nullptr || capture == nullptr) {
        HDF_LOGE("%{public}s: The pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

int32_t AudioAdapterListGetRender(const char *adapterName, struct AudioRender **render, uint32_t pid)
{
    int32_t num;

    HDF_LOGD("%{public}s: enter ", __func__);
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || render == nullptr) {
        HDF_LOGE("%{public}s: pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (int32_t i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].renderPid != pid) {
                HDF_LOGE("%{public}s: renderPid != pid renderPid = %{public}u, pid = %{public}u",
                    __func__, g_renderAndCaptureManage[i].renderPid, pid);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            *render = g_renderAndCaptureManage[i].render;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

#ifdef MULTI_A2DP_ENABLED
int32_t AudioAdapterListGetRenderByAddr(const char *adapterName,
    struct AudioRender **render, uint32_t pid, const std::string &macAddress)
{
    int32_t num;

    HDF_LOGD("%{public}s: enter ", __func__);
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || render == nullptr || macAddress.empty()) {
        HDF_LOGE("%{public}s: pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (int32_t i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            std::lock_guard<std::mutex> lock(g_dualRenderMapMutex);
            auto it = g_renderAndCaptureManage[i].renderMap.find(macAddress);
            if (it != g_renderAndCaptureManage[i].renderMap.end()) {
                *render = it->second.render;
                return HDF_SUCCESS;
            }
            HDF_LOGE("%{public}s: render not found for mac", __func__);
            return AUDIO_HAL_ERR_INVALID_OBJECT;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}
#endif

int32_t AudioAdapterListGetCapture(const char *adapterName, struct AudioCapture **capture, uint32_t pid)
{
    int32_t num;
    HDF_LOGD("%{public}s: enter ", __func__);
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr || capture == nullptr) {
        HDF_LOGE("%{public}s: pointer is null ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (int32_t i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return HDF_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].capturePid != pid) {
                HDF_LOGE("%{public}s: renderPid != pid renderPid = %{public}u, pid = %{public}u",
                    __func__, g_renderAndCaptureManage[i].capturePid, pid);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            *capture = g_renderAndCaptureManage[i].capture;
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_INVALID_PARAM;
}

int32_t AudioAdapterFrameGetCapture(const char *adapterName, struct AudioCapture **capture, uint32_t pid,
    uint32_t *index)
{
    if (adapterName == nullptr || capture == nullptr || index == nullptr) {
        HDF_LOGE("adapterName or capture or index is null");
        return HDF_ERR_INVALID_PARAM;
    }
    int32_t num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER)
        ? MAX_AUDIO_ADAPTER_NUM_SERVER
        : g_serverAdapterNum;
    *index = MAX_AUDIO_ADAPTER_NUM_SERVER;
    for (int32_t i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            HDF_LOGE("g_renderAndCaptureManage[%{public}d] adapterName is null!", i);
            return HDF_ERR_INVALID_PARAM;
        }
        if (strcmp(g_renderAndCaptureManage[i].adapterName, adapterName) == 0) {
            if (g_renderAndCaptureManage[i].capturePid != pid) {
                HDF_LOGE("%{public}s: capturePid != pid capturePid = %{public}u, pid = %{public}u",
                    __func__, g_renderAndCaptureManage[i].capturePid, pid);
                return AUDIO_HAL_ERR_INVALID_OBJECT;
            }
            if (!g_renderAndCaptureManage[i].captureDestroy) {
                *capture = g_renderAndCaptureManage[i].capture;
                *index = i;
                return HDF_SUCCESS;
            } else {
                g_renderAndCaptureManage[i].captureBusy = false;
                HDF_LOGE("g_renderAndCaptureManage[%{public}d] captureBusy is false!", i);
                return HDF_FAILURE;
            }
        }
    }
    HDF_LOGE("AudioAdapterFrameGetCapture failed!");
    return HDF_FAILURE;
}

int32_t AudioAdapterListCheckAndGetRender(struct AudioRender **render, struct HdfSBuf *data)
{
    if (render == nullptr || data == nullptr) {
        return HDF_FAILURE;
    }
    struct AudioRender *renderTemp = nullptr;
    const char *adapterName = nullptr;
    uint32_t pid = 0;
#ifdef MULTI_A2DP_ENABLED
    const char *macAddress = nullptr;
    if (HdiServiceRenderCaptureReadDataByAddr(data, &adapterName, &pid, &macAddress) < 0) {
        HDF_LOGE("%{public}s: HdiServiceRenderStart: HdiServiceRenderCaptureReadDataByAddr fail ", __func__);
        return HDF_FAILURE;
    }
    int ret = AudioAdapterListGetRenderByAddr(adapterName, &renderTemp, pid, macAddress == nullptr ? "" : macAddress);
#else
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        HDF_LOGE("%{public}s: HdiServiceRenderStart: HdiServiceRenderCaptureReadData fail ", __func__);
        return HDF_FAILURE;
    }
    int ret = AudioAdapterListGetRender(adapterName, &renderTemp, pid);
#endif
    if (ret < 0) {
        return ret;
    }
    if (renderTemp == nullptr) {
        return HDF_FAILURE;
    }
    *render = renderTemp;
    return HDF_SUCCESS;
}

int32_t AudioAdapterListCheckAndGetCapture(struct AudioCapture **capture, struct HdfSBuf *data)
{
    if (capture == nullptr || data == nullptr) {
        return HDF_FAILURE;
    }
    struct AudioCapture *captureTemp = nullptr;
    const char *adapterName = nullptr;
    uint32_t pid = 0;
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        HDF_LOGE("%{public}s: HdiServiceRenderStart: HdiServiceRenderCaptureReadData fail ", __func__);
        return HDF_FAILURE;
    }
    int ret = AudioAdapterListGetCapture(adapterName, &captureTemp, pid);
    if (ret < 0) {
        return ret;
    }
    if (captureTemp == nullptr) {
        return HDF_FAILURE;
    }
    *capture = captureTemp;
    return HDF_SUCCESS;
}

int32_t AudioAdapterCheckListExist(const char *adapterName)
{
    int32_t i, num;

    LOG_FUN_INFO();
    if (g_renderAndCaptureManage == nullptr) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (adapterName == nullptr) {
        HDF_LOGE("%{public}s: The pointer is null. ", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }

    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

int32_t AudioAdapterListDestroy(const char *adapterName, struct AudioAdapter **adapter)
{
    int32_t i, num;
    if (adapter == nullptr) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    LOG_FUN_INFO();
    if (adapterName == nullptr) {
        HDF_LOGE("%{public}s: adapterName is NULL. ", __func__);
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (g_renderAndCaptureManage == nullptr) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ? MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
            return AUDIO_HAL_ERR_INVALID_PARAM;
        }
        if (!strcmp(g_renderAndCaptureManage[i].adapterName, adapterName)) {
            if (g_renderAndCaptureManage[i].adapterUserNum == 1) {
                g_renderAndCaptureManage[i].adapterUserNum--;
                *adapter = g_renderAndCaptureManage[i].adapter;
                g_renderAndCaptureManage[i].adapter = nullptr;
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
    if (adapterName == nullptr || adapter == nullptr) {
        HDF_LOGE("%{public}s: adapterName is NULL. ", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    num = (g_serverAdapterNum > MAX_AUDIO_ADAPTER_NUM_SERVER) ?
        MAX_AUDIO_ADAPTER_NUM_SERVER : g_serverAdapterNum;
    if (g_renderAndCaptureManage == nullptr) {
        return HDF_ERR_INVALID_PARAM;
    }
    for (i = 0; i < num; i++) {
        if (g_renderAndCaptureManage[i].adapterName == nullptr) {
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

int32_t HdiServicePositionWrite(struct HdfSBuf *reply,
    uint64_t frames, struct AudioTimeStamp time)
{
    if (reply == nullptr) {
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

int32_t HdiServiceReqMmapBuffer(struct AudioMmapBufferDescriptor *desc, struct HdfSBuf *data)
{
    int32_t ret;
    if (desc == nullptr || data == nullptr) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    uint64_t memAddr = 0;
    if (!HdfSbufReadUint64(data, &memAddr)) {
        HDF_LOGE("%{public}s: memAddr Is NULL", __func__);
        return AUDIO_HAL_ERR_INTERNAL;
    }
    desc->memoryAddress = reinterpret_cast<void *>((uintptr_t)memAddr);
    ret = HdfSbufReadFileDescriptor(data);
    if (ret < 0) {
        return HDF_FAILURE;
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

void AudioSetCaptureBusy(uint32_t index, bool captureStatus)
{
    if (index == MAX_AUDIO_ADAPTER_NUM_SERVER) {
        g_renderAndCaptureManage[index].captureBusy = captureStatus;
    }
}
}