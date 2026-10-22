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
#include <cstdlib>
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

#define AUDIO_CHANNELCOUNT             2
#define AUDIO_SAMPLE_RATE_48K          48000
#define DEEP_BUFFER_RENDER_PERIOD_SIZE 4096
#define PCM_16_BIT                     16
#define PCM_8_BIT                      8
#define INT_32_MAX                     0x7fffffff
#define AUDIO_ADAPTER_NUM_MAX          5
#define AUDIO_THREAD_NUM               4
#define AUDIO_HALF_OF_MAX_VOLUME       0.5f

namespace {

class HdfAudioConcRenderTest : public testing::Test {
public:
    struct IAudioManager *manager_ = nullptr;
    struct IAudioAdapter *adapter_ = nullptr;
    struct AudioAdapterDescriptor *adapterDescs_ = nullptr;
    struct IAudioRender *render_ = nullptr;
    uint32_t renderId_ = 0;
    uint32_t frameByteLen_ = 0;
    char *devDescriptorName_ = nullptr;
    virtual void SetUp();
    virtual void TearDown();
    void InitAttrs(struct AudioSampleAttributes &attrs);
    void InitDevDesc(struct AudioDeviceDescriptor &devDesc);
    void AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock, bool freeSelf);
    void ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen);
};

void HdfAudioConcRenderTest::AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock,
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

void HdfAudioConcRenderTest::ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen)
{
    if ((descsLen > 0) && (descs != nullptr) && ((*descs) != nullptr)) {
        for (uint32_t i = 0; i < descsLen; i++) {
            AudioAdapterDescriptorFree(&(*descs)[i], false);
        }
        OsalMemFree(*descs);
        *descs = nullptr;
    }
}

void HdfAudioConcRenderTest::InitAttrs(struct AudioSampleAttributes &attrs)
{
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = 1;
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (attrs.frameSize);
    attrs.stopThreshold = INT_32_MAX;
}

void HdfAudioConcRenderTest::InitDevDesc(struct AudioDeviceDescriptor &devDesc)
{
    ASSERT_NE(adapterDescs_, nullptr);
    ASSERT_NE(adapterDescs_->ports, nullptr);
    for (uint32_t index = 0; index < adapterDescs_->portsLen; index++) {
        if (adapterDescs_->ports[index].dir == PORT_OUT) {
            devDesc.portId = adapterDescs_->ports[index].portId;
            return;
        }
    }
}

void HdfAudioConcRenderTest::SetUp()
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

    if (manager_->LoadAdapter(manager_, &adapterDescs_[0], &adapter_) != HDF_SUCCESS) {
        ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
        ASSERT_TRUE(false);
    }
    if (adapter_ == nullptr) {
        ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
        ASSERT_TRUE(false);
    }

    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitDevDesc(devDesc);
    devDescriptorName_ = strdup("cardname");
    ASSERT_NE(devDescriptorName_, nullptr);
    devDesc.desc = devDescriptorName_;
    devDesc.pins = PIN_OUT_SPEAKER;
    InitAttrs(attrs);
    attrs.silenceThreshold = 0;
    attrs.streamId = 0;

    int32_t ret = adapter_->CreateRender(adapter_, &devDesc, &attrs, &render_, &renderId_);
    if (ret != HDF_SUCCESS) {
        attrs.format = AUDIO_FORMAT_TYPE_PCM_32_BIT;
        ASSERT_EQ(HDF_SUCCESS, adapter_->CreateRender(adapter_, &devDesc, &attrs, &render_, &renderId_));
    }
    ASSERT_NE(render_, nullptr);

    // frame bytes used by RenderFrame, query the real value from driver first
    uint64_t frameSize = 0;
    if ((render_->GetFrameSize(render_, &frameSize) == HDF_SUCCESS) && (frameSize > 0) &&
        (frameSize <= DEEP_BUFFER_RENDER_PERIOD_SIZE)) {
        frameByteLen_ = static_cast<uint32_t>(frameSize * attrs.period);
    } else {
        frameByteLen_ = attrs.period * attrs.frameSize;
    }
}

void HdfAudioConcRenderTest::TearDown()
{
    ASSERT_NE(devDescriptorName_, nullptr);
    free(devDescriptorName_);
    devDescriptorName_ = nullptr;

    if (adapter_ != nullptr) {
        adapter_->DestroyRender(adapter_, renderId_);
        render_ = nullptr;
    }
    if (manager_ != nullptr && adapterDescs_ != nullptr) {
        manager_->UnloadAdapter(manager_, adapterDescs_[0].adapterName);
    }
    ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
    if (manager_ != nullptr) {
        IAudioManagerRelease(manager_, false);
        manager_ = nullptr;
    }
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcStartStopMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));
    std::atomic<int32_t> stopCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&stopCnt, this]() {
            if (render_->Stop(render_) == HDF_SUCCESS) {
                stopCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(1, stopCnt.load());
    render_->Stop(render_);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcGetAttrsMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, this]() {
            struct AudioSampleAttributes attrs = {};
            if (render_->GetSampleAttributes(render_, &attrs) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
    render_->Stop(render_);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcPauseResumeMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));

    std::atomic<int32_t> pauseCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&pauseCnt, this]() {
            if (render_->Pause(render_) == HDF_SUCCESS) {
                pauseCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(1, pauseCnt.load());

    std::atomic<int32_t> resumeCnt(0);
    threads.clear();
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&resumeCnt, this]() {
            if (render_->Resume(render_) == HDF_SUCCESS) {
                resumeCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(1, resumeCnt.load());

    render_->Stop(render_);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcFlushMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));
    ASSERT_EQ(HDF_SUCCESS, render_->Pause(render_));

    std::atomic<int32_t> flushCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&flushCnt, this]() {
            if (render_->Flush(render_) == HDF_SUCCESS) {
                flushCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    render_->Resume(render_);
    render_->Stop(render_);

    EXPECT_GE(flushCnt.load(), 1);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcGetLatencyAndPositionMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));

    std::atomic<int32_t> latencyCnt(0);
    std::atomic<int32_t> positionCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&latencyCnt, &positionCnt, this]() {
            uint32_t ms = 0;
            if (render_->GetLatency(render_, &ms) == HDF_SUCCESS) {
                latencyCnt++;
            }
            uint64_t frames = 0;
            struct AudioTimeStamp time = {};
            if (render_->GetRenderPosition(render_, &frames, &time) == HDF_SUCCESS) {
                positionCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    render_->Stop(render_);

    EXPECT_EQ(AUDIO_THREAD_NUM, latencyCnt.load());
    EXPECT_GE(positionCnt.load(), 1);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcGetFrameCountSizeMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> countCnt(0);
    std::atomic<int32_t> sizeCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&countCnt, &sizeCnt, this]() {
            uint64_t count = 0;
            if (render_->GetFrameCount(render_, &count) == HDF_SUCCESS) {
                countCnt++;
            }
            uint64_t size = 0;
            if (render_->GetFrameSize(render_, &size) == HDF_SUCCESS) {
                sizeCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(AUDIO_THREAD_NUM, countCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM, sizeCnt.load());
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcSetGetVolumeMutiThread001, TestSize.Level1)
{
    float originalVolume = 0.0;
    (void)render_->GetVolume(render_, &originalVolume);

    std::atomic<int32_t> getCnt(0);
    std::atomic<int32_t> setCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&getCnt, &setCnt, this]() {
            if (render_->SetVolume(render_, AUDIO_HALF_OF_MAX_VOLUME) == HDF_SUCCESS) {
                setCnt++;
            }
            float volume = 0.0;
            if (render_->GetVolume(render_, &volume) == HDF_SUCCESS) {
                getCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // restore the original volume
    (void)render_->SetVolume(render_, originalVolume);

    EXPECT_EQ(AUDIO_THREAD_NUM, getCnt.load());
    EXPECT_GE(setCnt.load(), 1);
}

HWTEST_F(HdfAudioConcRenderTest, HdfAudioRenderConcRenderFrameMutiThread001, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, render_->Start(render_));
    ASSERT_GT(frameByteLen_, 0);

    std::atomic<int32_t> writeCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&writeCnt, this]() {
            std::vector<int8_t> frame(frameByteLen_, 0);
            uint64_t replyBytes = 0;
            if (render_->RenderFrame(render_, frame.data(), frameByteLen_, &replyBytes) == HDF_SUCCESS) {
                writeCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    render_->Stop(render_);

    EXPECT_GE(writeCnt.load(), 1);
}

} // namespace
