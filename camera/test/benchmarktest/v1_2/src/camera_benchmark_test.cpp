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

#include "camera_benchmark_test.h"

using namespace OHOS;
using namespace std;
using namespace testing::ext;
using namespace OHOS::Camera;

constexpr int32_t ITERATION_FREQUENCY = 100;
constexpr int32_t REPETITION_FREQUENCY = 3;

void CameraBenchmarkTest::SetUp(const ::benchmark::State &state)
{
    cameraTest = std::make_shared<OHOS::Camera::Test>();
    cameraTest->Init();
    cameraTest->OpenCameraV1_2();
}

void CameraBenchmarkTest::TearDown(const ::benchmark::State &state)
{
    cameraTest->Close();
}

/**
  * @tc.name: NotifyDeviceStateChangeInfo
  * @tc.desc: benchmark
  * @tc.level: Level0
  * @tc.size: MediumTest
  * @tc.type: Function
  */
BENCHMARK_F(CameraBenchmarkTest, NotifyDeviceStateChangeInfo_benchmark_001)(
    benchmark::State &st)
{
    EXPECT_EQ(false, cameraTest->serviceV1_2 == nullptr);
    int notifyType = 1;
    int deviceState = 1008;
    for (auto _ : st) {
        cameraTest->rc = cameraTest->serviceV1_2->NotifyDeviceStateChangeInfo(notifyType, deviceState);
    }
}
BENCHMARK_REGISTER_F(CameraBenchmarkTest, NotifyDeviceStateChangeInfo_benchmark_001)->Iterations(ITERATION_FREQUENCY)->
    Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: PreCameraSwitch
  * @tc.desc: benchmark
  * @tc.level: Level0
  * @tc.size: MediumTest
  * @tc.type: Function
  */
BENCHMARK_F(CameraBenchmarkTest, PreCameraSwitch_benchmark_001)(
    benchmark::State &st)
{
    EXPECT_EQ(false, cameraTest->serviceV1_2 == nullptr);
    cameraTest->serviceV1_2->GetCameraIds(cameraTest->cameraIds);
    for (auto _ : st) {
        cameraTest->rc = cameraTest->serviceV1_2->PreCameraSwitch(cameraTest->cameraIds.front());
    }
}
BENCHMARK_REGISTER_F(CameraBenchmarkTest, PreCameraSwitch_benchmark_001)->Iterations(ITERATION_FREQUENCY)->
    Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: PrelaunchWithOpMode
  * @tc.desc: Prelaunch, benchmark.
  * @tc.level: Level0
  * @tc.size: MediumTest
  * @tc.type: Function
  */
BENCHMARK_F(CameraBenchmarkTest, PrelaunchWithOpMode_benchmark_001)(
    benchmark::State &st)
{
    EXPECT_EQ(false, cameraTest->serviceV1_2 == nullptr);
    cameraTest->serviceV1_2->GetCameraIds(cameraTest->cameraIds);

    cameraTest->prelaunchConfig = std::make_shared<OHOS::HDI::Camera::V1_1::PrelaunchConfig>();
    cameraTest->prelaunchConfig->cameraId = cameraTest->cameraIds.front();
    cameraTest->prelaunchConfig->streamInfos_V1_1 = {};
    cameraTest->prelaunchConfig->setting = {};
    for (auto _ : st) {
        cameraTest->rc = cameraTest->serviceV1_2->PrelaunchWithOpMode(
            *cameraTest->prelaunchConfig, OHOS::HDI::Camera::V1_2::NORMAL);
    }
}
BENCHMARK_REGISTER_F(CameraBenchmarkTest, PrelaunchWithOpMode_benchmark_001)->Iterations(ITERATION_FREQUENCY)->
    Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: UpdateStreams
  * @tc.desc: benchmark
  * @tc.level: Level0
  * @tc.size: MediumTest
  * @tc.type: Function
  */
BENCHMARK_F(CameraBenchmarkTest, UpdateStreams_benchmark_001)(
    benchmark::State &st)
{
    cameraTest->streamOperatorCallbackV1_2 = new OHOS::Camera::Test::TestStreamOperatorCallbackV1_2();
    cameraTest->rc = cameraTest->cameraDeviceV1_2->GetStreamOperator_V1_2(cameraTest->streamOperatorCallbackV1_2,
        cameraTest->streamOperator_V1_2);
    EXPECT_NE(cameraTest->streamOperator_V1_2, nullptr);
    cameraTest->streamInfoV1_1 = std::make_shared<OHOS::HDI::Camera::V1_1::StreamInfo_V1_1>();
    cameraTest->streamInfoV1_1->v1_0.streamId_ = 100;
    cameraTest->streamInfoV1_1->v1_0.width_ = 1920;
    cameraTest->streamInfoV1_1->v1_0.height_ = 1080;
    cameraTest->streamInfoV1_1->v1_0.intent_ = StreamIntent::PREVIEW;
    cameraTest->streamInfoV1_1->v1_0.tunneledMode_ = UT_TUNNEL_MODE;
    cameraTest->streamInfoV1_1->v1_0.dataspace_ = OHOS_CAMERA_SRGB_FULL;
    cameraTest->streamInfoV1_1->v1_0.format_ = PIXEL_FMT_YCRCB_420_SP;
    cameraTest->streamInfosV1_1.push_back(*cameraTest->streamInfoV1_1);
    // capture streamInfo
    cameraTest->streamInfoCapture = std::make_shared<OHOS::HDI::Camera::V1_1::StreamInfo_V1_1>();
    cameraTest->streamInfoCapture->v1_0.streamId_ = 101;
    cameraTest->streamInfoCapture->v1_0.width_ = 1920;
    cameraTest->streamInfoCapture->v1_0.height_ = 1080;
    cameraTest->streamInfoCapture->v1_0.intent_ = StreamIntent::STILL_CAPTURE;
    cameraTest->streamInfoCapture->v1_0.encodeType_ = ENCODE_TYPE_H265;
    cameraTest->streamInfoCapture->v1_0.tunneledMode_ = UT_TUNNEL_MODE;
    cameraTest->streamInfoCapture->v1_0.dataspace_ = OHOS_CAMERA_SRGB_FULL;
    cameraTest->streamInfoCapture->v1_0.format_ = PIXEL_FMT_YCRCB_420_SP;
    cameraTest->streamInfosV1_1.push_back(*cameraTest->streamInfoCapture);
    for (auto _ : st) {
        cameraTest->rc = cameraTest->streamOperator_V1_2->UpdateStreams(cameraTest->streamInfosV1_1);
    }
}
BENCHMARK_REGISTER_F(CameraBenchmarkTest, UpdateStreams_benchmark_001)->Iterations(ITERATION_FREQUENCY)->
    Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

BENCHMARK_MAIN();

