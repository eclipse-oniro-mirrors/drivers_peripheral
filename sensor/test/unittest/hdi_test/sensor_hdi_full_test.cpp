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

#include <cmath>
#include <cstdio>
#include <unistd.h>
#include <gtest/gtest.h>
#include <securec.h>
#include "hdf_base.h"
#include "osal_time.h"
#include "v3_0/isensor_interface.h"
#include "v3_1/isensor_interface.h"
#include "sensor_type.h"
#include "sensor_callback_impl.h"
#include "sensor_callback_impl_test.h"
#include "sensor_plug_callback_impl.h"
#include "sensor_test_callback_impl.h"
#include "sensor_uhdf_log.h"
#include "sensor_trace.h"

using namespace OHOS::HDI::Sensor::V3_0;
using namespace OHOS::HDI::Sensor::V3_1;
using namespace OHOS::HDI::Sensor;
using namespace testing::ext;

namespace {
    sptr<V3_0::ISensorInterface> g_sensorInterfaceV3_0 = nullptr;
    sptr<V3_1::ISensorInterface> g_sensorInterfaceV3_1 = nullptr;
    sptr<V3_0::ISensorCallback> g_traditionalCallback = new SensorCallbackImpl();
    sptr<V3_0::ISensorCallback> g_medicalCallback = new SensorCallbackImpl();
    sptr<V3_0::ISensorCallback> g_failureCallback = new SensorCallbackImplTest();
    sptr<SensorTestDataCallback> g_dataCallback = new SensorTestDataCallback();
    sptr<SensorTestPlugCallback> g_plugCallback = new SensorTestPlugCallback();
    sptr<V3_0::ISensorPlugCallback> g_plugCallbackBase = g_plugCallback;
    std::vector<HdfSensorInformation> g_info;
    std::vector<HdfSensorEvents> g_events;
    std::vector<V3_0::SdcSensorInfo> g_sdcInfo;

    constexpr int64_t SENSOR_INTERVAL1 = 200000000;
    constexpr int64_t SENSOR_INTERVAL2 = 20000000;
    constexpr int64_t SENSOR_INTERVAL3 = 40000000;
    constexpr int64_t SENSOR_INTERVAL4 = 50000000;
    constexpr int64_t SENSOR_INTERVAL5 = 100000000;
    constexpr int64_t SENSOR_INTERVAL6 = 30000000;
    constexpr int32_t SENSOR_POLL_TIME = 1;
    constexpr int32_t SENSOR_WAIT_TIME = 100;
    constexpr int32_t SENSOR_WAIT_TIME2 = 500;
    constexpr int32_t SENSOR_WAIT_TIME3 = 1000;
    constexpr int32_t ABNORMAL_SENSORID = -1;
    constexpr int32_t ABNORMAL_DEVICE_ID = -999;
    constexpr int32_t RATE_LEVEL_LOW = 1;
    constexpr int32_t RATE_LEVEL_MID = 50;
    constexpr int32_t RATE_LEVEL_HIGH = 100;
    constexpr uint32_t TEST_CALLBACK_ID = 100001;
    constexpr uint32_t TEST_CALLBACK_ID_THP = 100011;
    constexpr uint32_t INVALID_CALLBACK_ID = 99999;
    constexpr uint32_t SENSOR_OPTION_DEFAULT = 0;
    constexpr uint32_t SENSOR_OPTION_RANGE1 = 0;
    constexpr uint32_t SENSOR_OPTION_RANGE2 = 1;
    constexpr uint32_t SENSOR_OPTION_ACCURACY = 2;
    constexpr int32_t SENSOR_MODE_DEFAULT = 0;
    constexpr int32_t SENSOR_MODE_REALTIME = 1;
    constexpr int32_t SENSOR_MODE_ON_CHANGE = 2;
    constexpr int32_t SENSOR_MODE_ONE_SHOT = 3;
    constexpr int32_t SENSOR_MODE_FIFO = 4;
    constexpr int32_t SENSOR_MODE_INVALID = 99;

    bool IsSensorInList(int32_t sensorType)
    {
        for (const auto& iter : g_info) {
            if (iter.deviceSensorInfo.sensorType == sensorType) {
                return true;
            }
        }
        return false;
    }

    V3_0::DeviceSensorInfo GetFirstSensorInfo()
    {
        if (g_info.empty()) {
            return {-1, 0, 0, 1};
        }
        return g_info[0].deviceSensorInfo;
    }

    class SensorHdiFullTest : public testing::Test {
    public:
        static void SetUpTestCase();
        static void TearDownTestCase();
        void SetUp();
        void TearDown();
    };

    void SensorHdiFullTest::SetUpTestCase()
    {
        g_sensorInterfaceV3_0 = V3_0::ISensorInterface::Get();
        g_sensorInterfaceV3_1 = V3_1::ISensorInterface::Get();
        if (g_sensorInterfaceV3_0 != nullptr) {
            g_sensorInterfaceV3_0->GetAllSensorInfo(g_info);
        }
    }

    void SensorHdiFullTest::TearDownTestCase()
    {
    }

    void SensorHdiFullTest::SetUp()
    {
        g_dataCallback->Reset();
        g_plugCallback->Reset();
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    void SensorHdiFullTest::TearDown()
    {
    }

    /**
     * @tc.name: SensorHdiFullTest_GetSensorClient_001
     * @tc.desc: Get v3_0 ISensorInterface client and check whether it is not null.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetSensorClient_001, TestSize.Level1)
    {
        ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
    }

    /**
     * @tc.name: SensorHdiFullTest_GetSensorClient_002
     * @tc.desc: Get v3_1 ISensorInterface client and check whether it is not null.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetSensorClient_002, TestSize.Level1)
    {
        ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
    }

    /**
     * @tc.name: SensorHdiFullTest_GetAllSensorInfo_001
     * @tc.desc: Call GetAllSensorInfo to obtain all sensor information and verify the returned list is not empty.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetAllSensorInfo_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<HdfSensorInformation> info;
        int32_t ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_GT(info.size(), static_cast<size_t>(0));
        for (const auto& sensor : info) {
            EXPECT_GT(sensor.sensorName.size(), static_cast<size_t>(0));
            EXPECT_GT(sensor.vendorName.size(), static_cast<size_t>(0));
            EXPECT_GE(sensor.maxRange, 0.0f);
            EXPECT_GE(sensor.power, 0.0f);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_GetAllSensorInfo_002
     * @tc.desc: Call GetAllSensorInfo multiple times to verify stable output.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetAllSensorInfo_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<HdfSensorInformation> info1;
        std::vector<HdfSensorInformation> info2;
        int32_t ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info1);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info2);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_EQ(info1.size(), info2.size());
    }

    /**
     * @tc.name: SensorHdiFullTest_GetDeviceSensorInfo_001
     * @tc.desc: Call GetDeviceSensorInfo with a valid deviceId and verify the returned info.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetDeviceSensorInfo_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<HdfSensorInformation> info;
        int32_t ret = g_sensorInterfaceV3_0->GetDeviceSensorInfo(-1, info);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        for (const auto& sensor : info) {
            EXPECT_EQ(-1, sensor.deviceSensorInfo.deviceId);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_GetDeviceSensorInfo_002
     * @tc.desc: Call GetDeviceSensorInfo with an abnormal deviceId and verify it returns failure or empty.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetDeviceSensorInfo_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<HdfSensorInformation> info;
        int32_t ret = g_sensorInterfaceV3_0->GetDeviceSensorInfo(ABNORMAL_DEVICE_ID, info);
        EXPECT_NE(SENSOR_SUCCESS, ret);
        EXPECT_EQ(static_cast<size_t>(0), info.size());
    }

    /**
     * @tc.name: SensorHdiFullTest_Register_001
     * @tc.desc: Register a traditional sensor callback and verify success, then unregister.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Register_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Register_002
     * @tc.desc: Register a medical sensor callback and verify success, then unregister.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Register_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(MEDICAL_SENSOR_TYPE, g_medicalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Unregister(MEDICAL_SENSOR_TYPE, g_medicalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Register_003
     * @tc.desc: Register with an invalid groupId (SENSOR_GROUP_TYPE_MAX) and expect SENSOR_INVALID_PARAM.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Register_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(SENSOR_GROUP_TYPE_MAX, g_traditionalCallback);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
        ret = g_sensorInterfaceV3_0->Unregister(SENSOR_GROUP_TYPE_MAX, g_traditionalCallback);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Register_004
     * @tc.desc: Register the same callback twice in the same group and verify idempotent behavior.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Register_004, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Enable_001
     * @tc.desc: Register, SetBatch, Enable, wait, Disable for each sensor in the list and verify success.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Enable_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_GT(g_info.size(), static_cast<size_t>(0));
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_EQ(SensorCallbackImpl::sensorDataFlag, 1);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_Enable_002
     * @tc.desc: Enable an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Enable_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_0->Enable(abnormalInfo);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
        ret = g_sensorInterfaceV3_0->Disable(abnormalInfo);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Enable_003
     * @tc.desc: Enable a sensor without registering a callback first, verify behavior.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Enable_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Enable(devInfo);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        OsalMSleep(SENSOR_WAIT_TIME);
        ret = g_sensorInterfaceV3_0->Disable(devInfo);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_Disable_001
     * @tc.desc: Disable a sensor that was never enabled and verify graceful handling.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_Disable_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t ret = g_sensorInterfaceV3_0->Disable(devInfo);
        EXPECT_NE(SENSOR_INVALID_PARAM, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetBatch_001
     * @tc.desc: SetBatch with different sampling intervals for all sensors, enable, wait, disable.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetBatch_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        int64_t intervals[] = {SENSOR_INTERVAL1, SENSOR_INTERVAL2, SENSOR_INTERVAL3,
                               SENSOR_INTERVAL4, SENSOR_INTERVAL5, SENSOR_INTERVAL6};
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            for (int64_t interval : intervals) {
                ret = g_sensorInterfaceV3_0->SetBatch(devInfo, interval, SENSOR_POLL_TIME);
                EXPECT_EQ(SENSOR_SUCCESS, ret);
            }
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_SetBatch_002
     * @tc.desc: SetBatch with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetBatch_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_0->SetBatch(abnormalInfo, 0, 0);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetBatch_003
     * @tc.desc: SetBatch with zero sampling interval and zero report interval on valid sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetBatch_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetBatch(devInfo, 0, 0);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_001
     * @tc.desc: SetMode with SENSOR_MODE_DEFAULT for all sensors, enable, wait, disable.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_DEFAULT);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_002
     * @tc.desc: SetMode with SENSOR_MODE_REALTIME for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_REALTIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_003
     * @tc.desc: SetMode with SENSOR_MODE_ON_CHANGE for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_ON_CHANGE);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_004
     * @tc.desc: SetMode with SENSOR_MODE_ONE_SHOT for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_004, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_ONE_SHOT);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_005
     * @tc.desc: SetMode with SENSOR_MODE_FIFO for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_005, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_FIFO);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetMode_006
     * @tc.desc: SetMode with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetMode_006, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_0->SetMode(abnormalInfo, SENSOR_MODE_DEFAULT);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetOption_001
     * @tc.desc: SetOption with default option (0) for all sensors, enable, wait, disable.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetOption_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetOption(devInfo, SENSOR_OPTION_DEFAULT);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_SetOption_002
     * @tc.desc: SetOption with different option values (range and accuracy levels).
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetOption_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        uint32_t options[] = {SENSOR_OPTION_RANGE1, SENSOR_OPTION_RANGE2, SENSOR_OPTION_ACCURACY};
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            for (uint32_t option : options) {
                int32_t ret = g_sensorInterfaceV3_0->SetOption(devInfo, option);
                EXPECT_EQ(SENSOR_SUCCESS, ret);
            }
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetOption_003
     * @tc.desc: SetOption with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetOption_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_0->SetOption(abnormalInfo, SENSOR_OPTION_DEFAULT);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_ReadData_001
     * @tc.desc: ReadData from each sensor in the list and verify the returned events vector.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_ReadData_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            std::vector<HdfSensorEvents> events;
            int32_t ret = g_sensorInterfaceV3_0->ReadData(devInfo, events);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_ReadData_002
     * @tc.desc: ReadData with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_ReadData_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        std::vector<HdfSensorEvents> events;
        int32_t ret = g_sensorInterfaceV3_0->ReadData(abnormalInfo, events);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetSdcSensor_001
     * @tc.desc: SetSdcSensor to enable with a low rate level and then disable for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetSdcSensor_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            int32_t ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, true, RATE_LEVEL_LOW);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, false, RATE_LEVEL_LOW);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetSdcSensor_002
     * @tc.desc: SetSdcSensor with different rate levels (low, mid, high).
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetSdcSensor_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t rateLevels[] = {RATE_LEVEL_LOW, RATE_LEVEL_MID, RATE_LEVEL_HIGH};
        for (int32_t rateLevel : rateLevels) {
            int32_t ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, true, rateLevel);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, false, rateLevel);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
    }

    /**
     * @tc.name: SensorHdiFullTest_SetSdcSensor_003
     * @tc.desc: SetSdcSensor with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetSdcSensor_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_0->SetSdcSensor(abnormalInfo, true, RATE_LEVEL_MID);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_GetSdcSensorInfo_001
     * @tc.desc: GetSdcSensorInfo and verify the returned vector.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetSdcSensorInfo_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<V3_0::SdcSensorInfo> sdcInfo;
        int32_t ret = g_sensorInterfaceV3_0->GetSdcSensorInfo(sdcInfo);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_GetSdcSensorInfo_002
     * @tc.desc: GetSdcSensorInfo multiple times and verify stable results.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_GetSdcSensorInfo_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        std::vector<V3_0::SdcSensorInfo> sdcInfo1;
        std::vector<V3_0::SdcSensorInfo> sdcInfo2;
        int32_t ret = g_sensorInterfaceV3_0->GetSdcSensorInfo(sdcInfo1);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->GetSdcSensorInfo(sdcInfo2);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_EQ(sdcInfo1.size(), sdcInfo2.size());
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterAsync_001
     * @tc.desc: RegisterAsync for traditional sensors, then UnregisterAsync.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterAsync_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegisterAsync(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->UnregisterAsync(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterAsync_002
     * @tc.desc: RegisterAsync for medical sensors, then UnregisterAsync.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterAsync_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegisterAsync(MEDICAL_SENSOR_TYPE, g_medicalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->UnregisterAsync(MEDICAL_SENSOR_TYPE, g_medicalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterAsync_003
     * @tc.desc: RegisterAsync with invalid groupId (SENSOR_GROUP_TYPE_MAX) and expect failure.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterAsync_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegisterAsync(SENSOR_GROUP_TYPE_MAX, g_traditionalCallback);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
        ret = g_sensorInterfaceV3_0->UnregisterAsync(SENSOR_GROUP_TYPE_MAX, g_traditionalCallback);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegSensorPlugCallBack_001
     * @tc.desc: RegSensorPlugCallBack with a valid callback and then UnRegSensorPlugCallBack.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegSensorPlugCallBack_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegSensorPlugCallBack(g_plugCallbackBase);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->UnRegSensorPlugCallBack(g_plugCallbackBase);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegSensorPlugCallBack_002
     * @tc.desc: UnRegSensorPlugCallBack without prior registration and verify graceful handling.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegSensorPlugCallBack_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        sptr<V3_0::ISensorPlugCallback> unregisteredCallback = new SensorPlugCallbackImpl();
        int32_t ret = g_sensorInterfaceV3_0->UnRegSensorPlugCallBack(unregisteredCallback);
        EXPECT_NE(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegSensorPlugCallBack_003
     * @tc.desc: Register the same plug callback twice and verify idempotent behavior.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegSensorPlugCallBack_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegSensorPlugCallBack(g_plugCallbackBase);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->RegSensorPlugCallBack(g_plugCallbackBase);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->UnRegSensorPlugCallBack(g_plugCallbackBase);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_EnableWithCallbackId_001
     * @tc.desc: RegisterWithCallbackId, SetBatchWithCallbackId, EnableWithCallbackId, wait, DisableWithCallbackId.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_EnableWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_GT(g_info.size(), static_cast<size_t>(0));
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_1->SetBatchWithCallbackId(
                devInfo, TEST_CALLBACK_ID, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_1->EnableWithCallbackId(devInfo, TEST_CALLBACK_ID);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_1->DisableWithCallbackId(devInfo, TEST_CALLBACK_ID);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_EnableWithCallbackId_002
     * @tc.desc: EnableWithCallbackId with an invalid callbackId and expect failure.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_EnableWithCallbackId_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t ret = g_sensorInterfaceV3_1->EnableWithCallbackId(devInfo, INVALID_CALLBACK_ID);
        EXPECT_NE(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_EnableWithCallbackId_003
     * @tc.desc: EnableWithCallbackId with an abnormal sensorType and expect SENSOR_NOT_SUPPORT.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_EnableWithCallbackId_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
        int32_t ret = g_sensorInterfaceV3_1->EnableWithCallbackId(abnormalInfo, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_DisableWithCallbackId_001
     * @tc.desc: DisableWithCallbackId without prior enable and verify graceful handling.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_DisableWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t ret = g_sensorInterfaceV3_1->DisableWithCallbackId(devInfo, TEST_CALLBACK_ID);
        EXPECT_NE(SENSOR_INVALID_PARAM, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetBatchWithCallbackId_001
     * @tc.desc: SetBatchWithCallbackId with various intervals for all sensors.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetBatchWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        int64_t intervals[] = {SENSOR_INTERVAL1, SENSOR_INTERVAL2, SENSOR_INTERVAL3, SENSOR_INTERVAL4};
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            for (int64_t interval : intervals) {
                ret = g_sensorInterfaceV3_1->SetBatchWithCallbackId(
                    devInfo, TEST_CALLBACK_ID, interval, SENSOR_POLL_TIME);
                EXPECT_EQ(SENSOR_SUCCESS, ret);
            }
        }
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_SetBatchWithCallbackId_002
     * @tc.desc: SetBatchWithCallbackId with an invalid callbackId and expect failure.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SetBatchWithCallbackId_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        int32_t ret = g_sensorInterfaceV3_1->SetBatchWithCallbackId(
            devInfo, INVALID_CALLBACK_ID, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
        EXPECT_NE(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterWithCallbackId_001
     * @tc.desc: RegisterWithCallbackId for traditional group with a GPS callbackId, then unregister.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterWithCallbackId_002
     * @tc.desc: RegisterWithCallbackId for medical group with a THP callbackId, then unregister.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterWithCallbackId_002, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            MEDICAL_SENSOR_TYPE, g_medicalCallback, TEST_CALLBACK_ID_THP);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            MEDICAL_SENSOR_TYPE, g_medicalCallback, TEST_CALLBACK_ID_THP);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterWithCallbackId_003
     * @tc.desc: RegisterWithCallbackId with invalid groupId and expect SENSOR_INVALID_PARAM.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterWithCallbackId_003, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            SENSOR_GROUP_TYPE_MAX, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            SENSOR_GROUP_TYPE_MAX, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_RegisterWithCallbackId_004
     * @tc.desc: RegisterWithCallbackId with an invalid callbackId (below CALLBACK_ID_BEGIN).
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_RegisterWithCallbackId_004, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, INVALID_CALLBACK_ID);
        EXPECT_NE(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_UnregisterWithCallbackId_001
     * @tc.desc: UnregisterWithCallbackId without prior registration and verify graceful handling.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_UnregisterWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_NE(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_FullWorkflow_001
     * @tc.desc: Full end-to-end workflow: Register, SetBatch, SetMode, SetOption, Enable, wait, Disable, Unregister.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_FullWorkflow_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_GT(g_info.size(), static_cast<size_t>(0));
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_DEFAULT);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->SetOption(devInfo, SENSOR_OPTION_DEFAULT);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->ReadData(devInfo, g_events);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_FullWorkflowWithCallbackId_001
     * @tc.desc: Full end-to-end v3_1 workflow using CallbackId variants.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_FullWorkflowWithCallbackId_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_1 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        EXPECT_GT(g_info.size(), static_cast<size_t>(0));
        for (const auto& iter : g_info) {
            V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
            ret = g_sensorInterfaceV3_1->SetBatchWithCallbackId(
                devInfo, TEST_CALLBACK_ID, SENSOR_INTERVAL2, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_1->EnableWithCallbackId(devInfo, TEST_CALLBACK_ID);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_1->DisableWithCallbackId(devInfo, TEST_CALLBACK_ID);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
            TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_SdcCoexistence_001
     * @tc.desc: Enable SA and SDC simultaneously, then disable SDC first, then SA.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_SdcCoexistence_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        if (g_info.empty()) {
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
        ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Enable(devInfo);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, true, RATE_LEVEL_MID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        OsalMSleep(SENSOR_WAIT_TIME);
        ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, false, RATE_LEVEL_MID);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Disable(devInfo);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        SensorCallbackImpl::sensorDataFlag = 1;
    }

    /**
     * @tc.name: SensorHdiFullTest_AsyncCallbackWithData_001
     * @tc.desc: RegisterAsync, Enable, wait for data, verify callback received events, Disable, UnregisterAsync.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_AsyncCallbackWithData_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->RegisterAsync(TRADITIONAL_SENSOR_TYPE, g_dataCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        if (!g_info.empty()) {
            V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME2);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            EXPECT_GE(g_dataCallback->GetEventCount(), 0);
        }
        ret = g_sensorInterfaceV3_0->UnregisterAsync(TRADITIONAL_SENSOR_TYPE, g_dataCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }

    /**
     * @tc.name: SensorHdiFullTest_FailureCallback_001
     * @tc.desc: Register a failure callback, Enable, wait, Disable, Unregister. Verify no crash.
     * @tc.type: FUNC
     * @tc.require: SR000F869M
     */
    HWTEST_F(SensorHdiFullTest, SensorHdiFullTest_FailureCallback_001, TestSize.Level1)
    {
        if (g_sensorInterfaceV3_0 == nullptr) {
            ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
            return;
        }
        int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_failureCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
        if (!g_info.empty()) {
            V3_0::DeviceSensorInfo devInfo = g_info[0].deviceSensorInfo;
            ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            ret = g_sensorInterfaceV3_0->Enable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
            OsalMSleep(SENSOR_WAIT_TIME);
            ret = g_sensorInterfaceV3_0->Disable(devInfo);
            EXPECT_EQ(SENSOR_SUCCESS, ret);
        }
        ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_failureCallback);
        EXPECT_EQ(SENSOR_SUCCESS, ret);
    }
}
