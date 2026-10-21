/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <atomic>
#include <climits>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "hdf_dlist.h"
#include "osal_mem.h"
#include "v6_1/audio_types.h"
#include "v6_1/iaudio_adapter.h"
#include "v6_1/iaudio_manager.h"

using namespace std;
using namespace testing::ext;

#define AUDIO_CHANNELCOUNT              2
#define AUDIO_SAMPLE_RATE_48K           48000
#define DEEP_BUFFER_CAPTURE_PERIOD_SIZE 4096
#define PCM_16_BIT                      16
#define PCM_8_BIT                       8
#define INT_32_MAX                      0x7fffffff
#define AUDIO_ADAPTER_NUM_MAX           5
#define AUDIO_THREAD_NUM                4
#define AUDIO_HALF_OF_MAX_VOLUME        0.5f

namespace {

class HdfAudioConcCaptureTest : public testing::Test {
public:
    struct IAudioManager *manager_ = nullptr;
    struct IAudioAdapter *adapter_ = nullptr;
    struct AudioAdapterDescriptor *adapterDescs_ = nullptr;
    uint32_t adapterNum_ = 0;
    virtual void SetUp();
    virtual void TearDown();
    void InitAttrs(struct AudioSampleAttributes &attrs);
    void InitDevDesc(struct AudioDeviceDescriptor &devDesc);
    int32_t CreateCaptureHelper(struct IAudioCapture **capture, uint32_t *captureId, uint32_t *frameByteLen);
    void AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock, bool freeSelf);
    void ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen);
};

void HdfAudioConcCaptureTest::AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock,
    bool freeSelf)
{
    if (dataBlock == nullptr) {
        return;
    }

    if (dataBlock->adapterName != nullptr) {
        OsalMemFree(dataBlock->adapterName);
        dataBlock->adapterName = nullptr;
    }

    if (dataBlock->ports != nullptr) {
        OsalMemFree(dataBlock->ports);
    }

    if (freeSelf) {
        OsalMemFree(dataBlock);
    }
}

void HdfAudioConcCaptureTest::ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen)
{
    if ((descsLen > 0) && (descs != nullptr) && ((*descs) != nullptr)) {
        for (uint32_t i = 0; i < descsLen; i++) {
            AudioAdapterDescriptorFree(&(*descs)[i], false);
        }
        OsalMemFree(*descs);
        *descs = nullptr;
    }
}

void HdfAudioConcCaptureTest::InitAttrs(struct AudioSampleAttributes &attrs)
{
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = 1;
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_CAPTURE_PERIOD_SIZE;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_CAPTURE_PERIOD_SIZE / (attrs.frameSize);
    attrs.stopThreshold = INT_32_MAX;
}

void HdfAudioConcCaptureTest::InitDevDesc(struct AudioDeviceDescriptor &devDesc)
{
    ASSERT_NE(adapterDescs_, nullptr);
    ASSERT_NE(adapterDescs_->ports, nullptr);
    for (uint32_t index = 0; index < adapterDescs_->portsLen; index++) {
        if (adapterDescs_->ports[index].dir == PORT_IN) {
            devDesc.portId = adapterDescs_->ports[index].portId;
            return;
        }
    }
}

int32_t HdfAudioConcCaptureTest::CreateCaptureHelper(struct IAudioCapture **capture, uint32_t *captureId,
    uint32_t *frameByteLen)
{
    if ((capture == nullptr) || (captureId == nullptr) || (frameByteLen == nullptr)) {
        return HDF_ERR_INVALID_PARAM;
    }
    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitDevDesc(devDesc);
    devDesc.desc = const_cast<char *>("primary");
    devDesc.pins = PIN_IN_MIC;
    InitAttrs(attrs);
    attrs.silenceThreshold = 0;
    attrs.streamId = 0;

    int32_t ret = adapter_->CreateCapture(adapter_, &devDesc, &attrs, capture, captureId);
    if (ret != HDF_SUCCESS) {
        attrs.format = AUDIO_FORMAT_TYPE_PCM_32_BIT;
        ret = adapter_->CreateCapture(adapter_, &devDesc, &attrs, capture, captureId);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
    }

    // frame bytes used by CaptureFrame, query the real value from driver first
    uint64_t frameSize = 0;
    if (((*capture)->GetFrameSize(*capture, &frameSize) == HDF_SUCCESS) && (frameSize > 0) &&
        (frameSize <= DEEP_BUFFER_CAPTURE_PERIOD_SIZE)) {
        *frameByteLen = static_cast<uint32_t>(frameSize * attrs.period);
    } else {
        *frameByteLen = attrs.period * attrs.frameSize;
    }
    return HDF_SUCCESS;
}

void HdfAudioConcCaptureTest::SetUp()
{
    uint32_t size = AUDIO_ADAPTER_NUM_MAX;
    manager_ = IAudioManagerGet(false);
    ASSERT_NE(manager_, nullptr);

    adapterDescs_ = (struct AudioAdapterDescriptor *)OsalMemCalloc(
        sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
    ASSERT_NE(adapterDescs_, nullptr);

    ASSERT_EQ(HDF_SUCCESS, manager_->GetAllAdapters(manager_, adapterDescs_, &size));
    if (size > AUDIO_ADAPTER_NUM_MAX) {
        ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
        ASSERT_LT(size, AUDIO_ADAPTER_NUM_MAX);
    }
    adapterNum_ = size;

    if (manager_->LoadAdapter(manager_, &adapterDescs_[0], &adapter_) != HDF_SUCCESS) {
        ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
        ASSERT_TRUE(false);
    }
    if (adapter_ == nullptr) {
        ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
        ASSERT_TRUE(false);
    }
}

void HdfAudioConcCaptureTest::TearDown()
{
    ASSERT_NE(manager_, nullptr);
    ASSERT_NE(adapter_, nullptr);

    manager_->UnloadAdapter(manager_, adapterDescs_[0].adapterName);
    ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
    adapter_ = nullptr;
    IAudioManagerRelease(manager_, false);
    manager_ = nullptr;
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcCreateCaptureMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<uint32_t> captureIds(AUDIO_THREAD_NUM, 0);
    std::vector<int32_t> results(AUDIO_THREAD_NUM, HDF_FAILURE);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, &captureIds, &results, i, this]() {
            struct IAudioCapture *capture = nullptr;
            struct AudioDeviceDescriptor devDesc = {};
            struct AudioSampleAttributes attrs = {};
            InitDevDesc(devDesc);
            devDesc.desc = const_cast<char *>("primary");
            devDesc.pins = PIN_IN_MIC;
            InitAttrs(attrs);
            attrs.silenceThreshold = 0;
            attrs.streamId = 0;
            results[i] = adapter_->CreateCapture(adapter_, &devDesc, &attrs, &capture, &captureIds[i]);
            if (results[i] == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        if (results[i] == HDF_SUCCESS) {
            adapter_->DestroyCapture(adapter_, captureIds[i]);
        }
    }
    EXPECT_GE(successCnt.load(), 1);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcStartStopMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);

    ASSERT_EQ(HDF_SUCCESS, capture->Start(capture));
    std::atomic<int32_t> stopCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&stopCnt, capture]() {
            if (capture->Stop(capture) == HDF_SUCCESS) {
                stopCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(1, stopCnt.load());
    capture->Stop(capture);

    adapter_->DestroyCapture(adapter_, captureId);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcGetAttrsMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);

    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, capture]() {
            struct AudioSampleAttributes attrs = {};
            if (capture->GetSampleAttributes(capture, &attrs) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());

    adapter_->DestroyCapture(adapter_, captureId);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcGetFrameCountSizeMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);

    std::atomic<int32_t> countCnt(0);
    std::atomic<int32_t> sizeCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&countCnt, &sizeCnt, capture]() {
            uint64_t count = 0;
            if (capture->GetFrameCount(capture, &count) == HDF_SUCCESS) {
                countCnt++;
            }
            uint64_t size = 0;
            if (capture->GetFrameSize(capture, &size) == HDF_SUCCESS) {
                sizeCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(AUDIO_THREAD_NUM, countCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM, sizeCnt.load());

    adapter_->DestroyCapture(adapter_, captureId);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcCaptureFrameMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);
    ASSERT_GT(frameByteLen, 0);

    ASSERT_EQ(HDF_SUCCESS, capture->Start(capture));
    std::atomic<int32_t> readCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&readCnt, capture, frameByteLen]() {
            std::vector<int8_t> frame(frameByteLen, 0);
            uint32_t frameLen = frameByteLen;
            uint64_t replyBytes = 0;
            if (capture->CaptureFrame(capture, frame.data(), &frameLen, &replyBytes) == HDF_SUCCESS) {
                readCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    capture->Stop(capture);

    EXPECT_GE(readCnt.load(), 1);

    adapter_->DestroyCapture(adapter_, captureId);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcSetGetVolumeMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);

    float originalVolume = 0.0;
    (void)capture->GetVolume(capture, &originalVolume);

    std::atomic<int32_t> getCnt(0);
    std::atomic<int32_t> setCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&getCnt, &setCnt, capture]() {
            if (capture->SetVolume(capture, AUDIO_HALF_OF_MAX_VOLUME) == HDF_SUCCESS) {
                setCnt++;
            }
            float volume = 0.0;
            if (capture->GetVolume(capture, &volume) == HDF_SUCCESS) {
                getCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // restore the original volume
    (void)capture->SetVolume(capture, originalVolume);

    EXPECT_EQ(AUDIO_THREAD_NUM, getCnt.load());
    EXPECT_GE(setCnt.load(), 1);

    adapter_->DestroyCapture(adapter_, captureId);
}

HWTEST_F(HdfAudioConcCaptureTest, HdfAudioCaptureConcGetCapturePositionMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    uint32_t frameByteLen = 0;
    ASSERT_EQ(HDF_SUCCESS, CreateCaptureHelper(&capture, &captureId, &frameByteLen));
    ASSERT_NE(capture, nullptr);

    ASSERT_EQ(HDF_SUCCESS, capture->Start(capture));
    std::atomic<int32_t> positionCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&positionCnt, capture]() {
            uint64_t frames = 0;
            struct AudioTimeStamp time = {};
            if (capture->GetCapturePosition(capture, &frames, &time) == HDF_SUCCESS) {
                positionCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    capture->Stop(capture);

    EXPECT_GE(positionCnt.load(), 1);

    adapter_->DestroyCapture(adapter_, captureId);
}

} // namespace
