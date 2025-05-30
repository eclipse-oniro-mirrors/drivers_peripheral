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

#include "usbd_device_test.h"

#include <iostream>
#include <vector>

#include "UsbSubTest.h"
#include "hdf_log.h"
#include "usbd_wrapper.h"
#include "v2_0/iusb_host_interface.h"
#include "v2_0/iusb_port_interface.h"

using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::USB;
using namespace std;
using namespace OHOS::HDI::Usb::V2_0;

const int SLEEP_TIME = 3;
const uint8_t BUS_NUM_INVALID = 255;
const uint8_t DEV_ADDR_INVALID = 255;
UsbDev UsbdDeviceTest::dev_ = {0, 0};
sptr<UsbSubTest> UsbdDeviceTest::subscriber_ = nullptr;

namespace {
sptr<OHOS::HDI::Usb::V2_0::IUsbHostInterface> g_usbHostInterface = nullptr;
sptr<OHOS::HDI::Usb::V2_0::IUsbPortInterface> g_usbPortInterface = nullptr;

int32_t SwitchErrCode(int32_t ret)
{
    return ret == HDF_ERR_NOT_SUPPORT ? HDF_SUCCESS : ret;
}

void UsbdDeviceTest::SetUpTestCase(void)
{
    g_usbHostInterface = OHOS::HDI::Usb::V2_0::IUsbHostInterface::Get(true);
    g_usbPortInterface = OHOS::HDI::Usb::V2_0::IUsbPortInterface::Get();
    if (g_usbPortInterface == nullptr || g_usbHostInterface == nullptr) {
        HDF_LOGE("%{public}s:IUsbInterface::Get() failed.", __func__);
        exit(0);
    }
    auto ret = g_usbPortInterface->SetPortRole(1, 1, 1);
    sleep(SLEEP_TIME);
    HDF_LOGI("UsbdDeviceTest::[Device] %{public}d SetPortRole=%{public}d", __LINE__, ret);
    ret = SwitchErrCode(ret);
    ASSERT_EQ(0, ret);
    if (ret != 0) {
        exit(0);
    }

    subscriber_ = new UsbSubTest();
    if (g_usbHostInterface->BindUsbdHostSubscriber(subscriber_) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: bind usbd subscriber_ failed", __func__);
        exit(0);
    }
    std::cout << "please connect device, press enter to continue" << std::endl;
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
    printf("busNum = %d, devAddr = %d \n", subscriber_->busNum_, subscriber_->devAddr_);
    dev_ = {subscriber_->busNum_, subscriber_->devAddr_};
}

void UsbdDeviceTest::TearDownTestCase(void)
{
    g_usbHostInterface->UnbindUsbdHostSubscriber(subscriber_);
}

void UsbdDeviceTest::SetUp(void) {}

void UsbdDeviceTest::TearDown(void) {}

/**
 * @tc.name: UsbdDevice001
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: 正向测试：参数正确
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdOpenDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result =%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: UsbdDevice002
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdOpenDevice002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, dev_.devAddr};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: UsbdDevice003
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdOpenDevice003, TestSize.Level1)
{
    struct UsbDev dev = {dev_.busNum, DEV_ADDR_INVALID};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: UsbdDevice004
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdOpenDevice004, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**********************************************************************************************************/

/**
 * @tc.name: UsbdDevice011
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev);
 * @tc.desc: 正向测试：参数正确
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdCloseDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: UsbdDevice012
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdCloseDevice002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.busNum = BUS_NUM_INVALID;
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.name: UsbdDevice013
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdCloseDevice003, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.devAddr = DEV_ADDR_INVALID;
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.name: UsbdDevice014
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdCloseDevice004, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.busNum = BUS_NUM_INVALID;
    dev.devAddr = DEV_ADDR_INVALID;
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.name: UsbdResetDevice001
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc: 正向测试：参数正确
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdResetDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_usbHostInterface->ResetDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: UsbdResetDevice002
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdResetDevice002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.busNum = BUS_NUM_INVALID;
    ret = g_usbHostInterface->ResetDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(0, ret);
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.name: UsbdResetDevice003
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdResetDevice003, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.devAddr = DEV_ADDR_INVALID;
    ret = g_usbHostInterface->ResetDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(0, ret);
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.name: UsbdResetDevice004
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc:
 * @tc.type: FUNC
 */
HWTEST_F(UsbdDeviceTest, UsbdResetDevice004, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    dev.busNum = BUS_NUM_INVALID;
    dev.devAddr = DEV_ADDR_INVALID;
    ret = g_usbHostInterface->ResetDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(0, ret);
    ret = g_usbHostInterface->CloseDevice(dev);
    HDF_LOGI("UsbdDeviceTest:: Line:%{public}d Close result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    dev = dev_;
    g_usbHostInterface->CloseDevice(dev);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0100
 * @tc.name: testHdiUsbDeviceTestOpenDevice001
 * @tc.desc: Opens a USB device to set up a connection. dev ={1, 255}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestOpenDevice001, Function | MediumTest | Level2)
{
    struct UsbDev dev = {1, 255};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0200
 * @tc.name: testHdiUsbDeviceTestOpenDevice002
 * @tc.desc: Opens a USB device to set up a connection. dev ={255, 1}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestOpenDevice002, Function | MediumTest | Level2)
{
    struct UsbDev dev = {255, 1};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0300
 * @tc.name: testHdiUsbDeviceTestOpenDevice003
 * @tc.desc: Opens a USB device to set up a connection. dev ={255, 100}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestOpenDevice003, Function | MediumTest | Level2)
{
    struct UsbDev dev = {255, 100};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0400
 * @tc.name: testHdiUsbDeviceTestOpenDevice004
 * @tc.desc: Opens a USB device to set up a connection. dev ={100, 255}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestOpenDevice004, Function | MediumTest | Level2)
{
    struct UsbDev dev = {100, 255};
    auto ret = g_usbHostInterface->OpenDevice(dev);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0500
 * @tc.name: testHdiUsbDeviceTestCloseDevice001
 * @tc.desc: Closes a USB device to release all system resources related to the device. dev ={1, 255}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestCloseDevice001, Function | MediumTest | Level2)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    dev = {1, 255};
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_NE(ret, 0);
    dev = dev_;
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_EQ(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0600
 * @tc.name: testHdiUsbDeviceTestCloseDevice002
 * @tc.desc: Closes a USB device to release all system resources related to the device. dev ={255, 1}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestCloseDevice002, Function | MediumTest | Level2)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    dev = {255, 1};
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_NE(ret, 0);
    dev = dev_;
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_EQ(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0700
 * @tc.name: testHdiUsbDeviceTestCloseDevice003
 * @tc.desc: Closes a USB device to release all system resources related to the device. dev ={255, 100}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestCloseDevice003, Function | MediumTest | Level2)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    dev = {255, 100};
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_NE(ret, 0);
    dev = dev_;
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_EQ(ret, 0);
}

/**
 * @tc.number: SUB_USB_DeviceManager_HDI_CompatDeviceTest_0800
 * @tc.name: testHdiUsbDeviceTestCloseDevice004
 * @tc.desc: Closes a USB device to release all system resources related to the device. dev ={100, 255}.
 */
HWTEST_F(UsbdDeviceTest, testHdiUsbDeviceTestCloseDevice004, Function | MediumTest | Level2)
{
    struct UsbDev dev = dev_;
    auto ret = g_usbHostInterface->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    dev = {100, 255};
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_NE(ret, 0);
    dev = dev_;
    ret = g_usbHostInterface->CloseDevice(dev);
    EXPECT_EQ(ret, 0);
}
} // namespace
