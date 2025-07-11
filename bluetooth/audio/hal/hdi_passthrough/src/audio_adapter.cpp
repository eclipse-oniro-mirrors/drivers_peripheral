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
#include <hdf_log.h>
#include "audio_internal.h"
#include "audio_adapter_info_common.h"
#include "audio_adapter.h"
#include "fast_audio_render.h"
namespace OHOS::HDI::Audio_Bluetooth {
constexpr int CONFIG_CHANNEL_COUNT = 2; // two channels
constexpr float GAIN_MAX = 50.0;

constexpr int DEFAULT_RENDER_SAMPLING_RATE = 48000;
constexpr int DEEP_BUFFER_RENDER_PERIOD_SIZE = 4096;
constexpr int DEEP_BUFFER_RENDER_PERIOD_COUNT = 8;
constexpr const char *TYPE_RENDER = "Render";
constexpr const char *TYPE_CAPTURE = "Capture";
constexpr const int SHIFT_RIGHT_31_BITS = 31;

static void GetFastRenderFuncs(struct AudioHwRender *hwRender)
{
    hwRender->common.control.Start = FastRenderStart;
    hwRender->common.control.Stop = FastRenderStop;
    hwRender->common.control.Pause = FastRenderPause;
    hwRender->common.control.Resume = FastRenderResume;
    hwRender->common.control.Flush = FastRenderFlush;
    hwRender->common.control.TurnStandbyMode = FastRenderTurnStandbyMode;
    hwRender->common.control.AudioDevDump = FastRenderAudioDevDump;
    hwRender->common.attr.GetFrameSize = FastRenderGetFrameSize;
    hwRender->common.attr.GetFrameCount = FastRenderGetFrameCount;
    hwRender->common.attr.SetSampleAttributes = FastRenderSetSampleAttributes;
    hwRender->common.attr.GetSampleAttributes = FastRenderGetSampleAttributes;
    hwRender->common.attr.GetCurrentChannelId = FastRenderGetCurrentChannelId;
    hwRender->common.attr.SetExtraParams = FastRenderSetExtraParams;
    hwRender->common.attr.GetExtraParams = FastRenderGetExtraParams;
    hwRender->common.attr.ReqMmapBuffer = FastRenderReqMmapBuffer;
    hwRender->common.attr.GetMmapPosition = FastRenderGetMmapPosition;
    hwRender->common.scene.CheckSceneCapability = FastRenderCheckSceneCapability;
    hwRender->common.scene.SelectScene = FastRenderSelectScene;
    hwRender->common.volume.SetMute = FastRenderSetMute;
    hwRender->common.volume.GetMute = FastRenderGetMute;
    hwRender->common.volume.SetVolume = FastRenderSetVolume;
    hwRender->common.volume.GetVolume = FastRenderGetVolume;
    hwRender->common.volume.GetGainThreshold = FastRenderGetGainThreshold;
    hwRender->common.volume.GetGain = FastRenderGetGain;
    hwRender->common.volume.SetGain = FastRenderSetGain;
    hwRender->common.GetLatency = FastRenderGetLatency;
    hwRender->common.RenderFrame = FastRenderRenderFrame;
    hwRender->common.GetRenderPosition = FastRenderGetRenderPosition;
    hwRender->common.SetRenderSpeed = FastRenderSetRenderSpeed;
    hwRender->common.GetRenderSpeed = FastRenderGetRenderSpeed;
    hwRender->common.SetChannelMode = FastRenderSetChannelMode;
    hwRender->common.GetChannelMode = FastRenderGetChannelMode;
    hwRender->common.RegCallback = FastRenderRegCallback;
    hwRender->common.DrainBuffer = FastRenderDrainBuffer;
}

static void GetNormalRenderFuncs(struct AudioHwRender *hwRender)
{
    hwRender->common.control.Start = AudioRenderStart;
    hwRender->common.control.Stop = AudioRenderStop;
    hwRender->common.control.Pause = AudioRenderPause;
    hwRender->common.control.Resume = AudioRenderResume;
    hwRender->common.control.Flush = AudioRenderFlush;
    hwRender->common.control.TurnStandbyMode = AudioRenderTurnStandbyMode;
    hwRender->common.control.AudioDevDump = AudioRenderAudioDevDump;
    hwRender->common.attr.GetFrameSize = AudioRenderGetFrameSize;
    hwRender->common.attr.GetFrameCount = AudioRenderGetFrameCount;
    hwRender->common.attr.SetSampleAttributes = AudioRenderSetSampleAttributes;
    hwRender->common.attr.GetSampleAttributes = AudioRenderGetSampleAttributes;
    hwRender->common.attr.GetCurrentChannelId = AudioRenderGetCurrentChannelId;
    hwRender->common.attr.SetExtraParams = AudioRenderSetExtraParams;
    hwRender->common.attr.GetExtraParams = AudioRenderGetExtraParams;
    hwRender->common.attr.ReqMmapBuffer = AudioRenderReqMmapBuffer;
    hwRender->common.attr.GetMmapPosition = AudioRenderGetMmapPosition;
    hwRender->common.scene.CheckSceneCapability = AudioRenderCheckSceneCapability;
    hwRender->common.scene.SelectScene = AudioRenderSelectScene;
    hwRender->common.volume.SetMute = AudioRenderSetMute;
    hwRender->common.volume.GetMute = AudioRenderGetMute;
    hwRender->common.volume.SetVolume = AudioRenderSetVolume;
    hwRender->common.volume.GetVolume = AudioRenderGetVolume;
    hwRender->common.volume.GetGainThreshold = AudioRenderGetGainThreshold;
    hwRender->common.volume.GetGain = AudioRenderGetGain;
    hwRender->common.volume.SetGain = AudioRenderSetGain;
    hwRender->common.GetLatency = AudioRenderGetLatency;
    hwRender->common.RenderFrame = AudioRenderRenderFrame;
    hwRender->common.GetRenderPosition = AudioRenderGetRenderPosition;
    hwRender->common.SetRenderSpeed = AudioRenderSetRenderSpeed;
    hwRender->common.GetRenderSpeed = AudioRenderGetRenderSpeed;
    hwRender->common.SetChannelMode = AudioRenderSetChannelMode;
    hwRender->common.GetChannelMode = AudioRenderGetChannelMode;
    hwRender->common.RegCallback = AudioRenderRegCallback;
    hwRender->common.DrainBuffer = AudioRenderDrainBuffer;
}

static void GetHearingAidFuncs(struct AudioHwRender *hwRender)
{
    hwRender->common.control.Start = HearingAidStart;
    hwRender->common.control.Stop = HearingAidStop;
    hwRender->common.control.Pause = HearingAidPause;
    hwRender->common.control.Resume = HearingAidResume;
    hwRender->common.control.Flush = HearingAidFlush;
    hwRender->common.control.TurnStandbyMode = HearingAidTurnStandbyMode;
    hwRender->common.control.AudioDevDump = HearingAidAudioDevDump;
    hwRender->common.attr.GetFrameSize = HearingAidGetFrameSize;
    hwRender->common.attr.GetFrameCount = HearingAidGetFrameCount;
    hwRender->common.attr.SetSampleAttributes = HearingAidSetSampleAttributes;
    hwRender->common.attr.GetSampleAttributes = HearingAidGetSampleAttributes;
    hwRender->common.attr.GetCurrentChannelId = HearingAidGetCurrentChannelId;
    hwRender->common.attr.SetExtraParams = HearingAidSetExtraParams;
    hwRender->common.attr.GetExtraParams = HearingAidGetExtraParams;
    hwRender->common.attr.ReqMmapBuffer = HearingAidReqMmapBuffer;
    hwRender->common.attr.GetMmapPosition = HearingAidGetMmapPosition;
    hwRender->common.scene.CheckSceneCapability = HearingAidCheckSceneCapability;
    hwRender->common.scene.SelectScene = HearingAidSelectScene;
    hwRender->common.volume.SetMute = HearingAidSetMute;
    hwRender->common.volume.GetMute = HearingAidGetMute;
    hwRender->common.volume.SetVolume = HearingAidSetVolume;
    hwRender->common.volume.GetVolume = HearingAidGetVolume;
    hwRender->common.volume.GetGainThreshold = HearingAidGetGainThreshold;
    hwRender->common.volume.GetGain = HearingAidGetGain;
    hwRender->common.volume.SetGain = HearingAidSetGain;
    hwRender->common.GetLatency = HearingAidGetLatency;
    hwRender->common.RenderFrame = HearingAidRenderFrame;
    hwRender->common.GetRenderPosition = HearingAidGetRenderPosition;
    hwRender->common.SetRenderSpeed = HearingAidSetRenderSpeed;
    hwRender->common.GetRenderSpeed = HearingAidGetRenderSpeed;
    hwRender->common.SetChannelMode = HearingAidSetChannelMode;
    hwRender->common.GetChannelMode = HearingAidGetChannelMode;
    hwRender->common.RegCallback = HearingAidRegCallback;
    hwRender->common.DrainBuffer = HearingAidDrainBuffer;
}

static void GetCaptureFuncs(struct AudioHwCapture *hwCapture)
{
    hwCapture->common.control.Start = AudioCaptureStart;
    hwCapture->common.control.Stop = AudioCaptureStop;
    hwCapture->common.control.Pause = AudioCapturePause;
    hwCapture->common.control.Resume = AudioCaptureResume;
    hwCapture->common.control.Flush = AudioCaptureFlush;
    hwCapture->common.volume.SetMute = AudioCaptureSetMute;
    hwCapture->common.volume.GetMute = AudioCaptureGetMute;
    hwCapture->common.CaptureFrame = AudioCaptureCaptureFrame;
}

int32_t GetAudioRenderFunc(struct AudioHwRender *hwRender, const char *adapterName)
{
    if (hwRender == nullptr || adapterName == nullptr) {
        return HDF_FAILURE;
    }
    if (strcmp(adapterName, "bt_a2dp_fast") == 0) {
        GetFastRenderFuncs(hwRender);
    } else if (strcmp(adapterName, "bt_hearing_aid") == 0) {
        GetHearingAidFuncs(hwRender);
    } else {
        GetNormalRenderFuncs(hwRender);
    }
    return HDF_SUCCESS;
}

int32_t GetAudioCaptureFunc(struct AudioHwCapture *hwCapture, const char *adapterName)
{
    if (hwCapture == nullptr || adapterName == nullptr) {
        return HDF_FAILURE;
    }
    GetCaptureFuncs(hwCapture);
    return HDF_SUCCESS;
}

int32_t CheckParaDesc(const struct AudioDeviceDescriptor *desc, const char *type)
{
    if (desc == NULL || type == NULL) {
        return HDF_FAILURE;
    }
    if ((desc->portId) >> SHIFT_RIGHT_31_BITS) {
        return HDF_ERR_NOT_SUPPORT;
    }
    AudioPortPin pins = desc->pins;
    if (!strcmp(type, TYPE_CAPTURE)) {
        if (pins == PIN_IN_MIC || pins == PIN_IN_HS_MIC || pins == PIN_IN_LINEIN) {
            return HDF_SUCCESS;
        } else {
            return HDF_ERR_NOT_SUPPORT;
        }
    } else if (!strcmp(type, TYPE_RENDER)) {
        if (pins == PIN_OUT_SPEAKER || pins == PIN_OUT_HEADSET || pins == PIN_OUT_LINEOUT || pins == PIN_OUT_HDMI) {
            return HDF_SUCCESS;
        } else {
            return HDF_ERR_NOT_SUPPORT;
        }
    }
    return HDF_ERR_NOT_SUPPORT;
}

int32_t CheckParaAttr(const struct AudioSampleAttributes *attrs)
{
    if (attrs == NULL) {
        return HDF_FAILURE;
    }
    int32_t ret = ((attrs->sampleRate) >> SHIFT_RIGHT_31_BITS) + ((attrs->channelCount) >> SHIFT_RIGHT_31_BITS) +
        ((attrs->period) >> SHIFT_RIGHT_31_BITS) + ((attrs->frameSize) >> SHIFT_RIGHT_31_BITS) +
        ((attrs->startThreshold) >> SHIFT_RIGHT_31_BITS) + ((attrs->stopThreshold) >> SHIFT_RIGHT_31_BITS) +
        ((attrs->silenceThreshold) >> SHIFT_RIGHT_31_BITS);
    if (ret > 0) {
        return HDF_ERR_NOT_SUPPORT;
    }
    AudioCategory audioCategory = attrs->type;
    if (audioCategory < AUDIO_IN_MEDIA || audioCategory > AUDIO_MMAP_NOIRQ) {
        return HDF_ERR_NOT_SUPPORT;
    }
    AudioFormat audioFormat = attrs->format;
    return CheckAttrFormat(audioFormat);
}

int32_t AttrFormatToBit(const struct AudioSampleAttributes *attrs, int32_t *format)
{
    if (attrs == NULL || format == NULL) {
        return HDF_FAILURE;
    }
    AudioFormat audioFormat = attrs->format;
    switch (audioFormat) {
        case AUDIO_FORMAT_TYPE_PCM_8_BIT:
            *format = BIT_NUM_8;
            return HDF_SUCCESS;
        case AUDIO_FORMAT_TYPE_PCM_16_BIT:
            *format = BIT_NUM_16;
            return HDF_SUCCESS;
        case AUDIO_FORMAT_TYPE_PCM_24_BIT:
            *format = BIT_NUM_24;
            return HDF_SUCCESS;
        case AUDIO_FORMAT_TYPE_PCM_32_BIT:
            *format = BIT_NUM_32;
            return HDF_SUCCESS;
        default:
            return HDF_ERR_NOT_SUPPORT;
    }
}

int32_t InitHwRenderParam(struct AudioHwRender *hwRender, const struct AudioDeviceDescriptor *desc,
                          const struct AudioSampleAttributes *attrs)
{
    if (hwRender == NULL || desc == NULL || attrs == NULL) {
        HDF_LOGE("InitHwRenderParam param Is NULL");
        return HDF_FAILURE;
    }
    int32_t ret = CheckParaDesc(desc, TYPE_RENDER);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("CheckParaDesc Fail");
        return ret;
    }
    ret = CheckParaAttr(attrs);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("CheckParaAttr Fail");
        return ret;
    }
    int32_t formatValue = -1;
    ret = AttrFormatToBit(attrs, &formatValue);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("AttrFormatToBit Fail");
        return ret;
    }
    if (attrs->channelCount == 0) {
        return HDF_FAILURE;
    }
    hwRender->renderParam.renderMode.hwInfo.deviceDescript = *desc;
    hwRender->renderParam.frameRenderMode.attrs = *attrs;
    hwRender->renderParam.renderMode.ctlParam.audioGain.gainMax = GAIN_MAX;  // init gainMax
    hwRender->renderParam.renderMode.ctlParam.audioGain.gainMin = 0;
    hwRender->renderParam.frameRenderMode.frames = 0;
    hwRender->renderParam.frameRenderMode.time.tvNSec = 0;
    hwRender->renderParam.frameRenderMode.time.tvSec = 0;
    hwRender->renderParam.frameRenderMode.byteRate = DEFAULT_RENDER_SAMPLING_RATE;
    hwRender->renderParam.frameRenderMode.periodSize = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    hwRender->renderParam.frameRenderMode.periodCount = DEEP_BUFFER_RENDER_PERIOD_COUNT;
    hwRender->renderParam.frameRenderMode.attrs.period = attrs->period;
    hwRender->renderParam.frameRenderMode.attrs.frameSize = attrs->frameSize;
    hwRender->renderParam.frameRenderMode.attrs.startThreshold = attrs->startThreshold;
    hwRender->renderParam.frameRenderMode.attrs.stopThreshold = attrs->stopThreshold;
    hwRender->renderParam.frameRenderMode.attrs.silenceThreshold = attrs->silenceThreshold;
    hwRender->renderParam.frameRenderMode.attrs.isBigEndian = attrs->isBigEndian;
    hwRender->renderParam.frameRenderMode.attrs.isSignedData = attrs->isSignedData;
    return HDF_SUCCESS;
}

int32_t InitHwCaptureParam(struct AudioHwCapture *hwCapture, const struct AudioDeviceDescriptor *desc,
    const struct AudioSampleAttributes *attrs)
{
    if (hwCapture == nullptr || desc == nullptr || attrs == nullptr) {
        HDF_LOGE("InitHwCaptureParam param Is NULL");
        return HDF_FAILURE;
    }
    int32_t ret = CheckParaDesc(desc, TYPE_CAPTURE);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("CheckParaDesc Fail");
        return ret;
    }
    ret = CheckParaAttr(attrs);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("CheckParaAttr Fail");
        return ret;
    }
    int32_t formatValue = -1;
    ret = AttrFormatToBit(attrs, &formatValue);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("AttrFormatToBit Fail");
        return ret;
    }
    if (attrs->channelCount == 0) {
        return HDF_FAILURE;
    }
    hwCapture->captureParam.captureMode.hwInfo.deviceDescript = *desc;
    hwCapture->captureParam.frameCaptureMode.attrs = *attrs;
    hwCapture->captureParam.captureMode.ctlParam.audioGain.gainMax = GAIN_MAX;  // init gainMax
    hwCapture->captureParam.captureMode.ctlParam.audioGain.gainMin = 0;
    hwCapture->captureParam.frameCaptureMode.frames = 0;
    hwCapture->captureParam.frameCaptureMode.time.tvNSec = 0;
    hwCapture->captureParam.frameCaptureMode.time.tvSec = 0;
    hwCapture->captureParam.frameCaptureMode.byteRate = DEFAULT_RENDER_SAMPLING_RATE;
    hwCapture->captureParam.frameCaptureMode.periodSize = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    hwCapture->captureParam.frameCaptureMode.periodCount = DEEP_BUFFER_RENDER_PERIOD_COUNT;
    hwCapture->captureParam.frameCaptureMode.attrs.period = attrs->period;
    hwCapture->captureParam.frameCaptureMode.attrs.frameSize = attrs->frameSize;
    hwCapture->captureParam.frameCaptureMode.attrs.startThreshold = attrs->startThreshold;
    hwCapture->captureParam.frameCaptureMode.attrs.stopThreshold = attrs->stopThreshold;
    hwCapture->captureParam.frameCaptureMode.attrs.silenceThreshold = attrs->silenceThreshold;
    hwCapture->captureParam.frameCaptureMode.attrs.isBigEndian = attrs->isBigEndian;
    hwCapture->captureParam.frameCaptureMode.attrs.isSignedData = attrs->isSignedData;
    return HDF_SUCCESS;
}

AudioFormat g_formatIdZero = AUDIO_FORMAT_TYPE_PCM_16_BIT;
int32_t InitForGetPortCapability(struct AudioPort portIndex, struct AudioPortCapability *capabilityIndex)
{
    if (capabilityIndex == NULL) {
        HDF_LOGE("capabilityIndex Is NULL");
        return HDF_FAILURE;
    }
    /* get capabilityIndex from driver or default */
    if (portIndex.dir != PORT_OUT) {
        capabilityIndex->hardwareMode = true;
        capabilityIndex->channelMasks = AUDIO_CHANNEL_STEREO;
        capabilityIndex->channelCount = CONFIG_CHANNEL_COUNT;
        return HDF_SUCCESS;
    }
    if (portIndex.portId == 0) {
        capabilityIndex->hardwareMode = true;
        capabilityIndex->channelMasks = AUDIO_CHANNEL_STEREO;
        capabilityIndex->channelCount = CONFIG_CHANNEL_COUNT;
        capabilityIndex->deviceType = portIndex.dir;
        capabilityIndex->deviceId = PIN_OUT_SPEAKER;
        capabilityIndex->formatNum = 1;
        capabilityIndex->formats = &g_formatIdZero;
        capabilityIndex->sampleRateMasks = AUDIO_SAMPLE_RATE_MASK_16000;
        capabilityIndex->subPortsNum = 1;
        capabilityIndex->subPorts =
            reinterpret_cast<struct AudioSubPortCapability *>(calloc(capabilityIndex->subPortsNum,
            sizeof(struct AudioSubPortCapability)));
        if (capabilityIndex->subPorts == NULL) {
            HDF_LOGE("capabilityIndex subPorts is NULL!");
            return HDF_FAILURE;
        }
        capabilityIndex->subPorts->portId = portIndex.portId;
        capabilityIndex->subPorts->desc = portIndex.portName;
        capabilityIndex->subPorts->mask = PORT_PASSTHROUGH_LPCM;
        return HDF_SUCCESS;
    }
    if (portIndex.portId == 1) {
        capabilityIndex->hardwareMode = true;
        capabilityIndex->channelMasks = AUDIO_CHANNEL_STEREO;
        capabilityIndex->channelCount = CONFIG_CHANNEL_COUNT;
        capabilityIndex->deviceType = portIndex.dir;
        capabilityIndex->deviceId = PIN_OUT_HEADSET;
        capabilityIndex->formatNum = 1;
        capabilityIndex->formats = &g_formatIdZero;
        capabilityIndex->sampleRateMasks = AUDIO_SAMPLE_RATE_MASK_16000 | AUDIO_SAMPLE_RATE_MASK_8000;
        return HDF_SUCCESS;
    }
    if (portIndex.portId == HDMI_PORT_ID) {
        return HdmiPortInit(portIndex, capabilityIndex);
    }
    return HDF_FAILURE;
}

void AudioAdapterReleaseCapSubPorts(const struct AudioPortAndCapability *portCapabilitys, int32_t num)
{
    int32_t i = 0;
    if (portCapabilitys == NULL) {
        return;
    }
    while (i < num) {
        if (&portCapabilitys[i] == NULL) {
            break;
        }
        AudioMemFree((void **)(&portCapabilitys[i].capability.subPorts));
        i++;
    }
    return;
}

int32_t AudioAdapterInitAllPorts(struct AudioAdapter *adapter)
{
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == NULL) {
        HDF_LOGE("hwAdapter Is NULL");
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (hwAdapter->portCapabilitys != NULL) {
        HDF_LOGE("portCapabilitys already Init!");
        return AUDIO_HAL_SUCCESS;
    }
    uint32_t portNum = hwAdapter->adapterDescriptor.portNum;
    struct AudioPort *ports = hwAdapter->adapterDescriptor.ports;
    if (ports == NULL) {
        HDF_LOGE("ports is NULL!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (portNum == 0) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioPortAndCapability *portCapability =
        reinterpret_cast<struct AudioPortAndCapability *>(calloc(portNum, sizeof(struct AudioPortAndCapability)));
    if (portCapability == NULL) {
        HDF_LOGE("portCapability is NULL!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    for (uint32_t i = 0; i < portNum; i++) {
        portCapability[i].port = ports[i];
        if (InitForGetPortCapability(ports[i], &portCapability[i].capability)) {
            HDF_LOGE("ports Init Fail!");
            AudioAdapterReleaseCapSubPorts(portCapability, portNum);
            AudioMemFree((void **)&portCapability);
            return AUDIO_HAL_ERR_INTERNAL;
        }
    }
    hwAdapter->portCapabilitys = portCapability;
    hwAdapter->portCapabilitys->mode = PORT_PASSTHROUGH_LPCM;
    return AUDIO_HAL_SUCCESS;
}

void AudioReleaseRenderHandle(struct AudioHwRender *hwRender)
{
    return;
}

void AudioReleaseCaptureHandle(struct AudioHwCapture *hwCapture)
{
    return;
}

int32_t AudioAdapterCreateRenderPre(struct AudioHwRender *hwRender, const struct AudioDeviceDescriptor *desc,
                                    const struct AudioSampleAttributes *attrs, const struct AudioHwAdapter *hwAdapter)
{
    HDF_LOGD("%{public}s", __func__);
    if (hwAdapter == NULL || hwRender == NULL || desc == NULL || attrs == NULL) {
        HDF_LOGE("Pointer is null!");
        return HDF_FAILURE;
    }

    /* Fill hwRender para */
    if (InitHwRenderParam(hwRender, desc, attrs) < 0) {
        return HDF_FAILURE;
    }

    if (GetAudioRenderFunc(hwRender, hwAdapter->adapterDescriptor.adapterName) < 0) {
        return HDF_FAILURE;
    }
    /* Select Path */
    if (hwAdapter->adapterDescriptor.adapterName == NULL) {
        HDF_LOGE("pointer is null!");
        return HDF_FAILURE;
    }
    uint32_t adapterNameLen = strlen(hwAdapter->adapterDescriptor.adapterName);
    if (adapterNameLen == 0) {
        HDF_LOGE("adapterNameLen is null!");
        return HDF_FAILURE;
    }
    /* Get Adapter name */
    int32_t ret = strncpy_s(hwRender->renderParam.renderMode.hwInfo.adapterName, NAME_LEN - 1,
                            hwAdapter->adapterDescriptor.adapterName, adapterNameLen);
    if (ret != EOK) {
        HDF_LOGE("copy fail");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioAdapterCreateCapturePre(struct AudioHwCapture *hwCapture, const struct AudioDeviceDescriptor *desc,
    const struct AudioSampleAttributes *attrs, const struct AudioHwAdapter *hwAdapter)
{
    HDF_LOGD("%s", __func__);
    if (hwAdapter == nullptr || hwCapture == nullptr || desc == nullptr || attrs == nullptr) {
        HDF_LOGE("Pointer is null!");
        return HDF_FAILURE;
    }
    if (InitHwCaptureParam(hwCapture, desc, attrs) < 0) {
        return HDF_FAILURE;
    }
    if (GetAudioCaptureFunc(hwCapture, hwAdapter->adapterDescriptor.adapterName) < 0) {
        return HDF_FAILURE;
    }
    if (hwAdapter->adapterDescriptor.adapterName == nullptr) {
        HDF_LOGE("pointer is null!");
        return HDF_FAILURE;
    }
    uint32_t adapterNameLen = strlen(hwAdapter->adapterDescriptor.adapterName);
    if (adapterNameLen == 0) {
        HDF_LOGE("adapterNameLen is null!");
        return HDF_FAILURE;
    }
    int32_t ret = strncpy_s(hwCapture->captureParam.captureMode.hwInfo.adapterName, NAME_LEN - 1,
                            hwAdapter->adapterDescriptor.adapterName, adapterNameLen);
    if (ret != EOK) {
        HDF_LOGE("copy fail");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioAdapterCreateRender(struct AudioAdapter *adapter, const struct AudioDeviceDescriptor *desc,
                                 const struct AudioSampleAttributes *attrs, struct AudioRender **render)
{
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == NULL || desc == NULL || attrs == NULL || render == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (hwAdapter->adapterMgrRenderFlag > 0) {
        HDF_LOGE("Create render repeatedly!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioHwRender *hwRender = reinterpret_cast<struct AudioHwRender *>(calloc(1, sizeof(*hwRender)));
    if (hwRender == NULL) {
        HDF_LOGE("hwRender is NULL!");
        return AUDIO_HAL_ERR_MALLOC_FAIL;
    }
    int32_t ret = AudioAdapterCreateRenderPre(hwRender, desc, attrs, hwAdapter);
    if (ret != 0) {
        HDF_LOGE("AudioAdapterCreateRenderPre fail");
        AudioMemFree(reinterpret_cast<void **>(&hwRender));
        return AUDIO_HAL_ERR_INTERNAL;
    }
    hwAdapter->adapterMgrRenderFlag++;
    *render = &hwRender->common;
    return AUDIO_HAL_SUCCESS;
}

int32_t AudioAdapterCreateCapture(struct AudioAdapter *adapter, const struct AudioDeviceDescriptor *desc,
    const struct AudioSampleAttributes *attrs, struct AudioCapture **capture)
{
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == nullptr || desc == nullptr || attrs == nullptr || capture == nullptr) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (hwAdapter->adapterMgrCaptureFlag > 0) {
        HDF_LOGE("Create capture repeatedly!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioHwCapture *hwCapture = reinterpret_cast<struct AudioHwCapture *>(calloc(1, sizeof(*hwCapture)));
    if (hwCapture == nullptr) {
        HDF_LOGE("hwCapture is NULL!");
        return AUDIO_HAL_ERR_MALLOC_FAIL;
    }
    int32_t ret = AudioAdapterCreateCapturePre(hwCapture, desc, attrs, hwAdapter);
    if (ret != 0) {
        HDF_LOGE("AudioAdapterCreateCapturePre fail");
        AudioMemFree(reinterpret_cast<void **>(&hwCapture));
        return AUDIO_HAL_ERR_INTERNAL;
    }
    hwAdapter->adapterMgrCaptureFlag++;
    *capture = &hwCapture->common;
    return AUDIO_HAL_SUCCESS;
}

int32_t AudioAdapterDestroyRender(struct AudioAdapter *adapter, struct AudioRender *render)
{
    HDF_LOGI("enter");
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == NULL || render == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (hwAdapter->adapterMgrRenderFlag > 0) {
        hwAdapter->adapterMgrRenderFlag--;
    }
    struct AudioHwRender *hwRender = reinterpret_cast<struct AudioHwRender *>(render);
    if (hwRender == NULL) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    if (hwRender->renderParam.frameRenderMode.buffer != NULL) {
        HDF_LOGI("render not stop, first stop it.");
        int ret = render->control.Stop((AudioHandle)render);
        if (ret < 0) {
            HDF_LOGE("render Stop failed");
        }
    }
    AudioReleaseRenderHandle(hwRender);
    AudioMemFree(reinterpret_cast<void **>(&hwRender->renderParam.frameRenderMode.buffer));
    AudioMemFree(reinterpret_cast<void **>(&render));
    HDF_LOGI("AudioAdapterDestroyRender cleaned.");
    return AUDIO_HAL_SUCCESS;
}

int32_t AudioAdapterDestroyCapture(struct AudioAdapter *adapter, struct AudioCapture *capture)
{
    HDF_LOGI("enter");
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == nullptr || capture == nullptr) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (hwAdapter->adapterMgrCaptureFlag > 0) {
        hwAdapter->adapterMgrCaptureFlag--;
    }
    struct AudioHwCapture *hwCapture = reinterpret_cast<struct AudioHwCapture *>(capture);
    if (hwCapture == nullptr) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    AudioReleaseCaptureHandle(hwCapture);
    AudioMemFree(reinterpret_cast<void **>(&capture));
    HDF_LOGI("AudioAdapterDestroyCapture cleaned.");
    return AUDIO_HAL_SUCCESS;
}

int32_t AudioAdapterGetPortCapability(struct AudioAdapter *adapter, const struct AudioPort *port,
                                      struct AudioPortCapability *capability)
{
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter == NULL || port == NULL || port->portName == NULL || capability == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (port->portId < 0) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioPortAndCapability *hwAdapterPortCapabilitys = hwAdapter->portCapabilitys;
    if (hwAdapterPortCapabilitys == NULL) {
        HDF_LOGE("hwAdapter portCapabilitys is NULL!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    int32_t portNum = hwAdapter->adapterDescriptor.portNum;
    while (hwAdapterPortCapabilitys != NULL && portNum) {
        if (hwAdapterPortCapabilitys->port.portId == port->portId) {
            *capability = hwAdapterPortCapabilitys->capability;
            return AUDIO_HAL_SUCCESS;
        }
        hwAdapterPortCapabilitys++;
        portNum--;
    }
    return AUDIO_HAL_ERR_INTERNAL;
}

int32_t AudioAdapterSetPassthroughMode(struct AudioAdapter *adapter,
    const struct AudioPort *port, AudioPortPassthroughMode mode)
{
    if (adapter == NULL || port == NULL || port->portName == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (port->dir != PORT_OUT || port->portId < 0 || strcmp(port->portName, "AOP") != 0) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter->portCapabilitys == NULL) {
        HDF_LOGE("The pointer is null!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioPortAndCapability *portCapabilityTemp = hwAdapter->portCapabilitys;
    struct AudioPortCapability *portCapability = NULL;
    int32_t portNum = hwAdapter->adapterDescriptor.portNum;
    while (portCapabilityTemp != NULL && portNum > 0) {
        if (portCapabilityTemp->port.portId == port->portId) {
            portCapability = &portCapabilityTemp->capability;
            break;
        }
        portCapabilityTemp++;
        portNum--;
    }
    if (portCapability == NULL || portNum <= 0) {
        HDF_LOGE("hwAdapter portCapabilitys is Not Find!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioSubPortCapability *subPortCapability = portCapability->subPorts;
    if (subPortCapability == NULL) {
        HDF_LOGE("portCapability->subPorts is NULL!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    int32_t subPortNum = portCapability->subPortsNum;
    while (subPortCapability != NULL && subPortNum > 0) {
        if (subPortCapability->mask == mode) {
            portCapabilityTemp->mode = mode;
            break;
        }
        subPortCapability++;
        subPortNum--;
    }
    if (subPortNum > 0) {
        return AUDIO_HAL_SUCCESS;
    }
    return AUDIO_HAL_ERR_INTERNAL;
}

int32_t AudioAdapterGetPassthroughMode(struct AudioAdapter *adapter, const struct AudioPort *port,
                                       AudioPortPassthroughMode *mode)
{
    if (adapter == NULL || port == NULL || port->portName == NULL || mode == NULL) {
        return AUDIO_HAL_ERR_INVALID_PARAM;
    }
    if (port->dir != PORT_OUT || port->portId < 0 || strcmp(port->portName, "AOP") != 0) {
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioHwAdapter *hwAdapter = reinterpret_cast<struct AudioHwAdapter *>(adapter);
    if (hwAdapter->portCapabilitys == NULL) {
        HDF_LOGE("portCapabilitys pointer is null!");
        return AUDIO_HAL_ERR_INTERNAL;
    }
    struct AudioPortAndCapability *portCapabilitys = hwAdapter->portCapabilitys;
    int32_t portNum = hwAdapter->adapterDescriptor.portNum;
    while (portCapabilitys != NULL && portNum > 0) {
        if (portCapabilitys->port.portId == port->portId) {
            *mode = portCapabilitys->mode;
            return AUDIO_HAL_SUCCESS;
        }
        portCapabilitys++;
        portNum--;
    }
    return AUDIO_HAL_ERR_INTERNAL;
}

int32_t AudioAdapterSetExtraParams(struct AudioAdapter *adapter, enum AudioExtParamKey key,
                                   const char *condition, const char *value)
{
    (void)adapter;
    (void)key;
    (void)condition;
    (void)value;
    return HDF_ERR_NOT_SUPPORT;
}

int32_t AudioAdapterGetExtraParams(struct AudioAdapter *adapter, enum AudioExtParamKey key,
                                   const char *condition, char *value, int32_t length)
{
    (void)adapter;
    (void)key;
    (void)condition;
    (void)value;
    (void)length;
    return HDF_ERR_NOT_SUPPORT;
}

}