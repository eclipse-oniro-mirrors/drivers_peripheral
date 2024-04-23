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

#include "audio_adapter_vdi.h"

#include <limits.h>
#include "osal_mem.h"
#include "securec.h"
#include <hdf_base.h>
#include "audio_uhdf_log.h"
#include "audio_capture_vdi.h"
#include "audio_common_vdi.h"
#include "audio_render_vdi.h"
#include "v3_0/iaudio_callback.h"

#define HDF_LOG_TAG    HDF_AUDIO_PRIMARY_IMPL

struct AudioAdapterInfo {
    struct IAudioAdapterVdi *vdiAdapter;
    struct IAudioAdapter *adapter;
    uint32_t refCnt;
};

struct AudioAdapterPrivVdi {
    struct AudioAdapterInfo adapterInfo[AUDIO_VDI_ADAPTER_NUM_MAX];
    struct IAudioCallback *callback;
    bool isRegCb;
};

static struct AudioAdapterPrivVdi g_audioAdapterVdi;

static struct AudioAdapterPrivVdi *AudioAdapterGetPrivVdi(void)
{
    return &g_audioAdapterVdi;
}

struct IAudioAdapterVdi *AudioGetVdiAdapterByDescIndexVdi(uint32_t descIndex)
{
    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();

    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("get vdiAdapter error, descIndex=%{public}d", descIndex);
        return NULL;
    }

    return priv->adapterInfo[descIndex].vdiAdapter;
}

static struct IAudioAdapterVdi *AudioGetVdiAdapterVdi(const struct IAudioAdapter *adapter)
{
    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();

    if (adapter == NULL) {
        AUDIO_FUNC_LOGE("get vdiAdapter error");
        return NULL;
    }

    for (uint32_t i = 0; i < AUDIO_VDI_ADAPTER_NUM_MAX; i++) {
        if (adapter == priv->adapterInfo[i].adapter) {
            return priv->adapterInfo[i].vdiAdapter;
        }
    }

    AUDIO_FUNC_LOGE("audio get vdiadapter fail");
    return NULL;
}

int32_t AudioInitAllPortsVdi(struct IAudioAdapter *adapter)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);

    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->InitAllPorts, HDF_ERR_INVALID_PARAM);
    int32_t ret = vdiAdapter->InitAllPorts(vdiAdapter);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter InitAllPorts fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioCreateRenderVdi(struct IAudioAdapter *adapter, const struct AudioDeviceDescriptor *desc,
    const struct AudioSampleAttributes *attrs, struct IAudioRender **render, uint32_t *renderId)
{
    AUDIO_FUNC_LOGD("enter to %{public}s", __func__);
    struct AudioDeviceDescriptorVdi vdiDesc;
    struct AudioSampleAttributesVdi vdiAttrs;
    struct IAudioRenderVdi *vdiRender = NULL;

    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(desc, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(attrs, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(render, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(renderId, HDF_ERR_INVALID_PARAM);
    CHECK_VALID_RANGE_RETURN(*renderId, 0, AUDIO_VDI_STREAM_NUM_MAX - 1, HDF_ERR_INVALID_PARAM);

    *render = FindRenderCreated(desc->pins, attrs, renderId);
    if (*render != NULL) {
        AUDIO_FUNC_LOGE("already created");
        return HDF_SUCCESS;
    }

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->CreateRender, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->DestroyRender, HDF_ERR_INVALID_PARAM);

    AudioCommonDevDescToVdiDevDescVdi(desc, &vdiDesc);
    AudioCommonAttrsToVdiAttrsVdi(attrs, &vdiAttrs);

    int32_t ret = vdiAdapter->CreateRender(vdiAdapter, &vdiDesc, &vdiAttrs, &vdiRender);
    OsalMemFree((void *)vdiDesc.desc);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call CreateRender fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }
    vdiRender->AddAudioEffect = NULL;
    vdiRender->RemoveAudioEffect = NULL;
    vdiRender->GetFrameBufferSize = NULL;
    vdiRender->IsSupportsPauseAndResume = NULL;
    *render = AudioCreateRenderByIdVdi(attrs, renderId, vdiRender, desc);
    if (*render == NULL) {
        (void)vdiAdapter->DestroyRender(vdiAdapter, vdiRender);
        AUDIO_FUNC_LOGE("Create audio render failed");
        return HDF_ERR_INVALID_PARAM;
    }
    AUDIO_FUNC_LOGI("AudioCreateRenderVdi Success");
    return HDF_SUCCESS;
}

int32_t AudioDestroyRenderVdi(struct IAudioAdapter *adapter, uint32_t renderId)
{
    AUDIO_FUNC_LOGD("enter to %{public}s", __func__);
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_VALID_RANGE_RETURN(renderId, 0, AUDIO_VDI_STREAM_NUM_MAX - 1, HDF_ERR_INVALID_PARAM);
    if (DecreaseRenderUsrCount(renderId) > 0) {
        AUDIO_FUNC_LOGE("render destroy: more than one usr");
        return HDF_SUCCESS;
    }
    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);

    struct IAudioRenderVdi *vdiRender = AudioGetVdiRenderByIdVdi(renderId);
    CHECK_NULL_PTR_RETURN_VALUE(vdiRender, HDF_ERR_INVALID_PARAM);

    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->DestroyRender, HDF_ERR_INVALID_PARAM);
    int32_t ret = vdiAdapter->DestroyRender(vdiAdapter, vdiRender);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call DestroyRender fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    AudioDestroyRenderByIdVdi(renderId);
    return HDF_SUCCESS;
}

int32_t AudioCreateCaptureVdi(struct IAudioAdapter *adapter, const struct AudioDeviceDescriptor *desc,
    const struct AudioSampleAttributes *attrs, struct IAudioCapture **capture, uint32_t *captureId)
{
    AUDIO_FUNC_LOGD("enter to %{public}s", __func__);
    struct IAudioCaptureVdi *vdiCapture = NULL;

    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(desc, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(attrs, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(capture, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(captureId, HDF_ERR_INVALID_PARAM);
    CHECK_VALID_RANGE_RETURN(*captureId, 0, AUDIO_VDI_STREAM_NUM_MAX - 1, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);

    struct AudioDeviceDescriptorVdi vdiDesc;
    struct AudioSampleAttributesVdi vdiAttrs;
    (void)memset_s((void *)&vdiDesc, sizeof(vdiDesc), 0, sizeof(vdiDesc));
    (void)memset_s((void *)&vdiAttrs, sizeof(vdiAttrs), 0, sizeof(vdiAttrs));
    AudioCommonDevDescToVdiDevDescVdi(desc, &vdiDesc);
    AudioCommonAttrsToVdiAttrsVdi(attrs, &vdiAttrs);

    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->CreateCapture, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->DestroyCapture, HDF_ERR_INVALID_PARAM);
    int32_t ret = vdiAdapter->CreateCapture(vdiAdapter, &vdiDesc, &vdiAttrs, &vdiCapture);
    OsalMemFree((void *)vdiDesc.desc);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call CreateCapture fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }
    vdiCapture->AddAudioEffect = NULL;
    vdiCapture->RemoveAudioEffect = NULL;
    vdiCapture->GetFrameBufferSize = NULL;
    vdiCapture->IsSupportsPauseAndResume = NULL;
    *capture = AudioCreateCaptureByIdVdi(attrs, captureId, vdiCapture, desc);
    if (*capture == NULL) {
        (void)vdiAdapter->DestroyCapture(vdiAdapter, vdiCapture);
        AUDIO_FUNC_LOGE("create audio capture failed");
        return HDF_ERR_INVALID_PARAM;
    }
    AUDIO_FUNC_LOGI("AudioCreateCaptureVdi Success");
    return HDF_SUCCESS;
}

int32_t AudioDestroyCaptureVdi(struct IAudioAdapter *adapter, uint32_t captureId)
{
    AUDIO_FUNC_LOGD("enter to %{public}s", __func__);
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_VALID_RANGE_RETURN(captureId, 0, AUDIO_VDI_STREAM_NUM_MAX - 1, HDF_ERR_INVALID_PARAM);
    if (DecreaseCaptureUsrCount(captureId) > 0) {
        AUDIO_FUNC_LOGE("capture destroy: more than one usr");
        return HDF_SUCCESS;
    }

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);

    struct IAudioCaptureVdi *vdiCapture = AudioGetVdiCaptureByIdVdi(captureId);
    CHECK_NULL_PTR_RETURN_VALUE(vdiCapture, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->DestroyCapture, HDF_ERR_INVALID_PARAM);
    int32_t ret = vdiAdapter->DestroyCapture(vdiAdapter, vdiCapture);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call DestroyCapture fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    AudioDestroyCaptureByIdVdi(captureId);
    return HDF_SUCCESS;
}

int32_t AudioGetPortCapabilityVdi(struct IAudioAdapter *adapter, const struct AudioPort *port,
    struct AudioPortCapability* capability)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(port, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(capability, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);

    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->GetPortCapability, HDF_ERR_INVALID_PARAM);
    struct AudioPortCapabilityVdi vdiCap;
    struct AudioPortVdi vdiPort;
    (void)memset_s(&vdiCap, sizeof(vdiCap), 0, sizeof(vdiCap));
    (void)memset_s(&vdiPort, sizeof(vdiPort), 0, sizeof(vdiPort));

    int32_t ret = AudioCommonPortToVdiPortVdi(port, &vdiPort);
    if (ret != HDF_SUCCESS) {
        OsalMemFree((void *)vdiPort.portName);
        AUDIO_FUNC_LOGE("audio vdiAdapter call PortCapToVdiPortCap fail, ret=%{public}d", ret);
        return ret;
    }

    ret = vdiAdapter->GetPortCapability(vdiAdapter, &vdiPort, &vdiCap);
    OsalMemFree((void *)vdiPort.portName);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call GetPortCapability fail, ret=%{public}d", ret);
        return ret;
    }

    AudioCommonVdiPortCapToPortCapVdi(&vdiCap, capability);
    return HDF_SUCCESS;
}

int32_t AudioSetPassthroughModeVdi(struct IAudioAdapter *adapter, const struct AudioPort *port,
    enum AudioPortPassthroughMode mode)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(port, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->SetPassthroughMode, HDF_ERR_INVALID_PARAM);

    struct AudioPortVdi vdiPort;
    (void)memset_s((void *)&vdiPort, sizeof(vdiPort), 0, sizeof(vdiPort));
    int32_t ret = AudioCommonPortToVdiPortVdi(port, &vdiPort);
    if (ret != HDF_SUCCESS) {
        OsalMemFree((void *)vdiPort.portName);
        AUDIO_FUNC_LOGE("audio vdiAdapter call PortCapToVdiPortCap fail, ret=%{public}d", ret);
        return ret;
    }

    ret = vdiAdapter->SetPassthroughMode(vdiAdapter, &vdiPort, (enum AudioPortPassthroughModeVdi)mode);
    OsalMemFree((void *)vdiPort.portName);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call SetPassthroughMode fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioGetPassthroughModeVdi(struct IAudioAdapter *adapter, const struct AudioPort *port,
    enum AudioPortPassthroughMode *mode)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(port, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(mode, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->GetPassthroughMode, HDF_ERR_INVALID_PARAM);

    struct AudioPortVdi vdiPort;
    (void)memset_s((void *)&vdiPort, sizeof(vdiPort), 0, sizeof(vdiPort));
    int32_t ret = AudioCommonPortToVdiPortVdi(port, &vdiPort);
    if (ret != HDF_SUCCESS) {
        OsalMemFree((void *)vdiPort.portName);
        AUDIO_FUNC_LOGE("audio vdiAdapter call PortCapToVdiPortCap fail, ret=%{public}d", ret);
        return ret;
    }

    ret = vdiAdapter->GetPassthroughMode(vdiAdapter, &vdiPort, (enum AudioPortPassthroughModeVdi *)mode);
    OsalMemFree((void *)vdiPort.portName);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call GetPassthroughMode fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioGetDeviceStatusVdi(struct IAudioAdapter *adapter, struct AudioDeviceStatus *status)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(status, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->GetDeviceStatus, HDF_ERR_INVALID_PARAM);

    struct AudioDeviceStatusVdi vdiStatus;
    (void)memset_s((void *)&vdiStatus, sizeof(vdiStatus), 0, sizeof(vdiStatus));
    int32_t ret = vdiAdapter->GetDeviceStatus(vdiAdapter, &vdiStatus);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call GetDeviceStatus fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    status->pnpStatus = vdiStatus.pnpStatus;
    return HDF_SUCCESS;
}

int32_t AudioUpdateAudioRouteVdi(struct IAudioAdapter *adapter, const struct AudioRoute *route, int32_t *routeHandle)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(route, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(routeHandle, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->UpdateAudioRoute, HDF_ERR_INVALID_PARAM);

    struct AudioRouteVdi vdiRoute;
    (void)memset_s(&vdiRoute, sizeof(vdiRoute), 0, sizeof(vdiRoute));

    int32_t ret = AudioCommonRouteToVdiRouteVdi(route, &vdiRoute);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter route To vdiRoute fail");
        return HDF_FAILURE;
    }

    ret = vdiAdapter->UpdateAudioRoute(vdiAdapter, &vdiRoute, routeHandle);
    AudioCommonFreeVdiRouteVdi(&vdiRoute);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call UpdateAudioRoute fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioReleaseAudioRouteVdi(struct IAudioAdapter *adapter, int32_t routeHandle)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->ReleaseAudioRoute, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->ReleaseAudioRoute(vdiAdapter, routeHandle);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call ReleaseAudioRoute fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioSetMicMuteVdi(struct IAudioAdapter *adapter, bool mute)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->SetMicMute, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->SetMicMute(vdiAdapter, mute);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call SetMicMute fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioGetMicMuteVdi(struct IAudioAdapter *adapter, bool *mute)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(mute, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->GetMicMute, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->GetMicMute(vdiAdapter, mute);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call GetMicMute fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioSetVoiceVolumeVdi(struct IAudioAdapter *adapter, float volume)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->SetVoiceVolume, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->SetVoiceVolume(vdiAdapter, volume);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call SetVoiceVolume fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioSetExtraParamsVdi(struct IAudioAdapter *adapter, enum AudioExtParamKey key, const char *condition,
    const char *value)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(condition, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(value, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->SetExtraParams, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->SetExtraParams(vdiAdapter, (enum AudioExtParamKeyVdi)key, condition, value);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call SetExtraParams fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t AudioGetExtraParamsVdi(struct IAudioAdapter *adapter, enum AudioExtParamKey key, const char *condition,
    char *value, uint32_t valueLen)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(condition, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(value, HDF_ERR_INVALID_PARAM);

    struct IAudioAdapterVdi *vdiAdapter = AudioGetVdiAdapterVdi(adapter);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter, HDF_ERR_INVALID_PARAM);
    CHECK_NULL_PTR_RETURN_VALUE(vdiAdapter->GetExtraParams, HDF_ERR_INVALID_PARAM);

    int32_t ret = vdiAdapter->GetExtraParams(vdiAdapter, (enum AudioExtParamKeyVdi)key, condition, value,
        (int32_t)valueLen);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("audio vdiAdapter call GetExtraParams fail, ret=%{public}d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void AudioInitAdapterInstanceVdi(struct IAudioAdapter *adapter)
{
    adapter->InitAllPorts = AudioInitAllPortsVdi;
    adapter->CreateRender = AudioCreateRenderVdi;
    adapter->DestroyRender = AudioDestroyRenderVdi;
    adapter->CreateCapture = AudioCreateCaptureVdi;
    adapter->DestroyCapture = AudioDestroyCaptureVdi;

    adapter->GetPortCapability = AudioGetPortCapabilityVdi;
    adapter->SetPassthroughMode = AudioSetPassthroughModeVdi;
    adapter->GetPassthroughMode = AudioGetPassthroughModeVdi;
    adapter->GetDeviceStatus = AudioGetDeviceStatusVdi;
    adapter->UpdateAudioRoute = AudioUpdateAudioRouteVdi;

    adapter->ReleaseAudioRoute = AudioReleaseAudioRouteVdi;
    adapter->SetMicMute = AudioSetMicMuteVdi;
    adapter->GetMicMute = AudioGetMicMuteVdi;
    adapter->SetVoiceVolume = AudioSetVoiceVolumeVdi;
    adapter->SetExtraParams = AudioSetExtraParamsVdi;

    adapter->GetExtraParams = AudioGetExtraParamsVdi;
}

uint32_t AudioGetAdapterRefCntVdi(uint32_t descIndex)
{
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("get adapter ref error, descIndex=%{public}d", descIndex);
        return UINT_MAX;
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();
    return priv->adapterInfo[descIndex].refCnt;
}

int32_t AudioIncreaseAdapterRefVdi(uint32_t descIndex, struct IAudioAdapter **adapter)
{
    CHECK_NULL_PTR_RETURN_VALUE(adapter, HDF_ERR_INVALID_PARAM);
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("increase adapter ref error, descIndex=%{public}d", descIndex);
        return HDF_ERR_INVALID_PARAM;
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();
    if (priv->adapterInfo[descIndex].adapter == NULL) {
        AUDIO_FUNC_LOGE("Invalid adapter param!");
        return HDF_ERR_INVALID_PARAM;
    }

    priv->adapterInfo[descIndex].refCnt++;
    *adapter = priv->adapterInfo[descIndex].adapter;
    AUDIO_FUNC_LOGI("increase adapternameIndex[%{public}d], refCount[%{public}d]", descIndex,
        priv->adapterInfo[descIndex].refCnt);

    return HDF_SUCCESS;
}

void AudioDecreaseAdapterRefVdi(uint32_t descIndex)
{
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("decrease adapter ref error, descIndex=%{public}d", descIndex);
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();
    if (priv->adapterInfo[descIndex].refCnt == 0) {
        AUDIO_FUNC_LOGE("Invalid adapterInfo[%{public}d] had released", descIndex);
        return;
    }
    priv->adapterInfo[descIndex].refCnt--;
    AUDIO_FUNC_LOGI("decrease adapternameIndex[%{public}d], refCount[%{public}d]", descIndex,
        priv->adapterInfo[descIndex].refCnt);
}

void AudioEnforceClearAdapterRefCntVdi(uint32_t descIndex)
{
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("decrease adapter descIndex error, descIndex=%{public}d", descIndex);
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();
    priv->adapterInfo[descIndex].refCnt = 0;
    AUDIO_FUNC_LOGI("clear adapter ref count zero");
}

struct IAudioAdapter *AudioCreateAdapterVdi(uint32_t descIndex, struct IAudioAdapterVdi *vdiAdapter)
{
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("create adapter error, descIndex=%{public}d", descIndex);
        return NULL;
    }

    if (vdiAdapter == NULL) {
        AUDIO_FUNC_LOGE("audio vdiAdapter is null");
        return NULL;
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();
    struct IAudioAdapter *adapter = priv->adapterInfo[descIndex].adapter;
    if (adapter != NULL) {
        return adapter;
    }

    adapter = (struct IAudioAdapter *)OsalMemCalloc(sizeof(struct IAudioAdapter));
    if (adapter == NULL) {
        AUDIO_FUNC_LOGE("OsalMemCalloc adapter fail");
        return NULL;
    }

    AudioInitAdapterInstanceVdi(adapter);
    priv->adapterInfo[descIndex].vdiAdapter = vdiAdapter;
    priv->adapterInfo[descIndex].adapter = adapter;
    priv->adapterInfo[descIndex].refCnt = 1;

    AUDIO_FUNC_LOGD(" audio vdiAdapter create adapter success, refcount[1]");
    return adapter;
}

void AudioReleaseAdapterVdi(uint32_t descIndex)
{
    if (descIndex >= AUDIO_VDI_ADAPTER_NUM_MAX) {
        AUDIO_FUNC_LOGE("adapter release fail descIndex=%{public}d", descIndex);
        return;
    }

    struct AudioAdapterPrivVdi *priv = AudioAdapterGetPrivVdi();

    OsalMemFree((void *)priv->adapterInfo[descIndex].adapter);
    priv->adapterInfo[descIndex].adapter = NULL;
    priv->adapterInfo[descIndex].vdiAdapter = NULL;
    priv->adapterInfo[descIndex].refCnt = UINT_MAX;

    priv->isRegCb = false;
    priv->callback = NULL;

    AUDIO_FUNC_LOGD(" audio vdiAdapter release adapter success");
}
