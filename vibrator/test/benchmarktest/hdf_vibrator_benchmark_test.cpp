/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <securec.h>
#include <string>
#include <vector>
#include "hdf_base.h"
#include "osal_time.h"
#include "v2_0/ivibrator_interface.h"
#include "vibrator_plug_callback_impl.h"

using namespace OHOS::HDI::Vibrator;
using namespace OHOS::HDI::Vibrator::V2_0;
using namespace testing::ext;
using namespace std;

namespace {
    constexpr int32_t ITERATION_FREQUENCY = 100;
    constexpr int32_t REPETITION_FREQUENCY = 3;
    uint32_t g_duration = 100;
    int32_t g_intensity1 = 30;
    int32_t g_frequency1 = 200;
    V2_0::HapticPaket g_hapticPaket = {434, 1, {{V2_0::TRANSIENT, 0, 149, 100, 50, 0, 4,
        {{0, 0, 0}, {1, 1, 0}, {32, 1, -39}, {149, 0, -39}}}}};
    V2_0::VibratorPackage g_vibPackage = {434, 149, {{434, 1, {{V2_0::TRANSIENT, 0, 149, 100, 50, 0, 4,
        {{0, 0, 0}, {1, 1, 0}, {32, 1, -39}, {149, 0, -39}}}}}}};
    int32_t g_sessionId = 1;
    DeviceVibratorInfo g_deviceVibratorInfo = {-1, 1};
    uint16_t g_intensity = 50;
    HapticPaket g_pkg = {434, 1, {{V2_0::CONTINUOUS, 0, 149, 100, 50, 0, 4,
        {{0, 0, 0}, {1, 1, 0}, {32, 1, -39}, {149, 0, -39}}}}};
    std::vector<HdfWaveInformation> g_info;
    std::string g_timeSequence = "haptic.clock.timer";
    std::string g_builtIn = "haptic.default.effect";
    sptr<IVibratorInterface> g_vibratorInterface = nullptr;
    sptr<IVibratorPlugCallback> g_vibratorPlugCallback = new VibratorPlugCallbackImpl();
    std::vector<HdfVibratorInfo> g_vibratorInfo;

class VibratorBenchmarkTest : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};

void VibratorBenchmarkTest::SetUp(const ::benchmark::State &state)
{
    g_vibratorInterface = IVibratorInterface::Get();
    if (g_vibratorInterface == nullptr) {
        printf("g_vibratorInterface is nullptr");
        GTEST_SKIP() << "g_vibratorInterface is nullptr" << std::endl;
        return;
    }
    g_vibratorInterface->GetVibratorInfo(g_vibratorInfo);
}

void VibratorBenchmarkTest::TearDown(const ::benchmark::State &state)
{
}

/**
  * @tc.name: DriverSystem_VibratorBenchmark_StartOnce
  * @tc.desc: Benchmarktest for interface StartOnce
  * Controls this vibrator to perform a one-shot vibrator at a given duration
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, StartOnce)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet;
    int32_t endRet;
    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            startRet = g_vibratorInterface->StartOnce({it.deviceId, it.vibratorId}, g_duration);
            EXPECT_EQ(startRet, HDF_SUCCESS);

            endRet = g_vibratorInterface->Stop({it.deviceId, it.vibratorId}, HDF_VIBRATOR_MODE_ONCE);
            EXPECT_EQ(endRet, HDF_SUCCESS);
        }
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, StartOnce)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_Start
  * @tc.desc: Benchmarktest for interface Start
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, Start)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet;
    int32_t endRet;
    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            HdfEffectInfo effectInfo;
            g_vibratorInterface->GetEffectInfo({it.deviceId, it.vibratorId}, g_timeSequence, effectInfo);
            if (effectInfo.isSupportEffect == true) {
                startRet = g_vibratorInterface->Start({it.deviceId, it.vibratorId}, g_timeSequence);
                EXPECT_EQ(startRet, HDF_SUCCESS);

                endRet = g_vibratorInterface->Stop({it.deviceId, it.vibratorId}, HDF_VIBRATOR_MODE_PRESET);
                EXPECT_EQ(endRet, HDF_SUCCESS);
            }
        }
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, Start)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_Stop
  * @tc.desc: Benchmarktest for interface Stop
  * Controls this Performing built-in Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, Stop)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);
    int32_t startRet;
    int32_t endRet;

    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            HdfEffectInfo effectInfo;
            g_vibratorInterface->GetEffectInfo({it.deviceId, it.vibratorId}, g_builtIn, effectInfo);
            if (effectInfo.isSupportEffect == true) {
                startRet = g_vibratorInterface->Start({it.deviceId, it.vibratorId}, g_builtIn);
                EXPECT_EQ(startRet, HDF_SUCCESS);

                endRet = g_vibratorInterface->Stop({it.deviceId, it.vibratorId}, HDF_VIBRATOR_MODE_PRESET);
                EXPECT_EQ(endRet, HDF_SUCCESS);
            }
        }
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, Stop)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_GetVibratorInfo
  * @tc.desc: Benchmarktest for interface GetVibratorInfo
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetVibratorInfo)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;

    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->GetVibratorInfo(info);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetVibratorInfo)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_EnableVibratorModulation
  * @tc.desc: Benchmarktest for interface EnableVibratorModulation
  * Controls this Performing built-in Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, EnableVibratorModulation)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;

    int32_t startRet = g_vibratorInterface->GetVibratorInfo(info);
    EXPECT_EQ(startRet, HDF_SUCCESS);

    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            if ((info[0].isSupportIntensity == 1) || (info[0].isSupportFrequency == 1)) {
                printf("vibratot benchmarktest successed!\n\t");
                EXPECT_GT(g_duration, 0);
                EXPECT_GE(g_intensity1, info[0].intensityMinValue);
                EXPECT_LE(g_intensity1, info[0].intensityMaxValue);
                EXPECT_GE(g_frequency1, info[0].frequencyMinValue);
                EXPECT_LE(g_frequency1, info[0].frequencyMaxValue);
                printf("vibratot benchmark test successed!\n\t");
                startRet = g_vibratorInterface->EnableVibratorModulation({it.deviceId, it.vibratorId}, g_duration,
                                                                         g_intensity1, g_frequency1);
                EXPECT_EQ(startRet, HDF_SUCCESS);
                startRet = g_vibratorInterface->Stop({it.deviceId, it.vibratorId}, HDF_VIBRATOR_MODE_ONCE);
                EXPECT_EQ(startRet, HDF_SUCCESS);
            }
        }
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, EnableVibratorModulation)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: SUB_DriverSystem_VibratorBenchmark_EnableCompositeEffect
  * @tc.desc: Start periodic vibration with custom composite effect
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, EnableCompositeEffect)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    for (auto _ : state) {
        PrimitiveEffect primitiveEffect1 {0, 60007, 0};
        PrimitiveEffect primitiveEffect2 {1000, 60007, 0};
        PrimitiveEffect primitiveEffect3 {1000, 60007, 0};
        CompositeEffect effect1 = {
            .primitiveEffect = primitiveEffect1
        };
        CompositeEffect effect2 = {
            .primitiveEffect = primitiveEffect2
        };
        CompositeEffect effect3 = {
            .primitiveEffect = primitiveEffect3
        };
        std::vector<CompositeEffect> vec;
        vec.push_back(effect1);
        vec.push_back(effect2);
        vec.push_back(effect3);
        HdfCompositeEffect effect;
        effect.type = HDF_EFFECT_TYPE_PRIMITIVE;
        effect.compositeEffects = vec;
        int32_t ret;
        for (auto it : g_vibratorInfo) {
            HapticCapacity hapticCapacity;
            ret = g_vibratorInterface->GetHapticCapacity({it.deviceId, it.vibratorId}, hapticCapacity);
            EXPECT_EQ(ret, HDF_SUCCESS);
            if (hapticCapacity.isSupportPresetMapping) {
                ret = g_vibratorInterface->EnableCompositeEffect({it.deviceId, it.vibratorId}, effect);
                EXPECT_EQ(HDF_SUCCESS, ret);
            }
        }
        EXPECT_EQ(HDF_SUCCESS, ret);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, EnableCompositeEffect)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: SUB_DriverSystem_VibratorBenchmark_GetEffectInfo
  * @tc.desc: Get effect information with the given effect type
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetEffectInfo)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);
    HdfEffectInfo effectInfo;
    int32_t ret;
    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            ret = g_vibratorInterface->GetEffectInfo({it.deviceId, it.vibratorId}, "haptic.pattern.type1", effectInfo);
        }
    }
    EXPECT_EQ(HDF_SUCCESS, ret);
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetEffectInfo)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: SUB_DriverSystem_VibratorBenchmark_IsVibratorRunning
  * @tc.desc: Get vibration status.
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, IsVibratorRunning)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);
    bool stat {false};
    int32_t ret;
    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            ret = g_vibratorInterface->IsVibratorRunning({it.deviceId, it.vibratorId}, stat);
        }
    }
    EXPECT_EQ(HDF_SUCCESS, ret);
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, IsVibratorRunning)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_PlayPatternBySessionId
  * @tc.desc: Benchmarktest for interface PlayPatternBySessionId
  * Control vibrator perform
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, PlayPatternBySessionId)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet = 0;
    for (auto _ : state) {
        startRet = g_vibratorInterface->PlayPatternBySessionId(
            {-1, 1}, g_sessionId, g_hapticPaket);
    }
    EXPECT_EQ(startRet, HDF_SUCCESS);
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, PlayPatternBySessionId)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_PlayPackageBySession
  * @tc.desc: Benchmarktest for interface PlayPackageBySession
  * Control vibrator perform
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, PlayPackageBySession)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet = 0;
    for (auto _ : state) {
        startRet = g_vibratorInterface->PlayPackageBySession(
            {-1, 1}, g_sessionId, g_vibPackage);
    }
    EXPECT_EQ(startRet, HDF_SUCCESS);
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, PlayPackageBySession)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_StopVibrateBySessionId
  * @tc.desc: Benchmarktest for interface StopVibrateBySessionId
  * Control vibrator perform and stop by sessionID
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, StopVibrateBySessionId)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet = 0;
    int32_t endRet = 0;
    for (auto _ : state) {
        startRet = g_vibratorInterface->PlayPatternBySessionId(
            {-1, 1}, g_sessionId, g_hapticPaket);
        EXPECT_EQ(startRet, HDF_SUCCESS);

        endRet = g_vibratorInterface->StopVibrateBySessionId({-1, 1}, g_sessionId);
        EXPECT_EQ(endRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, StopVibrateBySessionId)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_GetDeviceVibratorInfo
  * @tc.desc: Benchmarktest for interface GetDeviceVibratorInfo
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetDeviceVibratorInfo)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->GetDeviceVibratorInfo(g_deviceVibratorInfo, info);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetDeviceVibratorInfo)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_GetVibratorIdSingle
  * @tc.desc: Benchmarktest for interface GetVibratorIdSingle
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetVibratorIdSingle)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->GetVibratorIdSingle(g_deviceVibratorInfo, info);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetVibratorIdSingle)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_PlayHapticPattern
  * @tc.desc: Benchmarktest for interface PlayHapticPattern
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, PlayHapticPattern)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->PlayHapticPattern(g_deviceVibratorInfo, g_pkg);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, PlayHapticPattern)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_GetHapticStartUpTime
  * @tc.desc: Benchmarktest for interface GetHapticStartUpTime
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetHapticStartUpTime)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {

        int32_t startUpTime = 0;
        int32_t mode = 0;
        int32_t startRet = g_vibratorInterface->GetHapticStartUpTime(g_deviceVibratorInfo, mode, startUpTime);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetHapticStartUpTime)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_StartByIntensity
  * @tc.desc: Benchmarktest for interface StartByIntensity
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, StartByIntensity)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    int32_t startRet;
    int32_t endRet;
    for (auto _ : state) {
        for (auto it : g_vibratorInfo) {
            HdfEffectInfo effectInfo;
            g_vibratorInterface->GetEffectInfo({it.deviceId, it.vibratorId}, g_timeSequence, effectInfo);
            if (effectInfo.isSupportEffect == true) {
                startRet = g_vibratorInterface->StartByIntensity({it.deviceId, it.vibratorId}, g_timeSequence,
                    g_intensity);
                EXPECT_EQ(startRet, HDF_SUCCESS);

                endRet = g_vibratorInterface->Stop({it.deviceId, it.vibratorId}, HDF_VIBRATOR_MODE_PRESET);
                EXPECT_EQ(endRet, HDF_SUCCESS);
            }
        }
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, StartByIntensity)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_GetAllWaveInfo
  * @tc.desc: Benchmarktest for interface GetAllWaveInfo
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, GetAllWaveInfo)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->GetAllWaveInfo(g_deviceVibratorInfo, g_info);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, GetAllWaveInfo)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_RegVibratorPlugCallback
  * @tc.desc: Benchmarktest for interface RegVibratorPlugCallback
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, RegVibratorPlugCallback)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->RegVibratorPlugCallback(g_vibratorPlugCallback);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, RegVibratorPlugCallback)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
  * @tc.name: DriverSystem_VibratorBenchmark_UnRegVibratorPlugCallback
  * @tc.desc: Benchmarktest for interface UnRegVibratorPlugCallback
  * Controls this Performing Time Series Vibrator Effects
  * @tc.type: FUNC
  */
BENCHMARK_F(VibratorBenchmarkTest, UnRegVibratorPlugCallback)(benchmark::State &state)
{
    ASSERT_NE(nullptr, g_vibratorInterface);

    std::vector<HdfVibratorInfo> info;
    for (auto _ : state) {
        int32_t startRet = g_vibratorInterface->UnRegVibratorPlugCallback(g_vibratorPlugCallback);
        EXPECT_EQ(startRet, HDF_SUCCESS);
    }
}

BENCHMARK_REGISTER_F(VibratorBenchmarkTest, UnRegVibratorPlugCallback)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();
}

BENCHMARK_MAIN();