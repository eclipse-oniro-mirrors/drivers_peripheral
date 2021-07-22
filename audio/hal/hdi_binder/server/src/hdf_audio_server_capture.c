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

#include "hdf_audio_server_capture.h"

int32_t GetInitCaptureParaAttrs(struct HdfSBuf *data, struct AudioSampleAttributes *attrs)
{
    if (data == NULL || attrs == NULL) {
        return HDF_FAILURE;
    }
    uint32_t tempCapturePara = 0;
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    attrs->type = (enum AudioCategory)tempCapturePara;
    if (!HdfSbufReadUint32(data, &attrs->period)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &attrs->frameSize)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &attrs->startThreshold)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &attrs->stopThreshold)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &attrs->silenceThreshold)) {
        HDF_LOGE("%{public}s", "Capture read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", "Failed to Get Speed sBuf!");
        return HDF_FAILURE;
    }
    attrs->isBigEndian = (bool)tempCapturePara;
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", "Failed to Get Speed sBuf!");
        return HDF_FAILURE;
    }
    attrs->isSignedData = (bool)tempCapturePara;
    return HDF_SUCCESS;
}

int32_t GetInitCapturePara(struct HdfSBuf *data, struct AudioDeviceDescriptor *devDesc,
                           struct AudioSampleAttributes *attrs)
{
    if (data == NULL || devDesc == NULL || attrs == NULL) {
        return HDF_FAILURE;
    }
    uint32_t tempCapturePara = 0;
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", "Failed to Get Speed sBuf!");
        return HDF_FAILURE;
    }
    attrs->format = (enum AudioFormat)tempCapturePara;
    if (!HdfSbufReadUint32(data, &attrs->channelCount)) {
        HDF_LOGE("%{public}s", " read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &attrs->sampleRate)) {
        HDF_LOGE("%{public}s", " read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", "Failed to Get Speed sBuf!");
        return HDF_FAILURE;
    }
    attrs->interleaved = (bool)tempCapturePara;
    if (GetInitCaptureParaAttrs(data, attrs) < 0) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &devDesc->portId)) {
        HDF_LOGE("%{public}s", " read buf fail");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &tempCapturePara)) {
        HDF_LOGE("%{public}s", " read buf fail");
        return HDF_FAILURE;
    }
    devDesc->pins = (enum AudioPortPin)tempCapturePara;
    devDesc->desc = NULL;
    return HDF_SUCCESS;
}

int32_t HdiServiceCreatCapture(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioAdapter *adapter = NULL;
    struct AudioDeviceDescriptor devDesc;
    struct AudioSampleAttributes attrs;
    struct AudioCapture *capture = NULL;
    const char *adapterName = NULL;
    uint32_t capturePid;
    if ((adapterName = HdfSbufReadString(data)) == NULL) {
        HDF_LOGE("%{public}s", "adapterNameCase Is NULL");
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &capturePid)) {
        HDF_LOGE("%{public}s", " read buf fail");
        return HDF_FAILURE;
    }
    HDF_LOGE("%{public}s, capturePid = %{public}u", "HdiServiceCreatCapture:", capturePid);
    int32_t ret = GetInitCapturePara(data, &devDesc, &attrs);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "read Render param failure!");
        return HDF_FAILURE;
    }
    if (AudioAdapterListGetAdapter(adapterName, &adapter)) {
        HDF_LOGE("%{public}s", "AudioAdapterListGetAdapter fail");
        return HDF_FAILURE;
    }
    if (adapter == NULL) {
        HDF_LOGE("%{public}s", "HdiServiceCreatCapture adapter is NULL!");
        return HDF_FAILURE;
    }
    const int32_t priority = attrs.type;
    ret = AudioCreatCaptureCheck(adapterName, priority);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "AudioCreatRenderCheck: Capture is working can not replace!");
        return ret;
    }
    ret = adapter->CreateCapture(adapter, &devDesc, &attrs, &capture);
    if (capture == NULL || ret < 0) {
        HDF_LOGE("%{public}s", "Failed to CreateCapture ");
        return HDF_FAILURE;
    }
    if (AudioAddCaptureInfoInAdapter(adapterName, capture, adapter, priority, capturePid)) {
        HDF_LOGE("%{public}s", "AudioAddRenderInfoInAdapter");
        adapter->DestroyCapture(adapter, capture);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureDestory(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioAdapter *adapter = NULL;
    struct AudioCapture *capture = NULL;
    const char *adapterName = NULL;
    uint32_t pid;
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        return HDF_FAILURE;
    }
    int32_t ret = AudioAdapterListGetCapture(adapterName, &capture, pid);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "AudioAdapterListGetAdapter fail");
        return ret;
    }
    ret = AudioAdapterListGetAdapterCapture(adapterName, &adapter, &capture);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "AudioAdapterListGetAdapter fail");
        return ret;
    }
    ret = adapter->DestroyCapture(adapter, capture);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "DestroyCapture failed!");
        return ret;
    }
    if (AudioDestroyCaptureInfoInAdapter(adapterName)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureStart(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    return capture->control.Start((AudioHandle)capture);
}

int32_t HdiServiceCaptureStop(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    return capture->control.Stop((AudioHandle)capture);
}

int32_t HdiServiceCapturePause(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    return capture->control.Pause((AudioHandle)capture);
}

int32_t HdiServiceCaptureResume(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    return capture->control.Resume((AudioHandle)capture);
}

int32_t HdiServiceCaptureFlush(struct HdfDeviceIoClient *client, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    return capture->control.Flush((AudioHandle)capture);
}

int32_t HdiServiceCaptureGetFrameSize(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint64_t size;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (capture->attr.GetFrameSize((AudioHandle)capture, &size)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, size)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureGetFrameCount(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint64_t count;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (capture->attr.GetFrameCount((AudioHandle)capture, &count)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, count)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureSetSampleAttr(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    int ret;
    struct AudioSampleAttributes attrs;
    struct AudioCapture *capture = NULL;
    ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (ReadAudioSapmleAttrbutes(data, &attrs) < 0) {
        return HDF_FAILURE;
    }
    return capture->attr.SetSampleAttributes((AudioHandle)capture, &attrs);
}

int32_t HdiServiceCaptureGetSampleAttr(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioSampleAttributes attrs;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->attr.GetSampleAttributes((AudioHandle)capture, &attrs);
    if (ret < 0) {
        return ret;
    }
    if (WriteAudioSampleAttributes(reply, &attrs) < 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureGetCurChannelId(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint32_t channelId;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->attr.GetCurrentChannelId((AudioHandle)capture, &channelId);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufWriteUint32(reply, channelId)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureCheckSceneCapability(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioSceneDescriptor scene;
    bool supported = false;
    struct AudioCapture *capture = NULL;
    uint32_t tempPins = 0;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufReadUint32(data, &scene.scene.id)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufReadUint32(data, &tempPins)) {
        return HDF_FAILURE;
    }
    scene.desc.pins = (enum AudioPortPin) tempPins;
    ret = capture->scene.CheckSceneCapability((AudioHandle)capture, &scene, &supported);
    if (ret < 0) {
        return ret;
    }
    uint32_t tempSupported = (uint32_t)supported;
    if (!HdfSbufWriteUint32(reply, tempSupported)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureSelectScene(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    struct AudioSceneDescriptor scene;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufReadUint32(data, &scene.scene.id)) {
        return HDF_FAILURE;
    }
    uint32_t tempPins = 0;
    if (!HdfSbufReadUint32(data, &tempPins)) {
        return HDF_FAILURE;
    }
    scene.desc.pins = (enum AudioPortPin) tempPins;
    return capture->scene.SelectScene((AudioHandle)capture, &scene);
}

int32_t HdiServiceCaptureGetMute(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    int ret;
    bool mute = false;
    struct AudioCapture *capture = NULL;
    ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->volume.GetMute((AudioHandle)capture, &mute);
    if (ret < 0) {
        return ret;
    }
    uint32_t tempMute = (uint32_t)mute;
    if (!HdfSbufWriteUint32(reply, tempMute)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureSetMute(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    bool mute = false;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    uint32_t tempMute = 0;
    if (!HdfSbufReadUint32(data, &tempMute)) {
        return HDF_FAILURE;
    }
    mute = (bool)tempMute;
    return capture->volume.SetMute((AudioHandle)capture, mute);
}

int32_t HdiServiceCaptureSetVolume(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint32_t volume;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufReadUint32(data, &volume)) {
        return HDF_FAILURE;
    }
    float setVolume = (float)volume / VOLUME_CHANGE;
    return capture->volume.SetVolume((AudioHandle)capture, setVolume);
}

int32_t HdiServiceCaptureGetVolume(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    float volume;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->volume.GetVolume((AudioHandle)capture, &volume);
    if (ret < 0) {
        return ret;
    }
    uint32_t tempVolume = (uint32_t)(volume * VOLUME_CHANGE);
    if (!HdfSbufWriteUint32(reply, tempVolume)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureGetGainThreshold(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    float min, max;
    uint32_t tempMin;
    uint32_t tempMax;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->volume.GetGainThreshold((AudioHandle)capture, &min, &max);
    if (ret < 0) {
        return ret;
    }
    tempMin = (uint32_t)min;
    if (!HdfSbufWriteUint32(reply, tempMin)) {
        return HDF_FAILURE;
    }
    tempMax = (uint32_t)max;
    if (!HdfSbufWriteUint32(reply, tempMax)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureGetGain(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    float gain;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->volume.GetGain((AudioHandle)capture, &gain);
    if (ret < 0) {
        return ret;
    }
    uint32_t tempGain = (uint32_t)gain;
    if (!HdfSbufWriteUint32(reply, tempGain)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureSetGain(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint32_t gain;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufReadUint32(data, &gain)) {
        return HDF_FAILURE;
    }
    return capture->volume.SetGain((AudioHandle)capture, (float)gain);
}

int32_t HdiServiceCaptureCaptureFrame(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    HDF_LOGI("%{public}s", "HdiServiceCaptureCaptureFrame entry!");
    char *frame = NULL;
    uint64_t requestBytes, replyBytes;
    struct AudioCapture *capture = NULL;
    const char *adapterName = NULL;
    uint32_t pid;
    if (HdiServiceRenderCaptureReadData(data, &adapterName, &pid) < 0) {
        HDF_LOGE("%{public}s", "HdiServiceRenderRenderFrame:HdiServiceRenderCaptureReadData fail!");
        return HDF_FAILURE;
    }
    int32_t ret = AudioAdapterListGetCapture(adapterName, &capture, pid);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "HdiServiceCaptureCaptureFrame:AudioAdapterListGetRender fail");
        return ret;
    }
    ret = AudioGetCaptureStatus(adapterName);
    if (ret < 0) {
        HDF_LOGE("%{public}s", "HdiServiceCaptureCaptureFrame:AudioGetCaptureStatus fail");
        return ret;
    }
    if (!HdfSbufReadUint64(data, &requestBytes)) {
        return HDF_FAILURE;
    }
    frame = (char *)calloc(1, FRAME_DATA);
    if (frame == NULL) {
        return HDF_FAILURE;
    }
    AudioSetCaptureStatus(adapterName, true);
    ret = capture->CaptureFrame((AudioHandle)capture, (void *)frame, requestBytes, &replyBytes);
    AudioSetCaptureStatus(adapterName, false);
    if (ret < 0) {
        AudioMemFree((void **)&frame);
        return ret;
    }
    if (!HdfSbufWriteBuffer(reply, (const void *)frame, (uint32_t)requestBytes)) {
        AudioMemFree((void **)&frame);
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, replyBytes)) {
        AudioMemFree((void **)&frame);
        return HDF_FAILURE;
    }
    AudioMemFree((void **)&frame);
    return HDF_SUCCESS;
}

int32_t HdiServiceCaptureGetCapturePosition(struct HdfDeviceIoClient *client,
    struct HdfSBuf *data, struct HdfSBuf *reply)
{
    if (client == NULL || data == NULL || reply == NULL) {
        return HDF_FAILURE;
    }
    uint64_t frames;
    struct AudioTimeStamp time;
    struct AudioCapture *capture = NULL;
    int ret = AudioAdapterListCheckAndGetCapture(&capture, data);
    if (ret < 0) {
        return ret;
    }
    ret = capture->GetCapturePosition((AudioHandle)capture, &frames, &time);
    if (ret < 0) {
        return ret;
    }
    if (!HdfSbufWriteUint64(reply, frames)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, time.tvSec)) {
        return HDF_FAILURE;
    }
    if (!HdfSbufWriteUint64(reply, time.tvNSec)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
