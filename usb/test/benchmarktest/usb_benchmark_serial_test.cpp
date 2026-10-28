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
#include "v1_0/iserial_interface.h"
#include "v1_0/serial_types.h"

using namespace benchmark::internal;
using namespace OHOS;
using namespace std;
using namespace OHOS::HDI::Usb::Serial::V1_0;

namespace {
sptr<ISerialInterface> g_serialInterface = nullptr;
std::vector<SerialPort> g_portList;

constexpr int32_t ITERATION_FREQUENCY = 100;
constexpr int32_t REPETITION_FREQUENCY = 3;
constexpr int32_t VALID_PORTID = 0;
constexpr int32_t INVALID_PORTID = -1;
constexpr int32_t SERIAL_TIMEOUT_MS = 1000;
constexpr int32_t SERIAL_READ_BUFFER_SIZE = 1024;

class UsbBenchmarkSerialTest : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};

void UsbBenchmarkSerialTest::SetUp(const ::benchmark::State &state)
{
    g_serialInterface = ISerialInterface::Get("serial_interface_service", true);
    ASSERT_NE(g_serialInterface, nullptr);
    if (g_portList.empty()) {
        (void)g_serialInterface->SerialGetPortList(g_portList);
    }
}

void UsbBenchmarkSerialTest::TearDown(const ::benchmark::State &state)
{
    if (g_serialInterface != nullptr) {
        (void)g_serialInterface->SerialClose(VALID_PORTID);
    }
}

/**
 * @tc.name: SerialGetPortList
 * @tc.desc: Test functions to SerialGetPortList benchmark test
 * @tc.desc: int32_t SerialGetPortList(std::vector<SerialPort> &portList);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialGetPortList)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    std::vector<SerialPort> portList;
    int32_t ret = 0;
    for (auto _ : state) {
        portList.clear();
        ret = g_serialInterface->SerialGetPortList(portList);
    }
    EXPECT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialGetPortList)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialGetPortListConsistency
 * @tc.desc: Benchmark test that repeatedly gets port list and verifies consistency.
 * @tc.desc: int32_t SerialGetPortList(std::vector<SerialPort> &portList);
 * @tc.desc: Positive test: parameters correctly, repeated calls return stable list size
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialGetPortListConsistency)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    std::vector<SerialPort> firstList;
    ASSERT_EQ(0, g_serialInterface->SerialGetPortList(firstList));
    size_t expectedSize = firstList.size();
    int32_t ret = 0;
    for (auto _ : state) {
        std::vector<SerialPort> portList;
        ret = g_serialInterface->SerialGetPortList(portList);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(expectedSize, portList.size());
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialGetPortListConsistency)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialOpenInvalid
 * @tc.desc: Test functions to SerialOpen benchmark test with invalid portId.
 * @tc.desc: int32_t SerialOpen(int32_t portId);
 * @tc.desc: Negative test: invalid portId, expected to fail without crash
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialOpenInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        (void)g_serialInterface->SerialClose(INVALID_PORTID);
        ret = g_serialInterface->SerialOpen(INVALID_PORTID);
    }
    EXPECT_NE(0, ret);
    (void)g_serialInterface->SerialClose(INVALID_PORTID);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialOpenInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialCloseInvalid
 * @tc.desc: Test functions to SerialClose benchmark test with invalid portId.
 * @tc.desc: int32_t SerialClose(int32_t portId);
 * @tc.desc: Negative test: invalid portId, no crash expected
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialCloseInvalid)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_serialInterface->SerialClose(INVALID_PORTID);
    }
    // Closing invalid port may return error; ensure no crash.
    (void)ret;
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialCloseInvalid)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialWriteInvalidPort
 * @tc.desc: Test functions to SerialWrite benchmark test with invalid portId.
 * @tc.desc: int32_t SerialWrite(int32_t portId, const std::vector<uint8_t> &data, uint32_t size, uint32_t timeout);
 * @tc.desc: Negative test: invalid portId, expected to fail without crash
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialWriteInvalidPort)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    std::vector<uint8_t> data = {'b', 'e', 'n', 'c', 'h'};
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_serialInterface->SerialWrite(INVALID_PORTID, data, data.size(), SERIAL_TIMEOUT_MS);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialWriteInvalidPort)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialReadInvalidPort
 * @tc.desc: Test functions to SerialRead benchmark test with invalid portId.
 * @tc.desc: int32_t SerialRead(int32_t portId, std::vector<uint8_t> &data, uint32_t size, uint32_t timeout);
 * @tc.desc: Negative test: invalid portId, expected to fail without crash
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialReadInvalidPort)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        std::vector<uint8_t> data;
        ret = g_serialInterface->SerialRead(INVALID_PORTID, data, SERIAL_READ_BUFFER_SIZE, SERIAL_TIMEOUT_MS);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialReadInvalidPort)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialGetAttributeInvalidPort
 * @tc.desc: Test functions to SerialGetAttribute benchmark test with invalid portId.
 * @tc.desc: int32_t SerialGetAttribute(int32_t portId, SerialAttribute &attribute);
 * @tc.desc: Negative test: invalid portId, expected to fail without crash
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialGetAttributeInvalidPort)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    SerialAttribute attribute;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_serialInterface->SerialGetAttribute(INVALID_PORTID, attribute);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialGetAttributeInvalidPort)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialSetAttributeInvalidPort
 * @tc.desc: Benchmark test that sets serial attribute with invalid portId.
 * @tc.desc: int32_t SerialSetAttribute(int32_t portId, const SerialAttribute &attribute);
 * @tc.desc: Negative test: invalid portId, expected to fail without crash
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialSetAttributeInvalidPort)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    SerialAttribute attribute;
    attribute.baudrate = BAUDRATE_115200;
    attribute.dataBits = USB_ATTR_DATABIT_8;
    attribute.parity = USB_ATTR_PARITY_NONE;
    attribute.stopBits = USB_ATTR_STOPBIT_1;
    int32_t ret = 0;
    for (auto _ : state) {
        ret = g_serialInterface->SerialSetAttribute(INVALID_PORTID, attribute);
    }
    EXPECT_NE(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialSetAttributeInvalidPort)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

/**
 * @tc.name: SerialGetPortListAndOpen
 * @tc.desc: Benchmark test that gets port list then opens the first valid port.
 * @tc.desc: int32_t SerialGetPortList / SerialOpen
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
BENCHMARK_F(UsbBenchmarkSerialTest, SerialGetPortListAndOpen)(benchmark::State &state)
{
    ASSERT_NE(g_serialInterface, nullptr);
    int32_t ret = 0;
    for (auto _ : state) {
        std::vector<SerialPort> portList;
        ret = g_serialInterface->SerialGetPortList(portList);
        EXPECT_EQ(0, ret);
        if (!portList.empty()) {
            int32_t portId = portList[0].portId;
            ret = g_serialInterface->SerialOpen(portId);
            EXPECT_EQ(0, ret);
            (void)g_serialInterface->SerialClose(portId);
        }
    }
    ASSERT_EQ(0, ret);
}

BENCHMARK_REGISTER_F(UsbBenchmarkSerialTest, SerialGetPortListAndOpen)->
    Iterations(ITERATION_FREQUENCY)->Repetitions(REPETITION_FREQUENCY)->ReportAggregatesOnly();

} // namespace

BENCHMARK_MAIN();
