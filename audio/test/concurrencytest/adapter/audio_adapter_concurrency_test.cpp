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

#define AUDIO_CHANNELCOUNT             2
#define AUDIO_SAMPLE_RATE_48K          48000
#define DEEP_BUFFER_RENDER_PERIOD_SIZE 4096
#define PCM_16_BIT                     16
#define PCM_8_BIT                      8
#define INT_32_MAX                     0x7fffffff
#define AUDIO_ADAPTER_NUM_MAX          5
#define AUDIO_THREAD_NUM               4
#define AUDIO_CREATE_REPEAT_TIMES      4
#define AUDIO_HALF_OF_MAX_VOLUME       0.5f

namespace {

class HdfAudioConcAdapterTest : public testing::Test {
public:
    struct IAudioManager *manager_ = nullptr;
    struct IAudioAdapter *adapter_ = nullptr;
    struct AudioAdapterDescriptor *adapterDescs_ = nullptr;
    uint32_t adapterNum_ = 0;
    virtual void SetUp();
    virtual void TearDown();
    void InitAttrs(struct AudioSampleAttributes &attrs);
    void InitDevDesc(struct AudioDeviceDescriptor &devDesc, enum AudioPortDirection dir);
    void InitPort(struct AudioPort &port, enum AudioPortDirection dir);
    void CreateRenderDestroyChurnWorker(const struct AudioDeviceDescriptor *devDesc,
        const struct AudioSampleAttributes *attrs, std::atomic<int32_t> &createCnt,
        std::atomic<int32_t> &destroyCnt);
    void AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock, bool freeSelf);
    void ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen);
};

void HdfAudioConcAdapterTest::AudioAdapterDescriptorFree(struct AudioAdapterDescriptor *dataBlock,
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

void HdfAudioConcAdapterTest::ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen)
{
    if ((descsLen > 0) && (descs != nullptr) && ((*descs) != nullptr)) {
        for (uint32_t i = 0; i < descsLen; i++) {
            AudioAdapterDescriptorFree(&(*descs)[i], false);
        }
        OsalMemFree(*descs);
        *descs = nullptr;
    }
}

void HdfAudioConcAdapterTest::InitAttrs(struct AudioSampleAttributes &attrs)
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

void HdfAudioConcAdapterTest::InitDevDesc(struct AudioDeviceDescriptor &devDesc, enum AudioPortDirection dir)
{
    ASSERT_NE(adapterDescs_, nullptr);
    ASSERT_NE(adapterDescs_->ports, nullptr);
    for (uint32_t index = 0; index < adapterDescs_->portsLen; index++) {
        if (adapterDescs_->ports[index].dir == dir) {
            devDesc.portId = adapterDescs_->ports[index].portId;
            return;
        }
    }
}

void HdfAudioConcAdapterTest::InitPort(struct AudioPort &port, enum AudioPortDirection dir)
{
    ASSERT_NE(adapterDescs_, nullptr);
    ASSERT_NE(adapterDescs_->ports, nullptr);
    for (uint32_t index = 0; index < adapterDescs_->portsLen; index++) {
        if (adapterDescs_->ports[index].dir == dir) {
            port = adapterDescs_->ports[index];
            return;
        }
    }
}

void HdfAudioConcAdapterTest::CreateRenderDestroyChurnWorker(const struct AudioDeviceDescriptor *devDesc,
    const struct AudioSampleAttributes *attrs, std::atomic<int32_t> &createCnt, std::atomic<int32_t> &destroyCnt)
{
    for (uint32_t j = 0; j < AUDIO_CREATE_REPEAT_TIMES; j++) {
        struct IAudioRender *render = nullptr;
        uint32_t renderId = 0;
        if (adapter_->CreateRender(adapter_, devDesc, attrs, &render, &renderId) != HDF_SUCCESS) {
            continue;
        }
        createCnt++;
        if (adapter_->DestroyRender(adapter_, renderId) == HDF_SUCCESS) {
            destroyCnt++;
        }
    }
}

void HdfAudioConcAdapterTest::SetUp()
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

void HdfAudioConcAdapterTest::TearDown()
{
    ASSERT_NE(manager_, nullptr);
    ASSERT_NE(adapter_, nullptr);

    manager_->UnloadAdapter(manager_, adapterDescs_[0].adapterName);
    ReleaseAdapterDescs(&adapterDescs_, AUDIO_ADAPTER_NUM_MAX);
    adapter_ = nullptr;
    IAudioManagerRelease(manager_, false);
    manager_ = nullptr;
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcInitAllPortsMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, this]() {
            if (adapter_->InitAllPorts(adapter_) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcCreateRenderMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<uint32_t> renderIds(AUDIO_THREAD_NUM, 0);
    std::vector<int32_t> results(AUDIO_THREAD_NUM, HDF_FAILURE);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, &renderIds, &results, i, this]() {
            struct IAudioRender *render = nullptr;
            struct AudioDeviceDescriptor devDesc = {};
            struct AudioSampleAttributes attrs = {};
            InitDevDesc(devDesc, PORT_OUT);
            devDesc.desc = const_cast<char *>("primary");
            devDesc.pins = PIN_OUT_SPEAKER;
            InitAttrs(attrs);
            attrs.silenceThreshold = 0;
            attrs.streamId = 0;
            results[i] = adapter_->CreateRender(adapter_, &devDesc, &attrs, &render, &renderIds[i]);
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
            adapter_->DestroyRender(adapter_, renderIds[i]);
        }
    }
    EXPECT_GE(successCnt.load(), 1);
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcGetPortCapabilityMutiThread001, TestSize.Level1)
{
    struct AudioPort port = {};
    InitPort(port, PORT_OUT);
    ASSERT_NE(port.portId, 0);

    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, port, this]() {
            struct AudioPortCapability capability = {};
            if (adapter_->GetPortCapability(adapter_, &port, &capability) == HDF_SUCCESS) {
                successCnt++;
            }
            AudioPortCapabilityFree(&capability, false);
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcGetDeviceStatusMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, this]() {
            struct AudioDeviceStatus status = {};
            if (adapter_->GetDeviceStatus(adapter_, &status) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcSetMicMuteGetMicMuteMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> getCnt(0);
    std::atomic<int32_t> setCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([i, &getCnt, &setCnt, this]() {
            bool mute = false;
            if (adapter_->GetMicMute(adapter_, &mute) == HDF_SUCCESS) {
                getCnt++;
            }
            if ((i % 2 == 0) && (adapter_->SetMicMute(adapter_, true) == HDF_SUCCESS) &&
                (adapter_->SetMicMute(adapter_, false) == HDF_SUCCESS)) {
                setCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // restore mic mute state
    (void)adapter_->SetMicMute(adapter_, false);

    EXPECT_EQ(AUDIO_THREAD_NUM, getCnt.load());
    EXPECT_GE(setCnt.load(), 1);
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcGetPassthroughModeMutiThread001, TestSize.Level1)
{
    struct AudioPort port = {};
    InitPort(port, PORT_OUT);
    ASSERT_NE(port.portId, 0);

    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, port, this]() {
            enum AudioPortPassthroughMode mode = PORT_PASSTHROUGH_LPCM;
            if (adapter_->GetPassthroughMode(adapter_, &port, &mode) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // passthrough mode support depends on the HAL implementation
    EXPECT_GE(successCnt.load(), 1);
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcCreateRenderDestroyChurnMutiThread001, TestSize.Level1)
{
    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitDevDesc(devDesc, PORT_OUT);
    devDesc.desc = const_cast<char *>("primary");
    devDesc.pins = PIN_OUT_SPEAKER;
    InitAttrs(attrs);
    attrs.silenceThreshold = 0;
    attrs.streamId = 0;

    std::atomic<int32_t> createCnt(0);
    std::atomic<int32_t> destroyCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([devDesc, attrs, &createCnt, &destroyCnt, this]() {
            CreateRenderDestroyChurnWorker(&devDesc, &attrs, createCnt, destroyCnt);
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_GE(createCnt.load(), 1);
    EXPECT_EQ(createCnt.load(), destroyCnt.load());
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcGetVersionMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, this]() {
            uint32_t majorVer = 0;
            uint32_t minorVer = 0;
            if (adapter_->GetVersion(adapter_, &majorVer, &minorVer) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcSetPassthroughModeMutiThread001, TestSize.Level1)
{
    struct AudioPort port = {};
    InitPort(port, PORT_OUT);
    ASSERT_NE(port.portId, 0);

    std::atomic<int32_t> setCnt(0);
    std::atomic<int32_t> getCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&setCnt, &getCnt, port, this]() {
            if (adapter_->SetPassthroughMode(adapter_, &port, PORT_PASSTHROUGH_LPCM) == HDF_SUCCESS) {
                setCnt++;
            }
            enum AudioPortPassthroughMode mode = PORT_PASSTHROUGH_LPCM;
            if (adapter_->GetPassthroughMode(adapter_, &port, &mode) == HDF_SUCCESS) {
                getCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // passthrough mode support depends on the HAL implementation
    EXPECT_GE(setCnt.load(), 1);
    EXPECT_GE(getCnt.load(), 1);
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcSetVoiceVolumeMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, this]() {
            if (adapter_->SetVoiceVolume(adapter_, AUDIO_HALF_OF_MAX_VOLUME) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // voice volume support depends on the HAL implementation, try to restore it
    (void)adapter_->SetVoiceVolume(adapter_, 1.0f);

    EXPECT_GE(successCnt.load(), 1);
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcDestroyCaptureMutiThread001, TestSize.Level1)
{
    struct IAudioCapture *capture = nullptr;
    uint32_t captureId = 0;
    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitDevDesc(devDesc, PORT_IN);
    devDesc.desc = const_cast<char *>("primary");
    devDesc.pins = PIN_IN_MIC;
    InitAttrs(attrs);
    attrs.silenceThreshold = 0;
    attrs.streamId = 0;
    ASSERT_EQ(HDF_SUCCESS, adapter_->CreateCapture(adapter_, &devDesc, &attrs, &capture, &captureId));
    ASSERT_NE(capture, nullptr);

    std::atomic<int32_t> destroyCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&destroyCnt, captureId, this]() {
            if (adapter_->DestroyCapture(adapter_, captureId) == HDF_SUCCESS) {
                destroyCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // only one thread can win the race to destroy the created capture
    EXPECT_EQ(1, destroyCnt.load());

    // the adapter service must still work after the concurrent destroy
    capture = nullptr;
    captureId = 0;
    EXPECT_EQ(HDF_SUCCESS, adapter_->CreateCapture(adapter_, &devDesc, &attrs, &capture, &captureId));
    EXPECT_EQ(HDF_SUCCESS, adapter_->DestroyCapture(adapter_, captureId));
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcDestroyRenderMutiThread001, TestSize.Level1)
{
    struct IAudioRender *render = nullptr;
    uint32_t renderId = 0;
    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitDevDesc(devDesc, PORT_OUT);
    devDesc.desc = const_cast<char *>("primary");
    devDesc.pins = PIN_OUT_SPEAKER;
    InitAttrs(attrs);
    attrs.silenceThreshold = 0;
    attrs.streamId = 0;
    ASSERT_EQ(HDF_SUCCESS, adapter_->CreateRender(adapter_, &devDesc, &attrs, &render, &renderId));
    ASSERT_NE(render, nullptr);

    std::atomic<int32_t> destroyCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&destroyCnt, renderId, this]() {
            if (adapter_->DestroyRender(adapter_, renderId) == HDF_SUCCESS) {
                destroyCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    // only one thread can win the race to destroy the created render
    EXPECT_EQ(1, destroyCnt.load());

    // the adapter service must still work after the concurrent destroy
    render = nullptr;
    renderId = 0;
    EXPECT_EQ(HDF_SUCCESS, adapter_->CreateRender(adapter_, &devDesc, &attrs, &render, &renderId));
    EXPECT_EQ(HDF_SUCCESS, adapter_->DestroyRender(adapter_, renderId));
}

HWTEST_F(HdfAudioConcAdapterTest, HdfAudioAdapterConcGetPortCapabilityAllPortsMutiThread001, TestSize.Level1)
{
    ASSERT_NE(adapterDescs_, nullptr);
    uint32_t portsLen = adapterDescs_->portsLen;
    ASSERT_GT(portsLen, 0);

    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([i, portsLen, &successCnt, this]() {
            struct AudioPort port = adapterDescs_->ports[i % portsLen];
            struct AudioPortCapability capability = {};
            if (adapter_->GetPortCapability(adapter_, &port, &capability) == HDF_SUCCESS) {
                successCnt++;
            }
            AudioPortCapabilityFree(&capability, false);
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

} // namespace
