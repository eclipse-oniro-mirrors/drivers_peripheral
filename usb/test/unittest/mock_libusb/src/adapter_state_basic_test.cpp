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
 * Basic mock-libusb tests for LibusbAdapter: hotplug and device state management.
 *
 * Scope (intentionally small for now):
 *   - device presence / enumeration (hotplug arrives and leaves)
 *   - device state: open/close, claim/release, configuration, interface setting,
 *     interface management, reset and clear-halt
 *   - hotplug notifications delivered to a subscriber
 * Transfer paths, descriptor parsing, fault injection and node/mmap based APIs are
 * parked under test/unittest/mock_libusb/pending/ and can be re-enabled later.
 *
 * Command line:
 *   --dump-transfers   print every transfer handled by the mock libusb
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "hdf_base.h"
#include "hdf_log.h"
#include "libusb_adapter.h"
#include "mock_libusb_control.h"
#include "usbd_type.h"
#include "v2_0/iusbd_subscriber.h"

using namespace OHOS::HDI::Usb::V1_2;
using namespace testing::ext;

/* Named constants of the mock device model used by these tests. */
#define BUS_NUM 1
#define ADDR_MAIN 2
#define ADDR_SPARE 3
#define ADDR_SECOND 4
#define ADDR_MISSING 9
#define MAIN_VID 0x1209
#define MAIN_PID 0x0001
#define SPARE_PID 0x0002
#define SECOND_PID 0x0003
#define INTERFACE_ID 0
#define ALT_SETTING_1 1
#define INVALID_ALT_SETTING 0xFF
#define INVALID_INTERFACE 0xC8
#define BULK_IN_EP 0x81
/* MakeDeviceId() packs the bus number into the high bits of the device id. */
#define DEVICE_ID_BUS_SHIFT 32
/* Deterministic wait parameters used by the hotplug case. */
#define WAIT_DEFAULT_TIMEOUT_MS 3000
#define WAIT_POLL_INTERVAL_MS 20
/* Configuration values exercised by ConfigurationStateTransitions. */
#define SECOND_CONFIG_VALUE 2
#define UNKNOWN_CONFIG_VALUE 9

namespace {

UsbDev MakeDev(uint8_t address)
{
    return UsbDev{BUS_NUM, address};
}

UsbPipe MakePipe(uint8_t interfaceId, uint8_t endpoint)
{
    return UsbPipe{interfaceId, endpoint};
}

uint64_t MakeDeviceId(uint8_t address)
{
    return (static_cast<uint64_t>(BUS_NUM) << DEVICE_ID_BUS_SHIFT) + address;
}

bool ContainsDevice(const std::vector<DeviceInfo> &devices, uint8_t address)
{
    for (const auto &device : devices) {
        if (device.deviceId == MakeDeviceId(address)) {
            return true;
        }
    }
    return false;
}

bool WaitFor(const std::function<bool()> &condition, int32_t timeoutMs = WAIT_DEFAULT_TIMEOUT_MS)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_POLL_INTERVAL_MS));
    }
    return condition();
}

class MockSubscriber : public OHOS::HDI::Usb::V2_0::IUsbdSubscriber {
public:
    int32_t DeviceEvent(const OHOS::HDI::Usb::V2_0::USBDeviceInfo &info) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        count_++;
        lastStatus_ = info.status;
        lastBusNum_ = info.busNum;
        lastDevNum_ = info.devNum;
        return HDF_SUCCESS;
    }

    int32_t PortChangedEvent(const OHOS::HDI::Usb::V2_0::PortInfo &info) override
    {
        (void)info;
        return HDF_SUCCESS;
    }

    int32_t Count()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return count_;
    }

    int32_t LastStatus()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return lastStatus_;
    }

    int32_t LastBusNum()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return lastBusNum_;
    }

    int32_t LastDevNum()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return lastDevNum_;
    }

private:
    std::mutex lock_;
    int32_t count_ = 0;
    int32_t lastStatus_ = -1;
    int32_t lastBusNum_ = -1;
    int32_t lastDevNum_ = -1;
};

class AdapterStateTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterStateTest::adapter_ = nullptr;

void AdapterStateTest::SetUpTestCase()
{
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_SPARE, MAIN_VID, SPARE_PID));
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_SECOND, MAIN_VID, SECOND_PID));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
}

void AdapterStateTest::TearDownTestCase()
{
    adapter_ = nullptr;
}

/**
 * @tc.name: DeviceEnumerationReflectsPresence
 * @tc.desc: Test functions to GetDevices (the device list follows the hotplug state)
 * @tc.desc: int32_t GetDevices(std::vector<struct DeviceInfo> &devices);
 * @tc.desc: Positive test: an arrived device is enumerated again after re-plug
 * @tc.desc: Negative test: a device that left is no longer listed
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, DeviceEnumerationReflectsPresence, TestSize.Level1)
{
    std::vector<DeviceInfo> devices;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(devices));
    EXPECT_TRUE(ContainsDevice(devices, ADDR_MAIN));
    EXPECT_TRUE(ContainsDevice(devices, ADDR_SECOND));

    /* unplug: the device table is updated even when no subscriber is bound yet */
    int32_t ret = mock_usb::RaiseHotplug(BUS_NUM, ADDR_SECOND, false);
    EXPECT_TRUE(ret == 0 || ret == -1);
    devices.clear();
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(devices));
    EXPECT_FALSE(ContainsDevice(devices, ADDR_SECOND));
    EXPECT_TRUE(ContainsDevice(devices, ADDR_MAIN));

    /* plug in again */
    ret = mock_usb::RaiseHotplug(BUS_NUM, ADDR_SECOND, true);
    EXPECT_TRUE(ret == 0 || ret == -1);
    devices.clear();
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(devices));
    EXPECT_TRUE(ContainsDevice(devices, ADDR_SECOND));
}

/**
 * @tc.name: OpenAndCloseDeviceState
 * @tc.desc: Test functions to OpenDevice / CloseDevice (reference counted device state)
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev, bool isDetach = false);
 * @tc.desc: Positive test: nested open bumps the reference count, each close releases one
 * @tc.desc: Negative test: opening a missing device or closing a released device fails
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, OpenAndCloseDeviceState, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    EXPECT_NE(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MISSING)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    /* nested open only bumps the reference count */
    EXPECT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
    /* the device is closed now */
    EXPECT_NE(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: ClaimAndReleaseInterfaceState
 * @tc.desc: Test functions to ClaimInterface / ReleaseInterface
 * @tc.desc: int32_t ClaimInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t force);
 * @tc.desc: int32_t ReleaseInterface(const UsbDev &dev, uint8_t interfaceId);
 * @tc.desc: Positive test: an interface can be claimed, released and claimed again
 * @tc.desc: Negative test: an interface id out of range is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, ClaimAndReleaseInterfaceState, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    /* interface id out of range is rejected before touching the stack */
    EXPECT_NE(HDF_SUCCESS, adapter_->ClaimInterface(dev, INVALID_INTERFACE, 1));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(dev, INTERFACE_ID, 1));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ReleaseInterface(dev, INTERFACE_ID));
    /* release is idempotent in the current implementation, and the interface can be
     * claimed again afterwards */
    (void)adapter_->ReleaseInterface(dev, INTERFACE_ID);
    EXPECT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(dev, INTERFACE_ID, 1));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ReleaseInterface(dev, INTERFACE_ID));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: ConfigurationStateTransitions
 * @tc.desc: Test functions to GetConfig / SetConfig
 * @tc.desc: int32_t GetConfig(const UsbDev &dev, uint8_t &configIndex);
 * @tc.desc: int32_t SetConfig(const UsbDev &dev, uint8_t configIndex);
 * @tc.desc: Positive test: the active configuration is switched and read back
 * @tc.desc: Negative test: an unknown configuration index is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, ConfigurationStateTransitions, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    uint8_t config = 0;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetConfig(dev, config));
    EXPECT_EQ(1, config);
    EXPECT_EQ(HDF_SUCCESS, adapter_->SetConfig(dev, SECOND_CONFIG_VALUE));
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetConfig(dev, config));
    EXPECT_EQ(SECOND_CONFIG_VALUE, config);
    /* setting the active configuration again is a no-op */
    EXPECT_EQ(HDF_SUCCESS, adapter_->SetConfig(dev, config));
    /* unknown configuration value */
    EXPECT_NE(HDF_SUCCESS, adapter_->SetConfig(dev, UNKNOWN_CONFIG_VALUE));
    EXPECT_EQ(HDF_SUCCESS, adapter_->SetConfig(dev, 1));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: InterfaceSettingStateTransitions
 * @tc.desc: Test functions to GetCurrentInterfaceSetting / SetInterface
 * @tc.desc: int32_t GetCurrentInterfaceSetting(const UsbDev &dev, uint8_t &settingIndex);
 * @tc.desc: int32_t SetInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t altIndex);
 * @tc.desc: Positive test: an alternate setting is selected and read back
 * @tc.desc: Negative test: an alternate setting that does not exist is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, InterfaceSettingStateTransitions, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(dev, INTERFACE_ID, 1));
    uint8_t setting = INVALID_ALT_SETTING;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetCurrentInterfaceSetting(dev, setting));
    EXPECT_EQ(0, setting);
    EXPECT_EQ(HDF_SUCCESS, adapter_->SetInterface(dev, INTERFACE_ID, ALT_SETTING_1));
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetCurrentInterfaceSetting(dev, setting));
    EXPECT_EQ(ALT_SETTING_1, setting);
    /* alt setting that does not exist in the active configuration */
    EXPECT_NE(HDF_SUCCESS, adapter_->SetInterface(dev, INTERFACE_ID, INVALID_ALT_SETTING));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ReleaseInterface(dev, INTERFACE_ID));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: InterfaceManagementAndActiveStatus
 * @tc.desc: Test functions to ManageInterface / GetInterfaceActiveStatus
 * @tc.desc: int32_t ManageInterface(const UsbDev &dev, uint8_t interfaceId, bool disable);
 * @tc.desc: int32_t GetInterfaceActiveStatus(const UsbDev &dev, uint8_t interfaceId, bool &unactivated);
 * @tc.desc: Positive test: the interface can be disabled and enabled again
 * @tc.desc: Negative test: an interface id above USB_MAX_INTERFACES is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, InterfaceManagementAndActiveStatus, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(dev, INTERFACE_ID, 1));
    bool unactivated = false;
    EXPECT_EQ(HDF_SUCCESS, adapter_->GetInterfaceActiveStatus(dev, INTERFACE_ID, unactivated));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ManageInterface(dev, INTERFACE_ID, true));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ManageInterface(dev, INTERFACE_ID, false));
    /* interface id above USB_MAX_INTERFACES is rejected */
    EXPECT_NE(HDF_SUCCESS, adapter_->ManageInterface(dev, INVALID_INTERFACE, false));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ReleaseInterface(dev, INTERFACE_ID));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: ResetAndClearHaltState
 * @tc.desc: Test functions to ResetDevice / ClearHalt
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc: int32_t ClearHalt(const UsbDev &dev, const UsbPipe &pipe);
 * @tc.desc: Positive test: the device is reset and the halt of a valid pipe is cleared
 * @tc.desc: Negative test: clearing the halt of an invalid interface fails
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, ResetAndClearHaltState, TestSize.Level1)
{
    UsbDev dev = MakeDev(ADDR_MAIN);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(dev));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ResetDevice(dev));
    EXPECT_EQ(HDF_SUCCESS, adapter_->ClearHalt(dev, MakePipe(INTERFACE_ID, BULK_IN_EP)));
    EXPECT_NE(HDF_SUCCESS, adapter_->ClearHalt(dev, MakePipe(INVALID_INTERFACE, BULK_IN_EP)));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(dev));
}

/**
 * @tc.name: HotplugArriveAndLeaveEvents
 * @tc.desc: Test functions to SetSubscriber / RemoveSubscriber (hotplug notification)
 * @tc.desc: int32_t SetSubscriber(sptr<V2_0::IUsbdSubscriber> subscriber);
 * @tc.desc: int32_t RemoveSubscriber(sptr<V2_0::IUsbdSubscriber> subscriber);
 * @tc.desc: Positive test: a bound subscriber receives ACT_DEVUP / ACT_DEVDOWN with busNum and devNum
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, HotplugArriveAndLeaveEvents, TestSize.Level1)
{
    sptr<MockSubscriber> subscriber = new (std::nothrow) MockSubscriber();
    ASSERT_NE(nullptr, subscriber);
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetSubscriber(subscriber));
    /* binding a subscriber first reports the devices that are already present; wait
     * until all of them have been delivered so that the baseline is deterministic */
    std::vector<DeviceInfo> present;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(present));
    ASSERT_NE(0U, present.size());
    EXPECT_TRUE(WaitFor([&subscriber, &present]() {
        return subscriber->Count() >= static_cast<int32_t>(present.size());
    }));
    int32_t baseline = subscriber->Count();

    /* unplug */
    ASSERT_EQ(0, mock_usb::RaiseHotplug(BUS_NUM, ADDR_SPARE, false));
    EXPECT_TRUE(WaitFor([&subscriber, baseline]() { return subscriber->Count() > baseline; }));
    EXPECT_EQ(ACT_DEVDOWN, subscriber->LastStatus());
    EXPECT_EQ(BUS_NUM, subscriber->LastBusNum());
    EXPECT_EQ(ADDR_SPARE, subscriber->LastDevNum());

    /* plug in */
    ASSERT_EQ(0, mock_usb::RaiseHotplug(BUS_NUM, ADDR_SPARE, true));
    EXPECT_TRUE(WaitFor([&subscriber, baseline]() { return subscriber->Count() > baseline + 1; }));
    EXPECT_EQ(ACT_DEVUP, subscriber->LastStatus());
    EXPECT_EQ(ADDR_SPARE, subscriber->LastDevNum());

    EXPECT_EQ(HDF_SUCCESS, adapter_->RemoveSubscriber(subscriber));
}

/**
 * @tc.name: DevicePresenceWithoutEventIsHarmless
 * @tc.desc: Test functions to GetDevices (device table update without a bound subscriber)
 * @tc.desc: int32_t GetDevices(std::vector<struct DeviceInfo> &devices);
 * @tc.desc: Positive test: the device table can be updated directly
 * @tc.desc: Negative test: raising a hotplug event without a registered callback does not crash
 * @tc.type: FUNC
 */
HWTEST_F(AdapterStateTest, DevicePresenceWithoutEventIsHarmless, TestSize.Level1)
{
    ASSERT_EQ(0, mock_usb::SetDevicePresent(BUS_NUM, ADDR_SECOND, false));
    std::vector<DeviceInfo> devices;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(devices));
    EXPECT_FALSE(ContainsDevice(devices, ADDR_SECOND));

    int32_t ret = mock_usb::RaiseHotplug(BUS_NUM, ADDR_SECOND, true);
    EXPECT_TRUE(ret == 0 || ret == -1); // 0 with a callback, -1 without one
    ASSERT_EQ(0, mock_usb::SetDevicePresent(BUS_NUM, ADDR_SECOND, true));
    devices.clear();
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetDevices(devices));
    EXPECT_TRUE(ContainsDevice(devices, ADDR_SECOND));
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
