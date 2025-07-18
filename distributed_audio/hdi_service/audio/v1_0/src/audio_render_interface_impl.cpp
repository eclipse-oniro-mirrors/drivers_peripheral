/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "audio_render_interface_impl.h"

#include <hdf_base.h>
#include <unistd.h>
#include "sys/time.h"

#include "cJSON.h"
#include "daudio_constants.h"
#include "daudio_events.h"
#include "daudio_log.h"
#include "daudio_utils.h"

#undef DH_LOG_TAG
#define DH_LOG_TAG "AudioRenderInterfaceImpl"

using namespace OHOS::DistributedHardware;
namespace OHOS {
namespace HDI {
namespace DistributedAudio {
namespace Audio {
namespace V1_0 {
AudioRenderInterfaceImpl::AudioRenderInterfaceImpl(const std::string &adpName, const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, const sptr<IDAudioCallback> &callback, uint32_t renderId)
    : adapterName_(adpName), devDesc_(desc),
    devAttrs_(attrs), renderId_(renderId), audioExtCallback_(callback)
{
    devAttrs_.frameSize = CalculateFrameSize(attrs.sampleRate, attrs.channelCount, attrs.format,
        AUDIO_NORMAL_INTERVAL, false);
    DHLOGD("Distributed audio render constructed, period(%{public}d), frameSize(%{public}d).",
        attrs.period, devAttrs_.frameSize);
}

AudioRenderInterfaceImpl::~AudioRenderInterfaceImpl()
{
    DHLOGD("Distributed audio render destructed, id(%{public}d).", devDesc_.pins);
}

sptr<IAudioCallback> AudioRenderInterfaceImpl::GetAudioCallback()
{
    return renderCallback_;
}

int32_t AudioRenderInterfaceImpl::GetLatency(uint32_t &ms)
{
    DHLOGD("Get render device latency.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    if (audioExtCallback_->GetLatency(renderId_, ms) != HDF_SUCCESS) {
        DHLOGE("Get render device latency failed.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

float AudioRenderInterfaceImpl::GetFadeRate(uint32_t currentIndex, const uint32_t durationIndex)
{
    if (currentIndex > durationIndex || durationIndex == 0) {
        return 1.0f;
    }

    float fadeRate = static_cast<float>(currentIndex) / durationIndex * DAUDIO_FADE_NORMALIZATION_FACTOR;
    if (fadeRate < 1) {
        return pow(fadeRate, DAUDIO_FADE_POWER_NUM) / DAUDIO_FADE_NORMALIZATION_FACTOR;
    }
    return -pow(fadeRate - DAUDIO_FADE_MAXIMUM_VALUE, DAUDIO_FADE_POWER_NUM) /
        DAUDIO_FADE_NORMALIZATION_FACTOR + 1;
}

int32_t AudioRenderInterfaceImpl::FadeInProcess(const uint32_t durationFrame,
    int8_t *frameData, const size_t frameLength)
{
    if (frameLength > RENDER_MAX_FRAME_SIZE) {
        DHLOGE("The frameLength is over max length.");
        return HDF_ERR_INVALID_PARAM;
    }
    int16_t* frame = reinterpret_cast<int16_t *>(frameData);
    const size_t newFrameLength = frameLength / 2;

    for (size_t k = 0; k < newFrameLength; ++k) {
        float rate = GetFadeRate(currentFrame_ * newFrameLength + k, durationFrame * newFrameLength);
        frame[k] = currentFrame_ == durationFrame - 1 ? frame[k] : static_cast<int16_t>(rate * frame[k]);
    }
    if (currentFrame_ < durationFrame - 1) {
        DHLOGD("Fade-in frame[currentFrame: %{public}d].", currentFrame_);
    }
    ++currentFrame_;
    currentFrame_ = currentFrame_ >= durationFrame ? durationFrame - 1 : currentFrame_;

    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::RenderFrame(const std::vector<int8_t> &frame, uint64_t &replyBytes)
{
    DHLOGD("Render frame[sampleRate: %{public}u, channelCount: %{public}u, format: %{public}d, frameSize: %{public}u].",
        devAttrs_.sampleRate, devAttrs_.channelCount, devAttrs_.format, devAttrs_.frameSize);

    int64_t startTime = GetNowTimeUs();
    std::lock_guard<std::mutex> renderLck(renderMtx_);
    if (renderStatus_ != RENDER_STATUS_START) {
        DHLOGE("Render status wrong, return false.");
        return HDF_ERR_DEVICE_BUSY;
    }

    AudioParameter param = { devAttrs_.format, devAttrs_.channelCount, devAttrs_.sampleRate, 0,
        devAttrs_.frameSize, devAttrs_.type};
    AudioData data = { param, frame };
    DumpFileUtil::WriteDumpFile(dumpFile_, static_cast<void *>(data.data.data()), frame.size());
    if (enableFade_ && (currentFrame_ < DURATION_FRAMES_MINUS)) {
        FadeInProcess(DURATION_FRAMES, data.data.data(), frame.size());
    }
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }
#ifdef DAUDIO_SUPPORT_SHARED_BUFFER
    if (WriteToShmem(data) != HDF_SUCCESS) {
        DHLOGE("Write stream data failed.");
        return HDF_FAILURE;
    }
#else
    if (audioExtCallback_->WriteStreamData(renderId_, data) != HDF_SUCCESS) {
        DHLOGE("Write stream data failed.");
        return HDF_FAILURE;
    }
#endif

    replyBytes = data.data.size();
    ++frameIndex_;
    DHLOGD("Render audio frame success.");
    int64_t endTime = GetNowTimeUs();
    if (IsOutDurationRange(startTime, endTime, lastRenderStartTime_)) {
        DHLOGE("This frame spend: %{public}" PRId64" us, interval of two frames: %{public}" PRId64 " us",
            endTime - startTime, startTime - lastRenderStartTime_);
    }
    lastRenderStartTime_ = startTime;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetRenderPosition(uint64_t &frames, AudioTimeStamp &time)
{
    DHLOGI("Get render position.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    CurrentTime currentTime;
    currentTime.tvSec = time.tvSec;
    currentTime.tvNSec = time.tvNSec;
    if (audioExtCallback_->GetRenderPosition(renderId_, frames, currentTime) != HDF_SUCCESS) {
        DHLOGE("Get render position failed.");
        return HDF_FAILURE;
    }
    time.tvSec = currentTime.tvSec;
    time.tvNSec = currentTime.tvNSec;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetRenderSpeed(float speed)
{
    DHLOGI("Set render speed.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    renderSpeed_ = speed;
    std::string content = std::to_string(speed);
    DAudioEvent event = { HDF_AUDIO_EVENT_SPEED_CHANGE, content};
    if (audioExtCallback_->NotifyEvent(renderId_, event) != HDF_SUCCESS) {
        DHLOGE("Set render speed failed.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetRenderSpeed(float &speed)
{
    DHLOGI("Get render speed, control render speed is not support yet.");
    speed = renderSpeed_;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetChannelMode(AudioChannelMode mode)
{
    DHLOGI("Set channel mode, control channel mode is not support yet.");
    channelMode_ = mode;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetChannelMode(AudioChannelMode &mode)
{
    DHLOGI("Get channel mode, control channel mode is not support yet.");
    mode = channelMode_;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::RegCallback(const sptr<IAudioCallback> &audioCallback, int8_t cookie)
{
    DHLOGI("Register render callback.");
    (void)cookie;
    renderCallback_ = audioCallback;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::DrainBuffer(AudioDrainNotifyType &type)
{
    DHLOGI("Drain audio buffer, not support yet.");
    (void)type;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::IsSupportsDrain(bool &support)
{
    DHLOGI("Check whether drain is supported, not support yet.");
    (void)support;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::Start()
{
    DHLOGI("Start render.");
    if (firstOpenFlag_) {
        firstOpenFlag_ = false;
    } else {
        cJSON *jParam = cJSON_CreateObject();
        if (jParam == nullptr) {
            DHLOGE("Failed to create cJSON object.");
            return HDF_FAILURE;
        }
        cJSON_AddStringToObject(jParam, KEY_DH_ID, std::to_string(devDesc_.pins).c_str());
        cJSON_AddStringToObject(jParam, "ChangeType", HDF_EVENT_RESTART.c_str());
        char *jsonData = cJSON_PrintUnformatted(jParam);
        if (jsonData == nullptr) {
            DHLOGE("Failed to create JSON data.");
            cJSON_Delete(jParam);
            return HDF_FAILURE;
        }
        std::string content(jsonData);
        cJSON_Delete(jParam);
        cJSON_free(jsonData);
        DAudioEvent event = { HDF_AUDIO_EVENT_CHANGE_PLAY_STATUS, content};
        if (audioExtCallback_ == nullptr) {
            DHLOGE("Callback is nullptr.");
            return HDF_FAILURE;
        }
        int32_t ret = audioExtCallback_->NotifyEvent(renderId_, event);
        if (ret != HDF_SUCCESS) {
            DHLOGE("Restart failed.");
        }
    }
#ifdef DAUDIO_SUPPORT_SHARED_BUFFER
    writeIndex_ = 0;
    writeNum_ = 0;
    if (NotifyFirstChangeEvent(HDF_AUDIO_EVENT_START) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
#endif
    
    std::lock_guard<std::mutex> renderLck(renderMtx_);
    renderStatus_ = RENDER_STATUS_START;
    currentFrame_ = CUR_FRAME_INIT_VALUE;
    frameIndex_ = 0;
    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_HDF_RENDER_To_SA, &dumpFile_);
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::Stop()
{
    DHLOGI("Stop render.");
#ifdef DAUDIO_SUPPORT_SHARED_BUFFER
    writeIndex_ = 0;
    writeNum_ = 0;
#endif
    cJSON *jParam = cJSON_CreateObject();
    if (jParam == nullptr) {
        DHLOGE("Failed to create cJSON object.");
        return HDF_FAILURE;
    }
    cJSON_AddStringToObject(jParam, KEY_DH_ID, std::to_string(devDesc_.pins).c_str());
    cJSON_AddStringToObject(jParam, "ChangeType", HDF_EVENT_PAUSE.c_str());
    char *jsonData = cJSON_PrintUnformatted(jParam);
    if (jsonData == nullptr) {
        DHLOGE("Failed to create JSON data.");
        cJSON_Delete(jParam);
        return HDF_FAILURE;
    }
    std::string content(jsonData);
    cJSON_Delete(jParam);
    cJSON_free(jsonData);
    DAudioEvent event = { HDF_AUDIO_EVENT_CHANGE_PLAY_STATUS, content};
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }
    int32_t ret = audioExtCallback_->NotifyEvent(renderId_, event);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Pause and clear cache streams failed.");
    }
    std::lock_guard<std::mutex> renderLck(renderMtx_);
    renderStatus_ = RENDER_STATUS_STOP;
    DumpFileUtil::CloseDumpFile(&dumpFile_);
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::Pause()
{
    DHLOGI("Pause render.");
    std::lock_guard<std::mutex> renderLck(renderMtx_);
    renderStatus_ = RENDER_STATUS_PAUSE;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::Resume()
{
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::Flush()
{
    DHLOGI("Flush.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    std::string content;
    DAudioEvent event = { HDF_AUDIO_EVENT_FLUSH, content};
    if (audioExtCallback_->NotifyEvent(renderId_, event) != HDF_SUCCESS) {
        DHLOGE("Flush failed.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::TurnStandbyMode()
{
    DHLOGI("Turn stand by mode, not support yet.");
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::AudioDevDump(int32_t range, int32_t fd)
{
    DHLOGI("Dump audio info, not support yet.");
    (void)range;
    (void)fd;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::IsSupportsPauseAndResume(bool &supportPause, bool &supportResume)
{
    DHLOGI("Check whether pause and resume is supported, not support yet.");
    (void)supportPause;
    (void)supportResume;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::CheckSceneCapability(const AudioSceneDescriptor &scene, bool &supported)
{
    DHLOGI("Check scene capability.");
    (void)scene;
    (void)supported;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SelectScene(const AudioSceneDescriptor &scene)
{
    DHLOGI("Select audio scene, not support yet.");
    (void)scene;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetMute(bool mute)
{
    DHLOGI("Set mute, not support yet.");
    (void)mute;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetMute(bool &mute)
{
    DHLOGI("Get mute, not support yet.");
    (void)mute;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetVolume(float volume)
{
    DHLOGI("Set volume.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    std::string content = std::to_string(volume);
    DAudioEvent event = { HDF_AUDIO_EVENT_VOLUME_CHANGE, content};
    if (audioExtCallback_->NotifyEvent(renderId_, event) != HDF_SUCCESS) {
        DHLOGE("Set volume failed.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetVolume(float &volume)
{
    DHLOGI("Can not get vol not by this interface.");
    (void)volume;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetGainThreshold(float &min, float &max)
{
    DHLOGI("Get gain threshold, not support yet.");
    min = 0;
    max = 0;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetGain(float gain)
{
    DHLOGI("Set gain, not support yet.");
    (void)gain;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetGain(float &gain)
{
    DHLOGI("Get gain, not support yet.");
    gain = 1.0;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetFrameSize(uint64_t &size)
{
    (void)size;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetFrameCount(uint64_t &count)
{
    (void)count;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetSampleAttributes(const AudioSampleAttributes &attrs)
{
    DHLOGI("Set sample attributes.");
    devAttrs_ = attrs;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetSampleAttributes(AudioSampleAttributes &attrs)
{
    DHLOGI("Get sample attributes.");
    attrs = devAttrs_;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetCurrentChannelId(uint32_t &channelId)
{
    DHLOGI("Get current channel id, not support yet.");
    (void)channelId;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::SetExtraParams(const std::string &keyValueList)
{
    DHLOGI("Set extra parameters, not support yet.");
    (void)keyValueList;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetExtraParams(std::string &keyValueList)
{
    DHLOGI("Get extra parameters, not support yet.");
    (void)keyValueList;
    return HDF_SUCCESS;
}

#ifdef DAUDIO_SUPPORT_SHARED_BUFFER
int32_t AudioRenderInterfaceImpl::CreateAshmem(int32_t ashmemLength)
{
    const std::string memory_name = "Normal Render";
    if (ashmemLength < DAUDIO_MIN_ASHMEM_LEN || ashmemLength > DAUDIO_MAX_ASHMEM_LEN) {
        DHLOGE("The length of shared memory is illegal.");
        return HDF_FAILURE;
    }
    ashmem_ = OHOS::Ashmem::CreateAshmem(memory_name.c_str(), ashmemLength);
    if (ashmem_ == nullptr) {
        DHLOGE("Create ashmem failed.");
        return HDF_FAILURE;
    }
    bool ret = ashmem_->MapReadAndWriteAshmem();
    if (ret != true) {
        DHLOGE("Mmap ashmem failed.");
        return HDF_FAILURE;
    }
    fd_ = ashmem_->GetAshmemFd();
    DHLOGI("Init Ashmem success, fd: %{public}d, length: %{public}d", fd_, ashmemLength);
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::WriteToShmem(const AudioData &data)
{
    DHLOGD("Write to shared memory.");
    if (ashmem_ == nullptr) {
        DHLOGE("Share memory is null");
        return HDF_FAILURE;
    }
    if (data.param.frameSize != static_cast<uint32_t>(lengthPerTrans_)) {
        DHLOGE("The param:framsize is invalid.");
        return HDF_FAILURE;
    }
    const auto &audioData = data.data;
    bool writeRet = ashmem_->WriteToAshmem(audioData.data(), audioData.size(), writeIndex_);
    if (writeRet) {
        DHLOGD("Write to ashmem success! write index: %{public}d, writeLength: %{public}d.",
            writeIndex_, lengthPerTrans_);
    } else {
        DHLOGE("Write data to ashmem failed.");
    }
    writeIndex_ += lengthPerTrans_;
    if (writeIndex_ >= ashmemLength_) {
        writeIndex_ = 0;
    }
    writeNum_ += CalculateSampleNum(devAttrs_.sampleRate, timeInterval_);
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::NotifyFirstChangeEvent(EXT_PARAM_EVENT evetType)
{
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Ext callback is null.");
        return HDF_FAILURE;
    }
    std::string content;
    std::initializer_list<std::pair<std::string, std::string>> items = { {KEY_DH_ID, std::to_string(devDesc_.pins)} };
    if (WrapCJsonItem(items, content) != HDF_SUCCESS) {
        DHLOGE("Wrap the event failed.");
        return HDF_FAILURE;
    }
    DAudioEvent event = { evetType, content };
    int32_t ret = audioExtCallback_->NotifyEvent(renderId_, event);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Start render mmap failed.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
#endif

int32_t AudioRenderInterfaceImpl::ReqMmapBuffer(int32_t reqSize, AudioMmapBufferDescriptor &desc)
{
#ifdef DAUDIO_SUPPORT_SHARED_BUFFER
    DHLOGI("Request mmap buffer.");
    int32_t minSize = CalculateSampleNum(devAttrs_.sampleRate, minTimeInterval_);
    int32_t maxSize = CalculateSampleNum(devAttrs_.sampleRate, maxTimeInterval_);
    int32_t realSize = reqSize;
    if (reqSize < minSize) {
        realSize = minSize;
    } else if (reqSize > maxSize) {
        realSize = maxSize;
    }
    DHLOGI("ReqMmap buffer realsize : %{public}d, minsize: %{public}d, maxsize:%{public}d.",
        realSize, minSize, maxSize);
    desc.totalBufferFrames = realSize;
    ashmemLength_ = realSize * static_cast<int32_t>(devAttrs_.channelCount) * devAttrs_.format;
    DHLOGI("Init ashmem real sample size : %{public}d, length: %{public}d.", realSize, ashmemLength_);
    int32_t ret = CreateAshmem(ashmemLength_);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Init ashmem error..");
        return HDF_FAILURE;
    }
    desc.memoryFd = fd_;
    desc.transferFrameSize = static_cast<int32_t>(CalculateSampleNum(devAttrs_.sampleRate, timeInterval_));
    lengthPerTrans_ = desc.transferFrameSize * static_cast<int32_t>(devAttrs_.channelCount) * devAttrs_.format;
    desc.isShareable = false;
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }
    ret = audioExtCallback_->RefreshAshmemInfo(renderId_, fd_, ashmemLength_, lengthPerTrans_);
    if (ret != HDF_SUCCESS) {
        DHLOGE("Refresh ashmem info failed.");
        return HDF_FAILURE;
    }
#else
    DHLOGI("Request mmap buffer.");
    if (audioExtCallback_ == nullptr) {
        DHLOGE("Callback is nullptr.");
        return HDF_FAILURE;
    }

    std::string content = std::to_string(reqSize);
    DAudioEvent event = { HDF_AUDIO_OFFLOAD_BUF_SIZE_CHANGE, content};
    if (audioExtCallback_->NotifyEvent(renderId_, event) != HDF_SUCCESS) {
        DHLOGE("Set volume failed.");
        return HDF_FAILURE;
    }
#endif
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetMmapPosition(uint64_t &frames, AudioTimeStamp &time)
{
    DHLOGI("Get mmap position, not support yet.");
    (void)frames;
    (void)time;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::AddAudioEffect(uint64_t effectid)
{
    DHLOGI("Add audio effect, not support yet.");
    (void)effectid;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::RemoveAudioEffect(uint64_t effectid)
{
    DHLOGI("Remove audio effect, not support yet.");
    (void)effectid;
    return HDF_SUCCESS;
}

int32_t AudioRenderInterfaceImpl::GetFrameBufferSize(uint64_t &bufferSize)
{
    DHLOGI("Get frame buffer size, not support yet.");
    (void)bufferSize;
    return HDF_SUCCESS;
}

const AudioDeviceDescriptor &AudioRenderInterfaceImpl::GetRenderDesc()
{
    return devDesc_;
}

void AudioRenderInterfaceImpl::SetVolumeInner(const uint32_t vol)
{
    std::lock_guard<std::mutex> volLck(volMtx_);
    vol_ = vol;
}

void AudioRenderInterfaceImpl::SetVolumeRangeInner(const uint32_t volMax, const uint32_t volMin)
{
    std::lock_guard<std::mutex> volLck(volMtx_);
    volMin_ = volMin;
    volMax_ = volMax;
}

uint32_t AudioRenderInterfaceImpl::GetVolumeInner()
{
    std::lock_guard<std::mutex> volLck(volMtx_);
    return vol_;
}

uint32_t AudioRenderInterfaceImpl::GetMaxVolumeInner()
{
    std::lock_guard<std::mutex> volLck(volMtx_);
    return volMax_;
}

uint32_t AudioRenderInterfaceImpl::GetMinVolumeInner()
{
    std::lock_guard<std::mutex> volLck(volMtx_);
    return volMin_;
}

void AudioRenderInterfaceImpl::SetAttrs(const std::string &adpName, const AudioDeviceDescriptor &desc,
    const AudioSampleAttributes &attrs, const sptr<IDAudioCallback> &callback, const int32_t dhId)
{
    DHLOGI("Set attrs, not support yet.");
}

void AudioRenderInterfaceImpl::SetDumpFlagInner()
{
    dumpFlag_ = true;
}
} // V1_0
} // Audio
} // Distributedaudio
} // HDI
} // OHOS
