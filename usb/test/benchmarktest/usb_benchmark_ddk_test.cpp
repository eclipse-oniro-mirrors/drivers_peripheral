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

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include "hdf_log.h"
#include "v1_2/iusb_ddk.h"
#include "usbd_wrapper.h"

using namespace benchmark::internal;
using namespace OHOS;
using namespace OHOS::HDI::Usb::Ddk;
using namespace std;

namespace {
OHOS::sptr<V1_2::IUsbDdk> g_usbDdk = nullptr;
V1_2::DriverAbilityInfo g_driverInfo;

constexpr int32_t ITERATION_FREQUENCY = 100;
constexpr int32_t REPETITION_FREQUENCY = 3;
constexpr int32_t INVALID_DEVICE_ID = 0;
constexpr uint8_t CONFIG_INDEX_0 = 0;
constexpr uint8_t INTERFACE_INDEX_0 = 0;
constexpr uint8_t SETTING_INDEX_0 = 0;

class UsbBenchmarkDdkTest : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};

void UsbBenchmarkDdkTest::SetUp(const ::benchmark::State &state)
{
    g_usbDdk = V1_2::IUsbDdk::Get();
    ASSERT_NE(g_usbDdk, nullptr);
}

void UsbBenchmarkDdkTest::TearDown(const ::benchmark::State &state) {}

/**
 * @tc.name: DdkGetDevices
 * @tc.desc: Test functions to GetDevices benchmark test
 * @tc.desc: int32_t GetDevices(std::vector<uint64_t> &deviceIds);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetDevices)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    std::vector<uint64_t> deviceIds;
    int32_t ret = 0;
    for (auto _ : state) {
        deviceIds.clear();
        ret = g_usbDdk->GetDevices(deviceIds);
    }
    // Without a device, GetDevices may return empty list; ensure no crash.
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetDevices)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkGetNonRootHubs
 * @tc.desc: Test functions to GetNonRootHubs benchmark test
 * @tc.desc: int32_t GetNonRootHubs(std::vector<uint64_t> &nonRootHubIds);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetNonRootHubs)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    std::vector<uint64_t> nonRootHubIds;
    int32_t ret = 0;
    for (auto _ : state) {
        nonRootHubIds.clear();
        ret = g_usbDdk->GetNonRootHubs(nonRootHubIds);
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetNonRootHubs)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkRemoveNonExistentDriverInfo
 * @tc.desc: Benchmark test that removes a non-existent driver info; expected to fail gracefully.
 * @tc.desc: int32_t RemoveDriverInfo(const std::string &driverUid);
 * @tc.desc: Negative test: non-existent driverUid, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkRemoveNonExistentDriverInfo)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->RemoveDriverInfo("non-existent-driver-uid-bench");
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkRemoveNonExistentDriverInfo)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkGetDeviceDescriptorInvalid
 * @tc.desc: Benchmark test that gets device descriptor with invalid device id; expected to fail gracefully.
 * @tc.desc: int32_t GetDeviceDescriptor(uint64_t deviceId, UsbDeviceDescriptor &desc);
 * @tc.desc: Negative test: invalid deviceId, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetDeviceDescriptorInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    V1_0::UsbDeviceDescriptor desc;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->GetDeviceDescriptor(INVALID_DEVICE_ID, desc);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetDeviceDescriptorInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkGetConfigDescriptorInvalid
 * @tc.desc: Benchmark test that gets config descriptor with invalid device id; expected to fail gracefully.
 * @tc.desc: int32_t GetConfigDescriptor(uint64_t deviceId, uint8_t configIndex, std::vector<uint8_t> &configDesc);
 * @tc.desc: Negative test: invalid deviceId, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetConfigDescriptorInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    std::vector<uint8_t> configDesc;
    int32_t ret = 0;
    for (auto _ : state) {
        configDesc.clear();
        ret = g_usbDdk->GetConfigDescriptor(INVALID_DEVICE_ID, CONFIG_INDEX_0, configDesc);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetConfigDescriptorInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkClaimInterfaceInvalid
 * @tc.desc: Benchmark test that claims interface with invalid device id; expected to fail gracefully.
 * @tc.desc: int32_t ClaimInterface(uint64_t deviceId, uint8_t interfaceIndex, uint64_t &interfaceHandle);
 * @tc.desc: Negative test: invalid deviceId, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkClaimInterfaceInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    uint64_t interfaceHandle = 0;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->ClaimInterface(INVALID_DEVICE_ID, INTERFACE_INDEX_0, interfaceHandle);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkClaimInterfaceInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkReleaseInterfaceInvalidHandle
 * @tc.desc: Benchmark test that releases interface with invalid handle; expected to fail gracefully.
 * @tc.desc: int32_t ReleaseInterface(uint64_t interfaceHandle);
 * @tc.desc: Negative test: invalid handle, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkReleaseInterfaceInvalidHandle)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->ReleaseInterface(0);
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkReleaseInterfaceInvalidHandle)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkSelectInterfaceSettingInvalidHandle
 * @tc.desc: Benchmark test that selects interface setting with invalid handle; expected to fail gracefully.
 * @tc.desc: int32_t SelectInterfaceSetting(uint64_t interfaceHandle, uint8_t settingIndex);
 * @tc.desc: Negative test: invalid handle, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkSelectInterfaceSettingInvalidHandle)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->SelectInterfaceSetting(0, SETTING_INDEX_0);
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkSelectInterfaceSettingInvalidHandle)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkGetCurrentInterfaceSettingInvalidHandle
 * @tc.desc: Benchmark test that gets current interface setting with invalid handle; expected to fail gracefully.
 * @tc.desc: int32_t GetCurrentInterfaceSetting(uint64_t interfaceHandle, uint8_t &settingIndex);
 * @tc.desc: Negative test: invalid handle, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetCurrentInterfaceSettingInvalidHandle)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    uint8_t settingIndex = 0;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->GetCurrentInterfaceSetting(0, settingIndex);
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetCurrentInterfaceSettingInvalidHandle)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkGetDeviceMemMapFdInvalid
 * @tc.desc: Benchmark test that gets device memory map fd with invalid device id; expected to fail gracefully.
 * @tc.desc: int32_t GetDeviceMemMapFd(uint64_t deviceId, int32_t &fd);
 * @tc.desc: Negative test: invalid deviceId, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkGetDeviceMemMapFdInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    int32_t fd = -1;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->GetDeviceMemMapFd(INVALID_DEVICE_ID, fd);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkGetDeviceMemMapFdInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkUpdateDriverInfoEmptyVids
 * @tc.desc: Benchmark test that updates driver info with empty vids list.
 * @tc.desc: int32_t UpdateDriverInfo(const DriverAbilityInfo &driverInfo);
 * @tc.desc: Boundary test: empty vids, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkUpdateDriverInfoEmptyVids)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    g_driverInfo.driverUid = "bench-driver-empty-vids";
    g_driverInfo.vids = {};
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->UpdateDriverInfo(g_driverInfo);
    }
    (void)ret;
    (void)g_usbDdk->RemoveDriverInfo(g_driverInfo.driverUid);
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkUpdateDriverInfoEmptyVids)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: DdkRemoveEmptyDriverUid
 * @tc.desc: Benchmark test that removes driver info with empty uid string.
 * @tc.desc: int32_t RemoveDriverInfo(const std::string &driverUid);
 * @tc.desc: Boundary test: empty driverUid, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkDdkTest, DdkRemoveEmptyDriverUid)(benchmark::State &state)
{
    ASSERT_NE(g_usbDdk, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbDdk->RemoveDriverInfo("");
    }
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkDdkTest, DdkRemoveEmptyDriverUid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

} // namespace

BENCHMARK_MAIN();
