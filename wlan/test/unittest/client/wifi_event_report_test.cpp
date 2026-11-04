/*
 * Copyright (c) 2021-2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "wifi_common_cmd.h"
#include "wifi_driver_client.h"

using namespace testing::ext;

namespace WlanEventReportTest {

const char *P2P0_IFNAME = "p2p0";
const char *WDLENHANCE0_IFNAME = "wdlenhance0";
const uint32_t ACTION_RECEIVED_EVENT_BIT = (1U << WIFI_EVENT_ACTION_RECEIVED);

int32_t g_callbackACount = 0;
int32_t g_callbackBCount = 0;

int32_t TestCallbackA(uint32_t event, void *data, const char *ifName)
{
    (void)event;
    (void)data;
    (void)ifName;
    g_callbackACount++;
    return RET_CODE_SUCCESS;
}

int32_t TestCallbackB(uint32_t event, void *data, const char *ifName)
{
    (void)event;
    (void)data;
    (void)ifName;
    g_callbackBCount++;
    return RET_CODE_SUCCESS;
}

class WifiEventReportTest : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;

private:
    static bool isInitialized;
};

bool WifiEventReportTest::isInitialized = false;

void WifiEventReportTest::SetUp()
{
    g_callbackACount = 0;
    g_callbackBCount = 0;
    isInitialized = (WifiDriverClientInit() == RET_CODE_SUCCESS);
    ASSERT_TRUE(isInitialized);
}

void WifiEventReportTest::TearDown()
{
    if (!isInitialized) {
        return;
    }
    WifiUnregisterEventCallback(TestCallbackA, ACTION_RECEIVED_EVENT_BIT, P2P0_IFNAME);
    WifiUnregisterEventCallback(TestCallbackB, ACTION_RECEIVED_EVENT_BIT, P2P0_IFNAME);
    WifiUnregisterEventCallback(TestCallbackB, ACTION_RECEIVED_EVENT_BIT, WDLENHANCE0_IFNAME);
    WifiDriverClientDeinit();
}

/**
 * @tc.name: WifiGetActionReportIfName001
 * @tc.desc: Names other than "wdlenhance" netdevs are normalized to "p2p0"
 * (regression guard for the "chba0" TX status silent drop)
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiGetActionReportIfName001, TestSize.Level1)
{
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName("chba0"));
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName("wlan0"));
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName("wlan1"));
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName("p2p0-461"));
}

/**
 * @tc.name: WifiGetActionReportIfName002
 * @tc.desc: "p2p0" passes through unchanged (exact-match passthrough)
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiGetActionReportIfName002, TestSize.Level1)
{
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName("p2p0"));
}

/**
 * @tc.name: WifiGetActionReportIfName003
 * @tc.desc: "wdlenhance" VAP interface names are preserved as-is
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiGetActionReportIfName003, TestSize.Level1)
{
    EXPECT_STREQ("wdlenhance0", WifiGetActionReportIfName("wdlenhance0"));
    EXPECT_STREQ("wdlenhance1", WifiGetActionReportIfName("wdlenhance1"));
}

/**
 * @tc.name: WifiGetActionReportIfName004
 * @tc.desc: NULL ifName returns "p2p0" (defensive contract)
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiGetActionReportIfName004, TestSize.Level1)
{
    EXPECT_STREQ("p2p0", WifiGetActionReportIfName(nullptr));
}

/**
 * @tc.name: WifiEventReportIfNameDispatch001
 * @tc.desc: ACTION_RECEIVED report on "p2p0" fires the "p2p0" subscriber;
 * a raw netdev name ("chba0") matches no subscriber and is silently dropped
 * (documents the exact-strcmp hazard that caused the incident)
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiEventReportIfNameDispatch001, TestSize.Level1)
{
    uint8_t frameBuf[4] = { 0x80, 0x00, 0x00, 0x00 };
    WifiActionData actionData = {};
    actionData.data = frameBuf;
    actionData.dataLen = sizeof(frameBuf);

    ASSERT_EQ(RET_CODE_SUCCESS, WifiRegisterEventCallback(TestCallbackA, ACTION_RECEIVED_EVENT_BIT, P2P0_IFNAME));
    EXPECT_EQ(0, g_callbackACount);

    WifiEventReport(P2P0_IFNAME, WIFI_EVENT_ACTION_RECEIVED, &actionData);
    EXPECT_EQ(1, g_callbackACount);

    WifiEventReport("chba0", WIFI_EVENT_ACTION_RECEIVED, &actionData);
    EXPECT_EQ(1, g_callbackACount);
}

/**
 * @tc.name: WifiEventReportIfNameDispatch002
 * @tc.desc: "p2p0" and "wdlenhance0" subscribers dispatch independently
 * @tc.type: FUNC
 * @tc.require: AR20260831417042
 */
HWTEST_F(WifiEventReportTest, WifiEventReportIfNameDispatch002, TestSize.Level1)
{
    uint8_t frameBuf[4] = { 0x80, 0x00, 0x00, 0x00 };
    WifiActionData actionData = {};
    actionData.data = frameBuf;
    actionData.dataLen = sizeof(frameBuf);

    ASSERT_EQ(RET_CODE_SUCCESS, WifiRegisterEventCallback(TestCallbackA, ACTION_RECEIVED_EVENT_BIT, P2P0_IFNAME));
    ASSERT_EQ(RET_CODE_SUCCESS,
              WifiRegisterEventCallback(TestCallbackB, ACTION_RECEIVED_EVENT_BIT, WDLENHANCE0_IFNAME));

    WifiEventReport(WDLENHANCE0_IFNAME, WIFI_EVENT_ACTION_RECEIVED, &actionData);
    EXPECT_EQ(0, g_callbackACount);
    EXPECT_EQ(1, g_callbackBCount);

    WifiEventReport(P2P0_IFNAME, WIFI_EVENT_ACTION_RECEIVED, &actionData);
    EXPECT_EQ(1, g_callbackACount);
    EXPECT_EQ(1, g_callbackBCount);
}

} // namespace WlanEventReportTest
