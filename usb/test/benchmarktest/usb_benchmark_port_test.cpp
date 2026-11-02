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
#include "usbd_port.h"
#include "v2_0/iusb_port_interface.h"
#include "v2_0/iusbd_subscriber.h"
#include "v2_0/usb_types.h"

using namespace benchmark::internal;
using namespace OHOS;
using namespace std;
using namespace OHOS::HDI::Usb::V2_0;

namespace {
sptr<IUsbPortInterface> g_usbPortInterface = nullptr;
bool g_portRoleSupported = false;

constexpr int32_t SLEEP_TIME = 3;
constexpr int32_t ITERATION_FREQUENCY = 100;
constexpr int32_t REPETITION_FREQUENCY = 3;
constexpr int32_t INVALID_PORT_ID = -1;

class UsbBenchmarkPortTest : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};

class UsbdPortSubscriberTest : public OHOS::HDI::Usb::V2_0::IUsbdSubscriber {
public:
    UsbdPortSubscriberTest() = default;
    ~UsbdPortSubscriberTest() = default;
    int32_t DeviceEvent(const OHOS::HDI::Usb::V2_0::USBDeviceInfo &info) override
    {
        return 0;
    }
    int32_t PortChangedEvent(const OHOS::HDI::Usb::V2_0::PortInfo &info) override
    {
        lastPortInfo_ = info;
        return 0;
    }
    OHOS::HDI::Usb::V2_0::PortInfo lastPortInfo_ = {0, 0, 0, 0, 0};
};

int32_t SwitchErrCode(int32_t ret)
{
    return ret == HDF_ERR_NOT_SUPPORT ? HDF_SUCCESS : ret;
}

void UsbBenchmarkPortTest::SetUp(const ::benchmark::State &state)
{
    g_usbPortInterface = IUsbPortInterface::Get();
    ASSERT_NE(g_usbPortInterface, nullptr);
    auto ret = g_usbPortInterface->SetPortRole(DEFAULT_PORT_ID, POWER_ROLE_SINK, DATA_ROLE_DEVICE);
    sleep(SLEEP_TIME);
    ret = SwitchErrCode(ret);
    g_portRoleSupported = (ret == HDF_SUCCESS);
}

void UsbBenchmarkPortTest::TearDown(const ::benchmark::State &state) {}

/**
 * @tc.name: PortQueryPort
 * @tc.desc: Test functions to QueryPort benchmark test
 * @tc.desc: int32_t QueryPort(int32_t &portId, int32_t &powerRole, int32_t &dataRole, int32_t &mode);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPort)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t portId = DEFAULT_PORT_ID;
    int32_t powerRole = POWER_ROLE_NONE;
    int32_t dataRole = DATA_ROLE_NONE;
    int32_t mode = PORT_MODE_NONE;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbPortInterface->QueryPort(portId, powerRole, dataRole, mode);
    }
    ASSERT_EQ(0, ret);
    EXPECT_GE(portId, 0);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPort)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortQueryPorts
 * @tc.desc: Test functions to QueryPorts benchmark test
 * @tc.desc: int32_t QueryPorts(std::vector<UsbPort>& portList);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPorts)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    std::vector<UsbPort> portList;
    int32_t ret = 0;
    for (auto _ : state) {
        portList.clear();
        ret = g_usbPortInterface->QueryPorts(portList);
    }
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(portList.empty());
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPorts)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortQueryPortRepeatedConsistency
 * @tc.desc: Benchmark test that repeatedly queries port and verifies result consistency.
 * @tc.desc: int32_t QueryPort(int32_t &portId, int32_t &powerRole, int32_t &dataRole, int32_t &mode);
 * @tc.desc: Positive test: parameters correctly, repeated query returns stable result
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPortRepeatedConsistency)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t portIdFirst = 0;
    int32_t powerRoleFirst = 0;
    int32_t dataRoleFirst = 0;
    int32_t modeFirst = 0;
    ASSERT_EQ(0, g_usbPortInterface->QueryPort(portIdFirst, powerRoleFirst, dataRoleFirst, modeFirst));
    int32_t ret = 0;
    for (auto _ : state) {
        int32_t portId = 0;
        int32_t powerRole = 0;
        int32_t dataRole = 0;
        int32_t mode = 0;
        ret = g_usbPortInterface->QueryPort(portId, powerRole, dataRole, mode);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(portIdFirst, portId);
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPortRepeatedConsistency)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortBindUsbdPortSubscriber
 * @tc.desc: Test functions to BindUsbdPortSubscriber benchmark test
 * @tc.desc: int32_t BindUsbdPortSubscriber(const sptr<IUsbdSubscriber> &subscriber);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortBindUsbdPortSubscriber)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        sptr<UsbdPortSubscriberTest> subscriber = new UsbdPortSubscriberTest();
        ASSERT_NE(subscriber, nullptr);
        ret = g_usbPortInterface->BindUsbdPortSubscriber(subscriber);
        EXPECT_EQ(0, ret);
        ret = g_usbPortInterface->UnbindUsbdPortSubscriber(subscriber);
        EXPECT_EQ(0, ret);
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortBindUsbdPortSubscriber)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortUnbindUsbdPortSubscriber
 * @tc.desc: Test functions to UnbindUsbdPortSubscriber benchmark test
 * @tc.desc: int32_t UnbindUsbdPortSubscriber(const sptr<IUsbdSubscriber> &subscriber);
 * @tc.desc: Positive test: parameters correctly, unbind after bind
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortUnbindUsbdPortSubscriber)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    sptr<UsbdPortSubscriberTest> subscriber = new UsbdPortSubscriberTest();
    ASSERT_NE(subscriber, nullptr);
    ASSERT_EQ(0, g_usbPortInterface->BindUsbdPortSubscriber(subscriber));
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbPortInterface->UnbindUsbdPortSubscriber(subscriber);
    }
    // Unbind of an already unbound subscriber may return an error; that is acceptable here.
    (void)ret;
    // restore bound state by re-binding then cleaning up
    ret = g_usbPortInterface->BindUsbdPortSubscriber(subscriber);
    EXPECT_EQ(0, ret);
    ret = g_usbPortInterface->UnbindUsbdPortSubscriber(subscriber);
    EXPECT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortUnbindUsbdPortSubscriber)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortBindUnbindCycle
 * @tc.desc: Benchmark test of repeated bind/unbind cycle of port subscriber.
 * @tc.desc: int32_t BindUsbdPortSubscriber / UnbindUsbdPortSubscriber
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortBindUnbindCycle)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        sptr<UsbdPortSubscriberTest> subscriber = new UsbdPortSubscriberTest();
        ASSERT_NE(subscriber, nullptr);
        ret = g_usbPortInterface->BindUsbdPortSubscriber(subscriber);
        EXPECT_EQ(0, ret);
        ret = g_usbPortInterface->UnbindUsbdPortSubscriber(subscriber);
        EXPECT_EQ(0, ret);
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortBindUnbindCycle)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortQueryPortWithInvalidPortId
 * @tc.desc: Benchmark test for QueryPort after setting port role with invalid port id.
 * @tc.desc: int32_t QueryPort(int32_t &portId, int32_t &powerRole, int32_t &dataRole, int32_t &mode);
 * @tc.desc: Negative test: invalid port id for SetPortRole, then verify QueryPort still returns valid result
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPortWithInvalidPortId)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    // Issue invalid SetPortRole first, ignore return; QueryPort should still succeed.
    (void)g_usbPortInterface->SetPortRole(INVALID_PORT_ID, POWER_ROLE_SOURCE, DATA_ROLE_HOST);
    int32_t portId = 0;
    int32_t powerRole = 0;
    int32_t dataRole = 0;
    int32_t mode = 0;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbPortInterface->QueryPort(portId, powerRole, dataRole, mode);
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPortWithInvalidPortId)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortSetPortRoleNoneNone
 * @tc.desc: Benchmark test for SetPortRole with POWER_ROLE_NONE / DATA_ROLE_NONE.
 * @tc.desc: int32_t SetPortRole(int32_t portId, int32_t powerRole, int32_t dataRole);
 * @tc.desc: Boundary test: none/none role, expected to fail gracefully without crashing
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortSetPortRoleNoneNone)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_usbPortInterface->SetPortRole(DEFAULT_PORT_ID, POWER_ROLE_NONE, DATA_ROLE_NONE);
    }
    // None/None typically fails; ensure no crash. Restore valid role afterwards.
    (void)ret;
    if (g_portRoleSupported) {
        ret = g_usbPortInterface->SetPortRole(DEFAULT_PORT_ID, POWER_ROLE_SINK, DATA_ROLE_DEVICE);
        ret = SwitchErrCode(ret);
        EXPECT_EQ(0, ret);
    }
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortSetPortRoleNoneNone)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortQueryPortsPortListSize
 * @tc.desc: Benchmark test for QueryPorts and verify portList size is consistent across calls.
 * @tc.desc: int32_t QueryPorts(std::vector<UsbPort>& portList);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPortsPortListSize)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    std::vector<UsbPort> firstPortList;
    ASSERT_EQ(0, g_usbPortInterface->QueryPorts(firstPortList));
    size_t expectedSize = firstPortList.size();
    int32_t ret = 0;
    for (auto _ : state) {
        std::vector<UsbPort> portList;
        ret = g_usbPortInterface->QueryPorts(portList);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(expectedSize, portList.size());
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPortsPortListSize)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: PortQueryPortAndVerifyRoles
 * @tc.desc: Benchmark test that queries port and verifies returned roles are within valid enum range.
 * @tc.desc: int32_t QueryPort(int32_t &portId, int32_t &powerRole, int32_t &dataRole, int32_t &mode);
 * @tc.desc: Positive test: parameters correctly, role range check
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkPortTest, PortQueryPortAndVerifyRoles)(benchmark::State &state)
{
    ASSERT_NE(g_usbPortInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        int32_t portId = 0;
        int32_t powerRole = 0;
        int32_t dataRole = 0;
        int32_t mode = 0;
        ret = g_usbPortInterface->QueryPort(portId, powerRole, dataRole, mode);
        EXPECT_EQ(0, ret);
        EXPECT_GE(powerRole, POWER_ROLE_NONE);
        EXPECT_LE(powerRole, POWER_ROLE_MAX);
        EXPECT_GE(dataRole, DATA_ROLE_NONE);
        EXPECT_LE(dataRole, DATA_ROLE_MAX);
        EXPECT_GE(mode, PORT_MODE_NONE);
        EXPECT_LE(mode, PORT_MODE_HOST);
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkPortTest, PortQueryPortAndVerifyRoles)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

} // namespace

BENCHMARK_MAIN();
