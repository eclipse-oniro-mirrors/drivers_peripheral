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

#include <benchmark/benchmark.h>
#include <climits>
#include <gtest/gtest.h>
#include <climits>
#include "osal_mem.h"
#include "v5_0/iaudio_capture.h"
#include "v5_0/iaudio_manager.h"

using namespace std;
using namespace testing::ext;
namespace {
static const uint32_t MAX_AUDIO_ADAPTER_NUM = 5;
const int32_t BUFFER_LENTH = 1024 * 16;
const int32_t DEEP_BUFFER_CAPTURE_PERIOD_SIZE = 4 * 1024;
const int32_t MMAP_SUGGUEST_REQ_SIZE = 1920;
const int32_t MOVE_LEFT_NUM = 8;
const int32_t TEST_SAMPLE_RATE_MASK_48000 = 48000;
const int32_t TEST_CHANNEL_COUNT_STERO = 2;
const int32_t ITERATION_FREQUENCY = 100;
const int32_t REPETITION_FREQUENCY = 3;

class AudioCaptureMmapBenchmarkTest : public benchmark::Fixture {
public:
    struct IAudioManager *manager_ = nullptr;;
    struct IAudioAdapter *adapter_ = nullptr;
    struct IAudioCapture *mmapCapture_ = nullptr;
    uint32_t captureId_ = 0;
    char *devDescriptorName_ = nullptr;
    struct AudioAdapterDescriptor adapterDescs_[MAX_AUDIO_ADAPTER_NUM];
    uint32_t adapterSize_ = 0;
    virtual void SetUp(const ::benchmark::State &state);
    virtual void TearDown(const ::benchmark::State &state);
    void InitCaptureDevDesc(struct AudioDeviceDescriptor &devDesc);
    void InitCaptureAttrs(struct AudioSampleAttributes &attrs);
    void FreeAdapterElements(struct AudioAdapterDescriptor *dataBlock, bool freeSelf);
    void ReleaseAllAdapterDescs(struct AudioAdapterDescriptor *descs, uint32_t descsLen);
};

void AudioCaptureMmapBenchmarkTest::InitCaptureDevDesc(struct AudioDeviceDescriptor &devDesc)
{
    ASSERT_NE(adapterDescs_, nullptr);
    ASSERT_NE(adapterDescs_->ports, nullptr);

    devDesc.pins = (enum AudioPortPin)PIN_IN_MIC;
    devDescriptorName_ = strdup("cardname");
    devDesc.desc = devDescriptorName_;

    for (uint32_t index = 0; index < adapterDescs_->portsLen; index++) {
        if (adapterDescs_->ports[index].dir == PORT_IN) {
            devDesc.portId = adapterDescs_->ports[index].portId;
            return;
        }
    }
}

void AudioCaptureMmapBenchmarkTest::InitCaptureAttrs(struct AudioSampleAttributes &attrs)
{
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = TEST_CHANNEL_COUNT_STERO;
    attrs.sampleRate = TEST_SAMPLE_RATE_MASK_48000;
    attrs.interleaved = 1;
    attrs.type = AUDIO_MMAP_NOIRQ;
    attrs.period = DEEP_BUFFER_CAPTURE_PERIOD_SIZE;
    attrs.frameSize = AUDIO_FORMAT_TYPE_PCM_16_BIT * TEST_CHANNEL_COUNT_STERO / MOVE_LEFT_NUM;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_CAPTURE_PERIOD_SIZE / (attrs.format * attrs.channelCount / MOVE_LEFT_NUM);
    attrs.stopThreshold = INT_MAX;
    attrs.silenceThreshold = BUFFER_LENTH;
}

void AudioCaptureMmapBenchmarkTest::FreeAdapterElements(struct AudioAdapterDescriptor *dataBlock, bool freeSelf)
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
}

void AudioCaptureMmapBenchmarkTest::ReleaseAllAdapterDescs(struct AudioAdapterDescriptor *descs, uint32_t descsLen)
{
    if (descs == nullptr || descsLen == 0) {
        return;
    }

    for (uint32_t i = 0; i < descsLen; i++) {
        FreeAdapterElements(&descs[i], false);
    }
}

void AudioCaptureMmapBenchmarkTest::SetUp(const ::benchmark::State &state)
{
    adapterSize_ = MAX_AUDIO_ADAPTER_NUM;
    manager_ = IAudioManagerGet(false);
    ASSERT_NE(manager_, nullptr);

    EXPECT_EQ(HDF_SUCCESS, manager_->GetAllAdapters(manager_, adapterDescs_, &adapterSize_));
    if (adapterSize_ > MAX_AUDIO_ADAPTER_NUM) {
        ReleaseAllAdapterDescs(adapterDescs_, adapterSize_);
        ASSERT_LT(adapterSize_, MAX_AUDIO_ADAPTER_NUM);
    }

    EXPECT_EQ(HDF_SUCCESS, manager_->LoadAdapter(manager_, &adapterDescs_[0], &adapter_));
    if (adapter_ == nullptr) {
        ReleaseAllAdapterDescs(adapterDescs_, adapterSize_);
        EXPECT_NE(adapter_, nullptr);
    }

    struct AudioDeviceDescriptor devDesc = {};
    struct AudioSampleAttributes attrs = {};
    InitCaptureDevDesc(devDesc);
    InitCaptureAttrs(attrs);
    EXPECT_EQ(HDF_SUCCESS, adapter_->CreateCapture(adapter_, &devDesc, &attrs, &mmapCapture_, &captureId_));
    if (mmapCapture_ == nullptr) {
        (void)manager_->UnloadAdapter(manager_, adapterDescs_[0].adapterName);
        ReleaseAllAdapterDescs(adapterDescs_, adapterSize_);
    }
    ASSERT_NE(mmapCapture_, nullptr);
}

void AudioCaptureMmapBenchmarkTest::TearDown(const ::benchmark::State &state)
{
    ASSERT_NE(devDescriptorName_, nullptr);
    free(devDescriptorName_);

    ASSERT_NE(mmapCapture_, nullptr);
    EXPECT_EQ(HDF_SUCCESS, adapter_->DestroyCapture(adapter_, captureId_));

    ASSERT_NE(manager_, nullptr);
    EXPECT_EQ(HDF_SUCCESS, manager_->UnloadAdapter(manager_, adapterDescs_[0].adapterName));
    ReleaseAllAdapterDescs(adapterDescs_, adapterSize_);

    IAudioManagerRelease(manager_, false);
}

BENCHMARK_F(AudioCaptureMmapBenchmarkTest, ReqMmapBuffer)(benchmark::State &state)
{
    ASSERT_NE(mmapCapture_, nullptr);
    int32_t ret;
    struct AudioMmapBufferDescriptor desc = {0};
    uint64_t frames = 0;
    struct AudioTimeStamp time = {0};

    for (auto _ : state) {
        ret = mmapCapture_->ReqMmapBuffer(mmapCapture_, MMAP_SUGGUEST_REQ_SIZE, &desc);
        ASSERT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT || ret == HDF_ERR_INVALID_PARAM);

        ret = mmapCapture_->Start(mmapCapture_);
        EXPECT_EQ(ret, HDF_SUCCESS);

        ret = mmapCapture_->GetMmapPosition(mmapCapture_, &frames, &time);
        ASSERT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT || ret == HDF_ERR_INVALID_PARAM);

        ret = mmapCapture_->Stop(mmapCapture_);
        ASSERT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    }
}

BENCHMARK_REGISTER_F(AudioCaptureMmapBenchmarkTest, ReqMmapBuffer)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();
}
