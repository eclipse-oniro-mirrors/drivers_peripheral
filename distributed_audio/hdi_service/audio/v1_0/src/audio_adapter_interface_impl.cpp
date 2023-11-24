/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio_adapter_interface_impl.h"

#include <dlfcn.h>
#include <hdf_base.h>
#include <sstream>

#include "cJSON.h"

#include "daudio_constants.h"
#include "daudio_errcode.h"
#include "daudio_events.h"
#include "daudio_log.h"
#include "daudio_utils.h"

#undef DH_LOG_TAG
#define DH_LOG_TAG "AudioAdapterInterfaceImpl"

using namespace OHOS::DistributedHardware;
namespace OHOS {
namespace HDI {
namespace DistributedAudio {
namespace Audio {
namespace V1_0 {
static constexpr uint32_t MAX_AUDIO_STREAM_NUM = 10;

AudioAdapterInterfaceImpl::AudioAdapterInterfaceImpl(const AudioAdapterDescriptor &desc)
    : adpDescriptor_(desc)
{
    renderDevs_ = std::vector<std::pair<int32_t, sptr<AudioRenderInterfaceImplBase>>>(
        MAX_AUDIO_STREAM_NUM, std::make_pair(0, sptr<AudioRenderInterfaceImplBase>(nullptr)));
    captureDevs_ = std::vector<std::pair<int32_t, sptr<AudioCaptureInterfaceImplBase>>>(
        MAX_AUDIO_STREAM_NUM, std::make_pair(0, sptr<AudioCaptureInterfaceImplBase>(nullptr)));
    renderParam_ = { 0, 0, 0, 0, 0, 0 };
    captureParam_ = { 0, 0, 0, 0, 0, 0 };

    DHLOGD("Distributed audio adapter constructed, name(%s).", GetAnonyString(desc.adapterName).c_str());
}

AudioAdapterInterfaceImpl::~AudioAdapterInterfaceImpl()
{
    DHLOGD("Distributed audio adapter destructed, name(%s).", GetAnonyString(adpDescriptor_.adapterName).c_str());
}

void AudioAdapterInterfaceImpl::SetSpeakerCallback(const int32_t dhId, const sptr<IDAudioCallback> &spkCallback)
{
    if (spkCallback == nullptr) {
        DHLOGE("Callback is nullptr.");
        return;
    }
    std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
    if (extCallbackMap_.find(dhId) != extCallbackMap_.end()) {
        DHLOGI("The callback of daudio is already set.");
        return;
    }
    extCallbackMap_[dhId] = spkCallback;
}

void AudioAdapterInterfaceImpl::SetMicCallback(const int32_t dhId, const sptr<IDAudioCallback> &micCallback)
{
    if (micCallback == nullptr) {
        DHLOGE("Callback is nullptr.");
        return;
    }
    std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
    if (extCallbackMap_.find(dhId) != extCallbackMap_.end()) {
        DHLOGI("The callback of daudio is already set.");
        return;
    }
    extCallbackMap_[dhId] = micCallback;
}

int32_t AudioAdapterInterfaceImpl::InitAllPorts()
{
    DHLOGI("Init (%zu) distributed audio ports.", mapAudioDevice_.size());
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::CreateRender(const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, sptr<IAudioRender> &render, uint32_t &renderId)
{
    DHLOGI("Create distributed audio render, {pin: %zu, sampleRate: %zu, channel: %zu, formats: %zu, type: %d}.",
        desc.pins, attrs.sampleRate, attrs.channelCount, attrs.format, static_cast<int32_t>(attrs.type));
    render = nullptr;
    sptr<AudioRenderInterfaceImplBase> audioRender = nullptr;
    if (!CheckDevCapability(desc)) {
        DHLOGE("Can not find device, create render failed.");
        return HDF_FAILURE;
    }
    int renderPinId = 0;
    auto extSpkCallback = MatchStreamCallback(attrs, desc, renderPinId);
    if (extSpkCallback == nullptr) {
        DHLOGE("Matched callback is null.");
        return HDF_FAILURE;
    }
#ifdef DAUDIO_SUPPORT_EXTENSION
    if (attrs.type == AUDIO_MMAP_NOIRQ) {
        DHLOGI("Try to mmap mode.");
        renderFlags_ = Audioext::V1_0::MMAP_MODE;
        audioRender = new AudioRenderExtImpl();
        audioRender->SetAttrs(adpDescriptor_.adapterName, desc, attrs, extSpkCallback, renderPinId);
    } else {
        DHLOGI("Try to normal mode.");
        renderFlags_ = Audioext::V1_0::NORMAL_MODE;
        audioRender = new AudioRenderInterfaceImpl(adpDescriptor_.adapterName, desc, attrs, extSpkCallback);
    }
#else
    renderFlags_ = Audioext::V1_0::NORMAL_MODE;
    audioRender = new AudioRenderInterfaceImpl(adpDescriptor_.adapterName, desc, attrs, extSpkCallback);
#endif
    int32_t ret = OpenRenderDevice(desc, attrs, extSpkCallback, renderPinId);
    if (ret != DH_SUCCESS) {
        DHLOGE("Open render device failed.");
        audioRender = nullptr;
        return ret == ERR_DH_AUDIO_HDF_WAIT_TIMEOUT ? HDF_ERR_TIMEOUT : HDF_FAILURE;
    }
    render = audioRender;
    if (InsertRenderImpl(audioRender, renderPinId, renderId) != HDF_SUCCESS) {
        DHLOGE("Generrate render ID and insert render failed.");
        return HDF_FAILURE;
    }
    DHLOGI("Create render success, render ID is %u.", renderId);
    return HDF_SUCCESS;
}

sptr<IDAudioCallback> AudioAdapterInterfaceImpl::MatchStreamCallback(const AudioSampleAttributes &attrs,
    const AudioDeviceDescriptor &desc, int32_t &dhId)
{
    dhId = static_cast<int32_t>(desc.pins);
    if (desc.pins == DEFAULT_RENDER_ID && attrs.type == AUDIO_MMAP_NOIRQ) {
        dhId = LOW_LATENCY_RENDER_ID;
    }

    std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
    auto iter = extCallbackMap_.find(dhId);
    if (iter == extCallbackMap_.end()) {
        DHLOGE("Can't find matched callback");
        return nullptr;
    }
    return iter->second;
}

int32_t AudioAdapterInterfaceImpl::InsertRenderImpl(const sptr<AudioRenderInterfaceImplBase> &audioRender,
    const int32_t dhId, uint32_t &renderId)
{
    std::lock_guard<std::mutex> devLck(renderDevMtx_);
    if (renderDevs_.size() != MAX_AUDIO_STREAM_NUM) {
        DHLOGE("Render device contain check error.");
        return HDF_FAILURE;
    }
    renderId = MAX_AUDIO_STREAM_NUM;
    for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
        if (renderDevs_[i].second == nullptr) {
            renderId = i;
            renderDevs_[renderId] = std::make_pair(dhId, audioRender);
            return HDF_SUCCESS;
        }
    }
    DHLOGE("The device is busy, can't create render anymore.");
    return HDF_FAILURE;
}

int32_t AudioAdapterInterfaceImpl::DestroyRender(uint32_t renderId)
{
    DHLOGI("Destroy distributed audio render, ID: %u.", renderId);
    if (!IsIdValid(renderId)) {
        DHLOGE("The input render ID is invalid.");
        return HDF_FAILURE;
    }
    sptr<AudioRenderInterfaceImplBase> audioRender(nullptr);
    int32_t dhId = 0;
    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        audioRender = renderDevs_[renderId].second;
        dhId = renderDevs_[renderId].first;
    }
    std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
    sptr<IDAudioCallback> extSpkCallback(extCallbackMap_[dhId]);
    if (audioRender == nullptr) {
        DHLOGD("Render has not been created, do not need destroy.");
        return HDF_SUCCESS;
    }

    int32_t ret = CloseRenderDevice(audioRender->GetRenderDesc(), extSpkCallback, dhId);
    if (ret != DH_SUCCESS) {
        DHLOGE("Close render device failed.");
        return HDF_FAILURE;
    }
    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        renderDevs_[renderId] = std::make_pair(0, sptr<AudioRenderInterfaceImplBase>(nullptr));
    }
    return HDF_SUCCESS;
}

bool AudioAdapterInterfaceImpl::CheckDevCapability(const AudioDeviceDescriptor &desc)
{
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    if (mapAudioDevice_.find(desc.pins) == mapAudioDevice_.end()) {
        DHLOGE("Can not find device, create render failed.");
        return false;
    }
    return true;
}

int32_t AudioAdapterInterfaceImpl::CreateCapture(const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, sptr<IAudioCapture> &capture, uint32_t &captureId)
{
    DHLOGI("Create distributed audio capture, {pin: %zu, sampleRate: %zu, channel: %zu, formats: %zu}.",
        desc.pins, attrs.sampleRate, attrs.channelCount, attrs.format);
    capture = nullptr;
    sptr<AudioCaptureInterfaceImplBase> audioCapture(nullptr);
    if (!CheckDevCapability(desc)) {
        DHLOGE("Can not find device, create capture failed.");
        return HDF_FAILURE;
    }
    int32_t capPinId = 0;
    auto extMicCallback = MatchStreamCallback(attrs, desc, capPinId);
    if (extMicCallback == nullptr) {
        DHLOGE("Matched callback is null.");
        return HDF_FAILURE;
    }
#ifdef DAUDIO_SUPPORT_EXTENSION
    if (attrs.type == AUDIO_MMAP_NOIRQ) {
        DHLOGI("Try to mmap mode.");
        capturerFlags_ = Audioext::V1_0::MMAP_MODE;
        audioCapture = new AudioCaptureExtImpl();
        audioCapture->SetAttrs(adpDescriptor_.adapterName, desc, attrs, extMicCallback, desc.pins);
    } else {
        DHLOGI("Try to normal mode.");
        capturerFlags_ = Audioext::V1_0::NORMAL_MODE;
        audioCapture = new AudioCaptureInterfaceImpl(adpDescriptor_.adapterName, desc, attrs, extMicCallback);
    }
#else
    capturerFlags_ = Audioext::V1_0::NORMAL_MODE;
    audioCapture = new AudioCaptureInterfaceImpl(adpDescriptor_.adapterName, desc, attrs, extMicCallback);
#endif
    int32_t ret = OpenCaptureDevice(desc, attrs, extMicCallback, capPinId);
    if (ret != DH_SUCCESS) {
        DHLOGE("Open capture device failed.");
        audioCapture = nullptr;
        return ret == ERR_DH_AUDIO_HDF_WAIT_TIMEOUT ? HDF_ERR_TIMEOUT : HDF_FAILURE;
    }
    capture = audioCapture;
    if (InsertCapImpl(audioCapture, capPinId, captureId) != HDF_SUCCESS) {
        DHLOGE("Generrate capture ID and insert capture failed.");
        return HDF_FAILURE;
    }
    DHLOGI("Create capture success, capture ID is %u.", captureId);
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::InsertCapImpl(const sptr<AudioCaptureInterfaceImplBase> &audioCapture,
    const int32_t dhId, uint32_t &captureId)
{
    std::lock_guard<std::mutex> devLck(capDevMtx_);
    if (captureDevs_.size() != MAX_AUDIO_STREAM_NUM) {
        DHLOGE("Capture device's size check error.");
        return HDF_FAILURE;
    }
    captureId = MAX_AUDIO_STREAM_NUM;
    for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
        if (captureDevs_[i].second == nullptr) {
            captureId = i;
            captureDevs_[captureId] = std::make_pair(dhId, audioCapture);
            return HDF_SUCCESS;
        }
    }
    DHLOGE("The device is busy, can't create capture anymore.");
    return HDF_FAILURE;
}

int32_t AudioAdapterInterfaceImpl::DestroyCapture(uint32_t captureId)
{
    DHLOGI("Destroy distributed audio capture, ID: %u.", captureId);
    if (!IsIdValid(captureId)) {
        DHLOGE("The input capture ID is invalid.");
        return HDF_FAILURE;
    }
    sptr<AudioCaptureInterfaceImplBase> audioCapture(nullptr);
    int32_t dhId = 0;
    {
        std::lock_guard<std::mutex> devLck(capDevMtx_);
        audioCapture = captureDevs_[captureId].second;
        dhId = captureDevs_[captureId].first;
    }
    std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
    sptr<IDAudioCallback> extMicCallback(extCallbackMap_[dhId]);
    if (audioCapture == nullptr) {
        DHLOGD("Capture has not been created, do not need destroy.");
        return HDF_SUCCESS;
    }

    int32_t ret = CloseCaptureDevice(audioCapture->GetCaptureDesc(), extMicCallback, dhId);
    if (ret != DH_SUCCESS) {
        DHLOGE("Close capture device failed.");
        return HDF_FAILURE;
    }
    {
        std::lock_guard<std::mutex> devLck(capDevMtx_);
        captureDevs_[captureId].first = 0;
        captureDevs_[captureId].second = nullptr;
    }
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetPortCapability(const AudioPort &port, AudioPortCapability &capability)
{
    DHLOGD("Get audio port capability.");
    (void)port;
    capability.sampleRateMasks = AUDIO_SAMPLE_RATE_DEFAULT;
    capability.channelCount = AUDIO_CHANNEL_COUNT_DEFAULT;

    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::SetPassthroughMode(const AudioPort &port, AudioPortPassthroughMode mode)
{
    (void)port;
    (void)mode;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetPassthroughMode(const AudioPort &port, AudioPortPassthroughMode &mode)
{
    (void)port;
    (void)mode;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetDeviceStatus(AudioDeviceStatus& status)
{
    (void) status;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::UpdateAudioRoute(const AudioRoute &route, int32_t &routeHandle)
{
    (void) route;
    (void) routeHandle;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::ReleaseAudioRoute(int32_t routeHandle)
{
    (void) routeHandle;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::SetMicMute(bool mute)
{
    (void) mute;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetMicMute(bool& mute)
{
    (void) mute;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::SetVoiceVolume(float volume)
{
    (void) volume;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::SetExtraParams(AudioExtParamKey key, const std::string &condition,
    const std::string &value)
{
    DHLOGD("Set audio parameters, key = %d, condition: %s value: %s.", key, condition.c_str(), value.c_str());
    int32_t ret = ERR_DH_AUDIO_HDF_FAIL;
    switch (key) {
        case AudioExtParamKey::AUDIO_EXT_PARAM_KEY_VOLUME:
            ret = SetAudioVolume(condition, value);
            if (ret != DH_SUCCESS) {
                DHLOGE("Set audio parameters failed.");
                return HDF_FAILURE;
            }
            break;
        case AudioExtParamKey::AUDIO_EXT_PARAM_KEY_NONE:
            DHLOGE("Parameter is unknown.");
            break;
        default:
            DHLOGE("Parameter is invalid.");
            return HDF_ERR_INVALID_PARAM;
    }
    DHLOGI("Set audio parameters success.");
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetExtraParams(AudioExtParamKey key, const std::string &condition,
    std::string &value)
{
    DHLOGD("Get audio parameters, key: %d, condition: %s.", key, condition.c_str());
    int32_t ret = ERR_DH_AUDIO_HDF_FAIL;
    switch (key) {
        case AudioExtParamKey::AUDIO_EXT_PARAM_KEY_VOLUME:
            ret = GetAudioVolume(condition, value);
            if (ret != DH_SUCCESS) {
                DHLOGE("Get audio parameters failed.");
                return HDF_FAILURE;
            }
            break;
        case AudioExtParamKey::AUDIO_EXT_PARAM_KEY_NONE:
            DHLOGE("Parameter is unknown.");
            break;
        default:
            DHLOGE("Parameter is invalid.");
            return HDF_ERR_INVALID_PARAM;
    }
    DHLOGI("Get audio parameters success.");
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::RegExtraParamObserver(const sptr<IAudioCallback> &audioCallback, int8_t cookie)
{
    DHLOGD("Register audio param observer.");
    paramCallback_ = audioCallback;
    (void) cookie;
    return HDF_SUCCESS;
}

AudioAdapterDescriptor AudioAdapterInterfaceImpl::GetAdapterDesc()
{
    adpDescriptor_.ports.clear();
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    for (auto pin = mapAudioDevice_.begin(); pin != mapAudioDevice_.end(); pin++) {
        AudioPort port = {PORT_OUT_IN, pin->first, ""};
        adpDescriptor_.ports.emplace_back(port);
    }
    return adpDescriptor_;
}

std::string AudioAdapterInterfaceImpl::GetDeviceCapabilitys(const uint32_t devId)
{
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    auto dev = mapAudioDevice_.find(devId);
    if (dev == mapAudioDevice_.end()) {
        DHLOGE("Device not found.");
        return "";
    }
    return dev->second;
}

int32_t AudioAdapterInterfaceImpl::AdapterLoad()
{
    status_ = AudioAdapterStatus::STATUS_LOAD;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::AdapterUnload()
{
    if (CheckRendersValid() || CheckCapsValid()) {
        DHLOGE("Adapter is busy, audio render or capture is not null.");
        return HDF_ERR_DEVICE_BUSY;
    }
    status_ = AudioAdapterStatus::STATUS_UNLOAD;
    return HDF_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::Notify(const uint32_t devId, const DAudioEvent &event)
{
    switch (static_cast<AudioExtParamEvent>(event.type)) {
        case HDF_AUDIO_EVENT_VOLUME_CHANGE:
            DHLOGI("Notify event: VOLUME_CHANGE, event content: %s.", event.content.c_str());
            return HandleVolumeChangeEvent(event);
        case HDF_AUDIO_EVENT_FOCUS_CHANGE:
            DHLOGI("Notify event: FOCUS_CHANGE, event content: %s.", event.content.c_str());
            return HandleFocusChangeEvent(event);
        case HDF_AUDIO_EVENT_RENDER_STATE_CHANGE:
            DHLOGI("Notify event: RENDER_STATE_CHANGE, event content: %s.", event.content.c_str());
            return HandleRenderStateChangeEvent(event);
        case HDF_AUDIO_EVENT_OPEN_SPK_RESULT:
        case HDF_AUDIO_EVENT_CLOSE_SPK_RESULT:
        case HDF_AUDIO_EVENT_OPEN_MIC_RESULT:
        case HDF_AUDIO_EVENT_CLOSE_MIC_RESULT:
        case HDF_AUDIO_EVENT_SPK_DUMP:
        case HDF_AUDIO_EVENT_MIC_DUMP:
            return HandleSANotifyEvent(event);
        case HDF_AUDIO_EVENT_SPK_CLOSED:
        case HDF_AUDIO_EVENT_MIC_CLOSED:
            return HandleDeviceClosed(event);
        default:
            DHLOGE("Audio event: %d is undefined.", event.type);
            return ERR_DH_AUDIO_HDF_INVALID_OPERATION;
    }
}

int32_t AudioAdapterInterfaceImpl::AddAudioDevice(const uint32_t devId, const std::string &caps)
{
    DHLOGI("Add distributed audio device %d.", devId);
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    auto dev = mapAudioDevice_.find(devId);
    if (dev != mapAudioDevice_.end()) {
        DHLOGI("Device has been add, do not repeat add.");
        return DH_SUCCESS;
    }
    mapAudioDevice_.insert(std::make_pair(devId, caps));

    DHLOGI("Add audio device success.");
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::RemoveAudioDevice(const uint32_t devId)
{
    DHLOGI("Remove distributed audio device %d.", devId);
    {
        std::lock_guard<std::mutex> devLck(devMapMtx_);
        if (mapAudioDevice_.find(devId) == mapAudioDevice_.end()) {
            DHLOGE("Device has not been add, remove device failed.");
            return ERR_DH_AUDIO_HDF_INVALID_OPERATION;
        }
        mapAudioDevice_.erase(devId);
    }
    if (devId == spkPinInUse_) {
        for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
            DestroyRender(i);
        }
    }
    if (devId == micPinInUse_) {
        for (uint32_t capId = 0; capId < MAX_AUDIO_STREAM_NUM; capId++) {
            DestroyCapture(capId);
        }
    }

    DHLOGI("Remove audio device success.");
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::OpenRenderDevice(const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, const sptr<IDAudioCallback> extSpkCallback, int32_t dhId)
{
    DHLOGI("Open render device, pin: %d.", dhId);
    if (isSpkOpened_) {
        DHLOGI("Render already opened.");
        return DH_SUCCESS;
    }
    std::lock_guard<std::mutex> devLck(renderOptMtx_);
    spkPinInUse_ = static_cast<uint32_t>(dhId);
    renderParam_.format = attrs.format;
    renderParam_.channelCount = attrs.channelCount;
    renderParam_.sampleRate = attrs.sampleRate;
    renderParam_.streamUsage = attrs.type;
    renderParam_.frameSize = CalculateFrameSize(attrs.sampleRate, attrs.channelCount, attrs.format,
        timeInterval_, renderFlags_ == Audioext::V1_0::MMAP_MODE);
    renderParam_.renderFlags = renderFlags_;

    int32_t ret = extSpkCallback->SetParameters(adpDescriptor_.adapterName, dhId, renderParam_);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Set render parameters failed.");
        return ERR_DH_AUDIO_HDF_SET_PARAM_FAIL;
    }
    ret = extSpkCallback->OpenDevice(adpDescriptor_.adapterName, dhId);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Open render device failed.");
        return ERR_DH_AUDIO_HDF_OPEN_DEVICE_FAIL;
    }

    ret = WaitForSANotify(EVENT_OPEN_SPK);
    if (ret != DH_SUCCESS) {
        DHLOGE("Wait SA notify failed. ret: %d", ret);
        return ret;
    }
    DHLOGI("Open render device success.");
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::CloseRenderDevice(const AudioDeviceDescriptor &desc,
    sptr<IDAudioCallback> extSpkCallback, const int32_t dhId)
{
    DHLOGI("Close render device, pin: %d.", dhId);
    if (extSpkCallback == nullptr) {
        DHLOGE("Callback is null.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    std::lock_guard<std::mutex> devLck(renderOptMtx_);
    if (spkPinInUse_ == 0) {
        DHLOGI("No need close render device.");
        return DH_SUCCESS;
    }
    renderParam_ = {};
    int32_t ret = extSpkCallback->CloseDevice(adpDescriptor_.adapterName, dhId);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Close audio device failed.");
        return ERR_DH_AUDIO_HDF_CLOSE_DEVICE_FAIL;
    }

    ret = WaitForSANotify(EVENT_CLOSE_SPK);
    if (ret != DH_SUCCESS) {
        DHLOGE("Wait SA notify failed. ret: %d.", ret);
        return ret;
    }
    spkPinInUse_ = 0;
    DHLOGI("Close render device success.");
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::OpenCaptureDevice(const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, const sptr<IDAudioCallback> extMicCallback, const int32_t dhId)
{
    DHLOGI("Open capture device, pin: %d.", dhId);
    if (isMicOpened_) {
        DHLOGI("Capture already opened.");
        return DH_SUCCESS;
    }
    std::lock_guard<std::mutex> devLck(captureOptMtx_);
    micPinInUse_ = dhId;
    captureParam_.format = attrs.format;
    captureParam_.channelCount = attrs.channelCount;
    captureParam_.sampleRate = attrs.sampleRate;
    captureParam_.streamUsage = attrs.type;
    captureParam_.frameSize = CalculateFrameSize(attrs.sampleRate, attrs.channelCount,
        attrs.format, timeInterval_, capturerFlags_ == Audioext::V1_0::MMAP_MODE);
    captureParam_.capturerFlags = capturerFlags_;

    int32_t ret = extMicCallback->SetParameters(adpDescriptor_.adapterName, dhId, captureParam_);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Set audio parameters failed.");
        return ERR_DH_AUDIO_HDF_SET_PARAM_FAIL;
    }
    ret = extMicCallback->OpenDevice(adpDescriptor_.adapterName, dhId);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Open audio device failed.");
        return ERR_DH_AUDIO_HDF_OPEN_DEVICE_FAIL;
    }

    ret = WaitForSANotify(EVENT_OPEN_MIC);
    if (ret != DH_SUCCESS) {
        DHLOGE("Wait SA notify failed. ret: %d.", ret);
        return ret;
    }
    DHLOGI("Open capture device success.");
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::CloseCaptureDevice(const AudioDeviceDescriptor &desc,
    const sptr<IDAudioCallback> extMicCallback, const int32_t dhId)
{
    DHLOGI("Close capture device, pin: %d.", dhId);
    std::lock_guard<std::mutex> devLck(captureOptMtx_);
    if (micPinInUse_ == 0) {
        DHLOGI("No need close capture device.");
        return DH_SUCCESS;
    }
    captureParam_ = {};
    int32_t ret = extMicCallback->CloseDevice(adpDescriptor_.adapterName, dhId);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Close audio device failed.");
        return ERR_DH_AUDIO_HDF_CLOSE_DEVICE_FAIL;
    }

    ret = WaitForSANotify(EVENT_CLOSE_MIC);
    if (ret != DH_SUCCESS) {
        DHLOGE("Wait SA notify failed. ret:%d.", ret);
        return ret;
    }
    micPinInUse_ = 0;
    DHLOGI("Close capture device success.");
    return DH_SUCCESS;
}

uint32_t AudioAdapterInterfaceImpl::GetVolumeGroup(const uint32_t devId)
{
    uint32_t volGroup = VOLUME_GROUP_ID_DEFAULT;
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    auto caps = mapAudioDevice_.find(devId);
    if (caps == mapAudioDevice_.end()) {
        DHLOGE("Can not find caps of dev:%u.", devId);
        return volGroup;
    }

    int32_t ret = GetAudioParamUInt(caps->second, VOLUME_GROUP_ID, volGroup);
    if (ret != DH_SUCCESS) {
        DHLOGE("Get volume group param failed, use default value, ret = %d.", ret);
    }
    return volGroup;
}

uint32_t AudioAdapterInterfaceImpl::GetInterruptGroup(const uint32_t devId)
{
    uint32_t iptGroup = INTERRUPT_GROUP_ID_DEFAULT;
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    auto caps = mapAudioDevice_.find(devId);
    if (caps == mapAudioDevice_.end()) {
        DHLOGE("Can not find caps of dev:%u.", devId);
        return iptGroup;
    }

    int32_t ret = GetAudioParamUInt(caps->second, INTERRUPT_GROUP_ID, iptGroup);
    if (ret != DH_SUCCESS) {
        DHLOGE("Get interrupt group param failed, use default value, ret = %d.", ret);
    }
    return iptGroup;
}

int32_t AudioAdapterInterfaceImpl::SetAudioVolume(const std::string& condition, const std::string &param)
{
    std::string content = condition;
    int32_t type = getEventTypeFromCondition(content);
    EXT_PARAM_EVENT eventType;

    if (type == VolumeEventType::EVENT_IS_STREAM_MUTE) {
        if (param == IS_MUTE_STATUS) {
            streamMuteStatus_ = 1;
        } else if (param == NOT_MUTE_STATUS) {
            streamMuteStatus_ = 0;
        } else {
            DHLOGE("Mute param is error.");
            return ERR_DH_AUDIO_HDF_FAIL;
        }
        eventType = HDF_AUDIO_EVNET_MUTE_SET;
        SetAudioParamStr(content, STREAM_MUTE_STATUS, param);
    } else {
        eventType = HDF_AUDIO_EVENT_VOLUME_SET;
        streamMuteStatus_ = 0;
        SetAudioParamStr(content, VOLUME_LEVEL, param);
    }
    DAudioEvent event = { eventType, content };

    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        for (const auto &item : renderDevs_) {
            std::lock_guard<std::mutex> callbackLck(extCallbackMtx_);
            sptr<IDAudioCallback> extSpkCallback(extCallbackMap_[item.first]);
            SetAudioParamStr(event.content, "dhId", std::to_string(item.first));
            auto render = item.second;
            if (render == nullptr || extSpkCallback == nullptr) {
                continue;
            }
            if (extSpkCallback->NotifyEvent(adpDescriptor_.adapterName,
                item.first, event) != HDF_SUCCESS) {
                DHLOGE("NotifyEvent failed.");
                return ERR_DH_AUDIO_HDF_FAIL;
            }
        }
    }
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::GetAudioVolume(const std::string& condition, std::string &param)
{
    int renderId = 0; // fromaudio framwork
    sptr<AudioRenderInterfaceImplBase> audioRender(nullptr);
    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        audioRender = renderDevs_[renderId].second;
    }
    if (audioRender == nullptr) {
        DHLOGE("Render has not been created.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    int32_t type = getEventTypeFromCondition(condition);
    uint32_t vol;
    switch (type) {
        case VolumeEventType::EVENT_GET_VOLUME:
            vol = audioRender->GetVolumeInner();
            break;
        case VolumeEventType::EVENT_GET_MAX_VOLUME:
            vol = audioRender->GetMaxVolumeInner();
            break;
        case VolumeEventType::EVENT_GET_MIN_VOLUME:
            vol = audioRender->GetMinVolumeInner();
            break;
        case VolumeEventType::EVENT_IS_STREAM_MUTE:
            vol = streamMuteStatus_;
            break;
        default:
            vol = 0;
            DHLOGE("Get volume failed.");
    }
    param = std::to_string(vol);
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::getEventTypeFromCondition(const std::string &condition)
{
    std::string::size_type position = condition.find_first_of(";");
    int32_t len = position - TYPE_CONDITION;
    if (len < 0 || len > MAX_EVENT_DIGITS || position == std::string::npos) {
        DHLOGE("Position is illegal or not find split word");
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    std::string val = condition.substr(TYPE_CONDITION, len);
    if (!CheckIsNum(val)) {
        DHLOGE("String is not number. str:%s.", val.c_str());
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    int32_t type = std::stoi(val);
    return static_cast<VolumeEventType>(type);
}

int32_t AudioAdapterInterfaceImpl::HandleVolumeChangeEvent(const DAudioEvent &event)
{
    DHLOGI("Vol change (%s).", event.content.c_str());
    int renderId = 0; // daudio SA
    sptr<AudioRenderInterfaceImplBase> audioRender(nullptr);
    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        audioRender = renderDevs_[renderId].second;
    }
    if (audioRender == nullptr) {
        DHLOGE("Render has not been created.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    int32_t vol = AUDIO_DEFAULT_MIN_VOLUME_LEVEL;
    int32_t ret = GetVolFromEvent(event.content, VOLUME_LEVEL, vol);
    if (ret != DH_SUCCESS) {
        DHLOGE("Get volume value failed.");
        return ERR_DH_AUDIO_HDF_FAIL;
    }

    if (event.content.rfind(FIRST_VOLUME_CHANAGE, 0) == 0) {
        int32_t maxVol = AUDIO_DEFAULT_MAX_VOLUME_LEVEL;
        ret = GetVolFromEvent(event.content, MAX_VOLUME_LEVEL, maxVol);
        if (ret != DH_SUCCESS) {
            DHLOGE("Get max volume value failed, use defult max volume.");
        }
        int32_t minVol = AUDIO_DEFAULT_MIN_VOLUME_LEVEL;
        ret = GetVolFromEvent(event.content, MIN_VOLUME_LEVEL, minVol);
        if (ret != DH_SUCCESS) {
            DHLOGE("Get min volume value failed, use defult min volume.");
        }
        audioRender->SetVolumeInner(vol);
        audioRender->SetVolumeRangeInner(maxVol, minVol);
        return DH_SUCCESS;
    }

    audioRender->SetVolumeInner(vol);
    if (paramCallback_ == nullptr) {
        DHLOGE("Audio param observer is null.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    std::string volumeChange = GetVolumeChangeString(event.content);
    int8_t reserved = 0;
    int8_t cookie = 0;
    ret = paramCallback_->ParamCallback(AUDIO_EXT_PARAM_KEY_VOLUME, volumeChange, std::to_string(vol),
        reserved, cookie);
    if (ret != DH_SUCCESS) {
        DHLOGE("Notify vol failed.");
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    return DH_SUCCESS;
}

std::string AudioAdapterInterfaceImpl::GetVolumeChangeString(const std::string &args)
{
    DHLOGI("Vol change (%s).", args.c_str());
    std::stringstream ss;
    ss << VOLUME_CHANAGE << ";"
        << AUDIO_STREAM_TYPE << "=" << ParseStringFromArgs(args, AUDIO_STREAM_TYPE) << ";"
        << VOLUME_LEVEL << "=" << ParseStringFromArgs(args, VOLUME_LEVEL.c_str()) << ";"
        << IS_UPDATEUI << "=" << ParseStringFromArgs(args, IS_UPDATEUI) << ";"
        << VOLUME_GROUP_ID << "=" << ParseStringFromArgs(args, VOLUME_GROUP_ID.c_str()) << ";"
        << KEY_DH_ID << "=" << ParseStringFromArgs(args, KEY_DH_ID) << ";";
    std::string res = ss.str();
    DHLOGI("get ss : %s", res.c_str());
    return res;
}

int32_t AudioAdapterInterfaceImpl::GetVolFromEvent(const std::string &content, const std::string &key, int32_t &vol)
{
    cJSON *jParam = cJSON_Parse(content.c_str());
    if (jParam == nullptr) {
        DHLOGE("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        cJSON_Delete(jParam);
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    cJSON *dhIdItem = cJSON_GetObjectItem(jParam, key.c_str());
    if (dhIdItem == NULL || !cJSON_IsString(dhIdItem)) {
        DHLOGE("Not found the keys of dhId.");
        cJSON_Delete(jParam);
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    std::string val(dhIdItem->valuestring);
    if (!CheckIsNum(val)) {
        DHLOGE("String is not number. str:%s.", val.c_str());
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    vol = std::stoi(val);
    cJSON_Delete(jParam);
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::HandleFocusChangeEvent(const DAudioEvent &event)
{
    DHLOGI("Focus change (%s).", event.content.c_str());
    if (paramCallback_ == nullptr) {
        DHLOGE("Audio param observer is null.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    std::stringstream ss;
    ss << INTERRUPT_EVENT << ";"
        << VOLUME_EVENT_TYPE << "=" << ParseStringFromArgs(event.content, VOLUME_EVENT_TYPE.c_str()) << ";"
        << FORCE_TYPE << "=" << ParseStringFromArgs(event.content, FORCE_TYPE) << ";"
        << HINT_TYPE << "=" << ParseStringFromArgs(event.content, HINT_TYPE) << ";"
        << KEY_DH_ID << "=" << ParseStringFromArgs(event.content, KEY_DH_ID) << ";";
    DHLOGI("get ss : %s", ss.str().c_str());
    int8_t reserved = 0;
    int8_t cookie = 0;
    int32_t ret = paramCallback_->ParamCallback(AUDIO_EXT_PARAM_KEY_FOCUS, ss.str(), "", reserved, cookie);
    if (ret != DH_SUCCESS) {
        DHLOGE("Notify Focus failed.");
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::HandleRenderStateChangeEvent(const DAudioEvent &event)
{
    DHLOGI("Render state change (%s).", event.content.c_str());
    if (paramCallback_ == nullptr) {
        DHLOGE("Audio param observer is null.");
        return ERR_DH_AUDIO_HDF_NULLPTR;
    }
    std::stringstream ss;
    ss << RENDER_STATE_CHANGE_EVENT << ";"
        << KEY_STATE << "=" << ParseStringFromArgs(event.content, KEY_STATE) << ";"
        << KEY_DH_ID << "=" << ParseStringFromArgs(event.content, KEY_DH_ID) << ";";
    DHLOGI("get ss : %s", ss.str().c_str());
    int8_t reserved = 0;
    int8_t cookie = 0;
    int32_t ret = paramCallback_->ParamCallback(AUDIO_EXT_PARAM_KEY_STATUS, ss.str(), "", reserved, cookie);
    if (ret != DH_SUCCESS) {
        DHLOGE("Notify render state failed.");
        return ERR_DH_AUDIO_HDF_FAIL;
    }
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::ConvertMsg2Code(const std::string &msg)
{
    if (msg == HDF_EVENT_RESULT_SUCCESS) {
        return DH_SUCCESS;
    } else if (msg == HDF_EVENT_INIT_ENGINE_FAILED) {
        return ERR_DH_AUDIO_HDF_INIT_ENGINE_FAILED;
    } else if (msg == HDF_EVENT_NOTIFY_SINK_FAILED) {
        return ERR_DH_AUDIO_HDF_NOTIFY_SINK_FAILED;
    } else if (msg == HDF_EVENT_TRANS_SETUP_FAILED) {
        return ERR_DH_AUDIO_HDF_TRANS_SETUP_FAILED;
    } else if (msg == HDF_EVENT_TRANS_START_FAILED) {
        return ERR_DH_AUDIO_HDF_TRANS_START_FAILED;
    } else {
        return ERR_DH_AUDIO_HDF_FAIL;
    }
}

int32_t AudioAdapterInterfaceImpl::HandleSANotifyEvent(const DAudioEvent &event)
{
    DHLOGD("Notify event type %d, event content: %s.", event.type, event.content.c_str());
    switch (event.type) {
        case HDF_AUDIO_EVENT_OPEN_SPK_RESULT:
            if (event.content == HDF_EVENT_RESULT_SUCCESS) {
                isSpkOpened_ = true;
            }
            errCode_ = ConvertMsg2Code(event.content);
            spkNotifyFlag_ = true;
            spkWaitCond_.notify_all();
            break;
        case HDF_AUDIO_EVENT_CLOSE_SPK_RESULT:
            if (event.content == HDF_EVENT_RESULT_SUCCESS) {
                isSpkOpened_ = false;
            }
            errCode_ = ConvertMsg2Code(event.content);
            spkNotifyFlag_ = true;
            spkWaitCond_.notify_all();
            break;
        case HDF_AUDIO_EVENT_OPEN_MIC_RESULT:
            if (event.content == HDF_EVENT_RESULT_SUCCESS) {
                isMicOpened_ = true;
            }
            errCode_ = ConvertMsg2Code(event.content);
            micNotifyFlag_ = true;
            micWaitCond_.notify_all();
            break;
        case HDF_AUDIO_EVENT_CLOSE_MIC_RESULT:
            if (event.content == HDF_EVENT_RESULT_SUCCESS) {
                isMicOpened_ = false;
            }
            errCode_ = ConvertMsg2Code(event.content);
            micNotifyFlag_ = true;
            micWaitCond_.notify_all();
            break;
        case HDF_AUDIO_EVENT_SPK_DUMP:
            SetDumpFlag(true);
            break;
        case HDF_AUDIO_EVENT_MIC_DUMP:
            SetDumpFlag(false);
            break;
        default:
            DHLOGE("Notify not support event type %d, event content: %s.", event.type, event.content.c_str());
            return ERR_DH_AUDIO_HDF_FAIL;
    }
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::WaitForSANotify(const AudioDeviceEvent &event)
{
    if (event == EVENT_OPEN_SPK || event == EVENT_CLOSE_SPK) {
        spkNotifyFlag_ = false;
        std::unique_lock<std::mutex> lck(spkWaitMutex_);
        auto status = spkWaitCond_.wait_for(lck, std::chrono::seconds(WAIT_SECONDS), [this, event]() {
            return spkNotifyFlag_ ||
                (event == EVENT_OPEN_SPK && isSpkOpened_) || (event == EVENT_CLOSE_SPK && !isSpkOpened_);
        });
        if (!status) {
            DHLOGE("Wait spk event: %d timeout(%d)s.", event, WAIT_SECONDS);
            return ERR_DH_AUDIO_HDF_WAIT_TIMEOUT;
        }
        if (event == EVENT_OPEN_SPK && !isSpkOpened_) {
            DHLOGE("Wait open render device failed.");
            return errCode_;
        } else if (event == EVENT_CLOSE_SPK && isSpkOpened_) {
            DHLOGE("Wait close render device failed.");
            return errCode_;
        }
        return DH_SUCCESS;
    }

    if (event == EVENT_OPEN_MIC || event == EVENT_CLOSE_MIC) {
        micNotifyFlag_ = false;
        std::unique_lock<std::mutex> lck(micWaitMutex_);
        auto status = micWaitCond_.wait_for(lck, std::chrono::seconds(WAIT_SECONDS), [this, event]() {
            return micNotifyFlag_ ||
                (event == EVENT_OPEN_MIC && isMicOpened_) || (event == EVENT_CLOSE_MIC && !isMicOpened_);
        });
        if (!status) {
            DHLOGE("Wait mic event: %d timeout(%d)s.", event, WAIT_SECONDS);
            return ERR_DH_AUDIO_HDF_WAIT_TIMEOUT;
        }
        if (event == EVENT_OPEN_MIC && !isMicOpened_) {
            DHLOGE("Wait open capture device failed.");
            return errCode_;
        } else if (event == EVENT_CLOSE_MIC && isMicOpened_) {
            DHLOGE("Wait close capture device failed.");
            return errCode_;
        }
        return DH_SUCCESS;
    }
    return DH_SUCCESS;
}

int32_t AudioAdapterInterfaceImpl::HandleDeviceClosed(const DAudioEvent &event)
{
    DHLOGI("Handle device closed, event type: %d.", event.type);
    if (paramCallback_ != nullptr) {
        std::stringstream ss;
        ss << "ERR_EVENT;DEVICE_TYPE=" <<
            (event.type == HDF_AUDIO_EVENT_SPK_CLOSED ? AUDIO_DEVICE_TYPE_SPEAKER : AUDIO_DEVICE_TYPE_MIC) << ";"
            << KEY_DH_ID << "=" << ParseStringFromArgs(event.content, KEY_DH_ID) << ";";
        DHLOGI("get ss : %s", ss.str().c_str());
        int8_t reserved = 0;
        int8_t cookie = 0;
        int32_t ret = paramCallback_->ParamCallback(AUDIO_EXT_PARAM_KEY_STATUS, ss.str(),
            std::to_string(EVENT_DEV_CLOSED), reserved, cookie);
        if (ret != DH_SUCCESS) {
            DHLOGE("Notify fwk failed.");
        }
    }

    if (isSpkOpened_ && event.type == HDF_AUDIO_EVENT_SPK_CLOSED) {
        DHLOGE("Render device status error, close render.");
        bool destroyStatus = true;
        for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
            if (DestroyRender(i) != DH_SUCCESS) {
                destroyStatus = false;
            }
        }
        return destroyStatus ? DH_SUCCESS : ERR_DH_AUDIO_HDF_FAIL;
    } else if (isMicOpened_ && event.type == HDF_AUDIO_EVENT_MIC_CLOSED) {
        DHLOGE("Capture device status error, close capture.");
        bool capCloseStatus = true;
        for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
            if (DestroyCapture(i) != DH_SUCCESS) {
                capCloseStatus = false;
            }
        }
        return capCloseStatus ? DH_SUCCESS : ERR_DH_AUDIO_HDF_FAIL;
    }
    DHLOGI("Handle device closed success.");
    return DH_SUCCESS;
}

bool AudioAdapterInterfaceImpl::IsPortsNoReg()
{
    std::lock_guard<std::mutex> devLck(devMapMtx_);
    return mapAudioDevice_.empty();
}

inline bool AudioAdapterInterfaceImpl::IsIdValid(const uint32_t id)
{
    if (id >= MAX_AUDIO_STREAM_NUM) {
        DHLOGE("Current id:%u is not supported.", id);
        return false;
    }
    return true;
}

bool AudioAdapterInterfaceImpl::CheckRendersValid()
{
    {
        std::lock_guard<std::mutex> devLck(renderDevMtx_);
        for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
            if (renderDevs_[i].second != nullptr) {
                DHLOGI("Containing active render.");
                return true;
            }
        }
    }
    return false;
}

bool AudioAdapterInterfaceImpl::CheckCapsValid()
{
    {
        std::lock_guard<std::mutex> devLck(capDevMtx_);
        for (uint32_t i = 0; i < MAX_AUDIO_STREAM_NUM; i++) {
            if (captureDevs_[i].second != nullptr) {
                DHLOGI("Containing active capture.");
                return true;
            }
        }
    }
    return false;
}

void AudioAdapterInterfaceImpl::SetDumpFlag(bool isRender)
{
    if (isRender) {
        std::lock_guard<std::mutex> renderLck(renderDevMtx_);
        for (auto item : renderDevs_) {
            auto render = item.second;
            if (render == nullptr) {
                continue;
            }
            render->SetDumpFlagInner();
        }
    } else {
        std::lock_guard<std::mutex> capLck(capDevMtx_);
        for (auto item : captureDevs_) {
            auto capture = item.second;
            if (capture == nullptr) {
                continue;
            }
            capture->SetDumpFlagInner();
        }
    }
}
} // V1_0
} // Audio
} // Distributedaudio
} // HDI
} // OHOS