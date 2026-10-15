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

/*
 * Basic mock tests for the USB DDK service: service and device state management.
 *
 * Scope (intentionally small for now):
 *   - service lifecycle: Init / Release
 *   - device state: claim / release interface, select / query interface setting
 * Descriptor, transfer, driver-info and permission cases are parked under
 * test/unittest/mock_libusb_ddk/pending/ and can be re-enabled later.
 *
 * Command line:
 *   --dump-transfers   print every transfer handled by the mock libusb
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "hdf_base.h"
#include "hdf_log.h"
#include "mock_ddk_permission_control.h"
#include "mock_libusb_control.h"
#include "v1_2/usb_ddk_service.h"

using namespace OHOS::HDI::Usb::Ddk::V1_2;
using namespace testing::ext;

/* Named constants of the mock device model used by these tests. */
#define BUS_NUM 1
#define ADDR_MAIN 2
#define MAIN_VID 0x1209
#define MAIN_PID 0x0001
#define INTERFACE_ID 0
#define ALT_SETTING_1 1
#define INVALID_ALT_SETTING 0xFF
/* MakeDeviceId() packs the bus number into the high bits of the device id. */
#define DEVICE_ID_BUS_SHIFT 32
/* Handle/address values for the negative branches. */
#define INVALID_HANDLE_OFFSET 0x1234
#define MISSING_DEVICE_ADDR 0x40

namespace {

uint64_t MakeDeviceId(uint8_t address)
{
    return (static_cast<uint64_t>(BUS_NUM) << DEVICE_ID_BUS_SHIFT) | address;
}

class DdkStateBasicTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static UsbDdkService service_;
    static uint64_t mainDeviceId_;
};

UsbDdkService DdkStateBasicTest::service_;
uint64_t DdkStateBasicTest::mainDeviceId_ = MakeDeviceId(ADDR_MAIN);

void DdkStateBasicTest::SetUpTestCase()
{
    mock_ddk::SetPermissionGranted(true);
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    ASSERT_EQ(HDF_SUCCESS, service_.Init());
}

void DdkStateBasicTest::TearDownTestCase()
{
    (void)service_.Release();
}

/**
 * @tc.name: InitAndReleaseLifecycle
 * @tc.desc: Test functions to Init / Release
 * @tc.desc: int32_t Init();
 * @tc.desc: int32_t Release();
 * @tc.desc: Positive test: repeated Init is idempotent and the service is usable again after Release
 * @tc.type: FUNC
 */
HWTEST_F(DdkStateBasicTest, InitAndReleaseLifecycle, TestSize.Level1)
{
    EXPECT_EQ(HDF_SUCCESS, service_.Init()); // idempotent while already initialised
    ASSERT_EQ(HDF_SUCCESS, service_.Release());
    EXPECT_EQ(HDF_SUCCESS, service_.Init()); // usable again
}

/**
 * @tc.name: ClaimAndInterfaceSettingState
 * @tc.desc: Test functions to ClaimInterface / ReleaseInterface / SelectInterfaceSetting /
 *           GetCurrentInterfaceSetting
 * @tc.desc: int32_t ClaimInterface(uint64_t deviceId, uint8_t interfaceIndex, uint64_t &interfaceHandle);
 * @tc.desc: int32_t ReleaseInterface(uint64_t interfaceHandle);
 * @tc.desc: int32_t SelectInterfaceSetting(uint64_t interfaceHandle, uint8_t settingIndex);
 * @tc.desc: int32_t GetCurrentInterfaceSetting(uint64_t interfaceHandle, uint8_t &settingIndex);
 * @tc.desc: Positive test: claiming an interface and switching its alternate setting works
 * @tc.desc: Negative test: an invalid alternate setting, an unknown handle or unknown device is rejected
 * @tc.type: FUNC
 */
HWTEST_F(DdkStateBasicTest, ClaimAndInterfaceSettingState, TestSize.Level1)
{
    uint64_t handle = 0;
    ASSERT_EQ(HDF_SUCCESS, service_.ClaimInterface(mainDeviceId_, INTERFACE_ID, handle));
    EXPECT_NE(0U, handle);

    uint8_t setting = INVALID_ALT_SETTING;
    EXPECT_EQ(HDF_SUCCESS, service_.GetCurrentInterfaceSetting(handle, setting));
    EXPECT_EQ(0, setting);
    EXPECT_EQ(HDF_SUCCESS, service_.SelectInterfaceSetting(handle, ALT_SETTING_1));
    EXPECT_EQ(HDF_SUCCESS, service_.GetCurrentInterfaceSetting(handle, setting));
    EXPECT_EQ(ALT_SETTING_1, setting);
    /* an alternate setting that does not exist is rejected */
    EXPECT_NE(HDF_SUCCESS, service_.SelectInterfaceSetting(handle, INVALID_ALT_SETTING));
    /* unknown handle cannot be used */
    uint8_t unusedSetting = 0;
    EXPECT_NE(HDF_SUCCESS, service_.GetCurrentInterfaceSetting(handle + INVALID_HANDLE_OFFSET, unusedSetting));
    EXPECT_NE(HDF_SUCCESS, service_.SelectInterfaceSetting(handle + INVALID_HANDLE_OFFSET, 0));

    EXPECT_EQ(HDF_SUCCESS, service_.ReleaseInterface(handle));
    /* the handle is gone after the release */
    EXPECT_NE(HDF_SUCCESS, service_.ReleaseInterface(handle));
    /* claiming an interface of an unknown device fails */
    uint64_t missingHandle = 0;
    EXPECT_NE(HDF_SUCCESS, service_.ClaimInterface(MakeDeviceId(MISSING_DEVICE_ADDR), INTERFACE_ID, missingHandle));
}
} // namespace

int main(int argc, char **argv)
{
    bool dumpTransfers = false;
    std::vector<char *> filtered;
    filtered.reserve(static_cast<size_t>(argc) + 1);
    filtered.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-transfers") == 0) {
            dumpTransfers = true;
            continue;
        }
        filtered.push_back(argv[i]);
    }
    filtered.push_back(nullptr);
    mock_usb::SetDumpEnabled(dumpTransfers);
    int filteredArgc = static_cast<int>(filtered.size() - 1);
    testing::InitGoogleTest(&filteredArgc, filtered.data());
    int result = RUN_ALL_TESTS();
    if (mock_usb::DumpEnabled()) {
        mock_usb::Dump(stdout);
    }
    return result;
}
