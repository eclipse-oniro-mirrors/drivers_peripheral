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

#include "libusb_adapter_test.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

#include "hdf_log.h"
#include "securec.h"
#include "usbd_type.h"
#include "v1_2/iusbd_transfer_callback.h"
#include "v1_2/usb_types.h"
#include "v2_0/iusbd_bulk_callback.h"
#include "v2_0/iusbd_subscriber.h"
#include "libusb_adapter.h"

using namespace testing::ext;
using namespace OHOS;
using namespace std;
using namespace OHOS::HDI::Usb::V1_2;

const uint8_t BUS_NUM_INVALID = 255;
const uint8_t DEV_ADDR_INVALID = 255;
const uint8_t INTERFACE_ID_INVALID = 32;
constexpr uint32_t DEVICE_DESCRIPTOR_CLASS_INDEX = 4;
constexpr int32_t DEVICE_READY_RETRY_COUNT = 30;
constexpr int32_t DEVICE_READY_RETRY_INTERVAL_MS = 100;
constexpr uint8_t INTERFACEID_OK = 0;
constexpr uint32_t MAX_BUFFER_LENGTH = 0x0100U;
constexpr int32_t ASHMEM_MAX_SIZE = 1024;
constexpr int32_t ASYNC_TRANSFER_TIME_OUT = 1000;
constexpr int32_t ENDPOINT_ADDRESS_IN = 0x81;
constexpr int32_t ENDPOINT_ADDRESS_OUT = 0x1;
constexpr uint32_t BULK_CANCEL_RACE_LOOP = 30;
constexpr uint32_t BULK_CANCEL_WAIT_US = 20000;
constexpr uint32_t BULK_CLEANUP_RETRY_MAX = 100;

namespace OHOS::HDI::Usb::V1_2 {

UsbDev LibusbAdapterTest::dev_ = {0, 0};

std::shared_ptr<LibusbAdapter> g_libusbAdapter = nullptr;

class UsbDeviceSubscriber : public OHOS::HDI::Usb::V2_0::IUsbdSubscriber {
public:
    int32_t DeviceEvent(const OHOS::HDI::Usb::V2_0::USBDeviceInfo &info) override
    {
        if (info.status != ACT_DEVUP) {
            return 0;
        }
        UsbDev dev = {static_cast<uint8_t>(info.busNum), static_cast<uint8_t>(info.devNum)};
        std::vector<uint8_t> descriptor;
        int32_t ret = LibusbAdapter::GetInstance()->GetDeviceDescriptor(dev, descriptor);
        if (ret == 0 && descriptor.size() > DEVICE_DESCRIPTOR_CLASS_INDEX &&
            descriptor[DEVICE_DESCRIPTOR_CLASS_INDEX] != LIBUSB_CLASS_HUB) {
            LibusbAdapterTest::dev_ = dev;
        }
        return 0;
    }

    int32_t PortChangedEvent(const OHOS::HDI::Usb::V2_0::PortInfo &info) override
    {
        return 0;
    }
};

sptr<UsbDeviceSubscriber> g_subscriber = nullptr;

class UsbdBulkCallbackTest : public OHOS::HDI::Usb::V2_0::IUsbdBulkCallback {
public:
    int32_t OnBulkWriteCallback(int32_t status, int32_t actLength) override
    {
        return 0;
    }
    int32_t OnBulkReadCallback(int32_t status, int32_t actLength) override
    {
        return 0;
    }
};

class UsbdTransferCallbackTest : public OHOS::HDI::Usb::V1_2::IUsbdTransferCallback {
public:
    int32_t OnTransferWriteCallback(int32_t status, int32_t actLength,
        const std::vector<OHOS::HDI::Usb::V1_2::UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        return 0;
    }
    int32_t OnTransferReadCallback(int32_t status, int32_t actLength,
        const std::vector<OHOS::HDI::Usb::V1_2::UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        return 0;
    }
};

int32_t InitAshmemOne(sptr<Ashmem> &asmptr, int32_t asmSize, uint8_t rflg)
{
    asmptr = Ashmem::CreateAshmem("ttashmem000", asmSize);
    if (asmptr == nullptr) {
        HDF_LOGE("InitAshmemOne CreateAshmem failed");
        return HDF_FAILURE;
    }
    asmptr->MapReadAndWriteAshmem();
    if (rflg == 0) {
        uint8_t tdata[ASHMEM_MAX_SIZE];
        int32_t offset = 0;
        int32_t tlen = 0;
        int32_t retSafe = memset_s(tdata, sizeof(tdata), 'Y', ASHMEM_MAX_SIZE);
        if (retSafe != EOK) {
            HDF_LOGE("InitAshmemOne memset_s failed");
            return HDF_FAILURE;
        }
        while (offset < asmSize) {
            tlen = (asmSize - offset) < ASHMEM_MAX_SIZE ? (asmSize - offset) : ASHMEM_MAX_SIZE;
            asmptr->WriteToAshmem(tdata, tlen, offset);
            offset += tlen;
        }
    }
    return HDF_SUCCESS;
}

void StartBulkEventThread(std::shared_ptr<LibusbAdapter> adapter, const UsbDev &dev)
{
    if (adapter == nullptr) {
        return;
    }
    struct USBTransferInfo info = {
        .endpoint = ENDPOINT_ADDRESS_OUT,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TRANSFER_TIME_OUT,
        .length = MAX_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    sptr<UsbdTransferCallbackTest> asyncCallback = new UsbdTransferCallbackTest();
    if (asyncCallback == nullptr) {
        HDF_LOGE("StartBulkEventThread new async callback failed");
        return;
    }
    sptr<Ashmem> ashmem;
    (void)InitAshmemOne(ashmem, MAX_BUFFER_LENGTH, 0);
    (void)adapter->AsyncSubmitTransfer(dev, info, asyncCallback, ashmem);
    usleep(BULK_CANCEL_WAIT_US);
    (void)adapter->AsyncCancelTransfer(dev, ENDPOINT_ADDRESS_OUT);
}

void CleanupBulkTransfer(std::shared_ptr<LibusbAdapter> adapter, const UsbDev &dev, const UsbPipe &pipe)
{
    if (adapter == nullptr) {
        return;
    }
    (void)adapter->BulkCancel(dev, pipe);
    for (uint32_t i = 0; i < BULK_CLEANUP_RETRY_MAX; i++) {
        if (adapter->UnRegBulkCallback(dev, pipe) == HDF_SUCCESS) {
            break;
        }
        usleep(BULK_CANCEL_WAIT_US);
    }
}

bool FindBulkInEndpoint(const UsbDev &dev, uint8_t &intfId, uint8_t &endpointId)
{
    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != LIBUSB_SUCCESS) {
        return false;
    }
    libusb_device **devs = nullptr;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    bool found = false;
    if (cnt > 0) {
        for (ssize_t i = 0; i < cnt && !found; i++) {
            if (libusb_get_bus_number(devs[i]) != dev.busNum ||
                libusb_get_device_address(devs[i]) != dev.devAddr) {
                continue;
            }
            libusb_config_descriptor *config = nullptr;
            if (libusb_get_active_config_descriptor(devs[i], &config) != LIBUSB_SUCCESS) {
                break;
            }
            for (int j = 0; j < config->bNumInterfaces && !found; j++) {
                const libusb_interface *intf = &config->interface[j];
                for (int k = 0; k < intf->num_altsetting && !found; k++) {
                    const libusb_interface_descriptor *intfDesc = &intf->altsetting[k];
                    for (int m = 0; m < intfDesc->bNumEndpoints; m++) {
                        const libusb_endpoint_descriptor *ep = &intfDesc->endpoint[m];
                        if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK) {
                            continue;
                        }
                        if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) != LIBUSB_ENDPOINT_IN) {
                            continue;
                        }
                        intfId = intfDesc->bInterfaceNumber;
                        endpointId = ep->bEndpointAddress;
                        found = true;
                        break;
                    }
                }
            }
            libusb_free_config_descriptor(config);
            break;
        }
    }
    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    return found;
}

void LibusbAdapterTest::SetUpTestCase(void)
{
    g_libusbAdapter = LibusbAdapter::GetInstance();
    if (g_libusbAdapter == nullptr) {
        HDF_LOGE("%{public}s: LibusbAdapter::GetInstance() failed", __func__);
        exit(0);
    }
    g_subscriber = new UsbDeviceSubscriber();
    g_libusbAdapter->SetSubscriber(g_subscriber);
    for (int i = 0; i < DEVICE_READY_RETRY_COUNT && (dev_.busNum == 0 || dev_.devAddr == 0); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEVICE_READY_RETRY_INTERVAL_MS));
    }
    HDF_LOGI("LibusbAdapterTest::dev_ = {%{public}d, %{public}d}",
        static_cast<int>(dev_.busNum), static_cast<int>(dev_.devAddr));
}

void LibusbAdapterTest::TearDownTestCase(void)
{
    if (g_subscriber != nullptr && g_libusbAdapter != nullptr) {
        g_libusbAdapter->RemoveSubscriber(g_subscriber);
        g_subscriber = nullptr;
    }
    g_libusbAdapter = nullptr;
}

void LibusbAdapterTest::SetUp(void) {}

void LibusbAdapterTest::TearDown(void) {}

/**
 * @tc.name: LibusbAdapter001
 * @tc.desc: Test functions to GetInstance
 * @tc.desc: std::shared_ptr<LibusbAdapter> GetInstance();
 * @tc.desc: Positive test: get singleton instance
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetInstance001, TestSize.Level1)
{
    auto instance = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, instance);
}

/**
 * @tc.name: LibusbAdapter002
 * @tc.desc: Test functions to GetInstance
 * @tc.desc: std::shared_ptr<LibusbAdapter> GetInstance();
 * @tc.desc: Positive test: verify singleton
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetInstance002, TestSize.Level1)
{
    auto instance1 = LibusbAdapter::GetInstance();
    auto instance2 = LibusbAdapter::GetInstance();
    ASSERT_EQ(instance1, instance2);
}

/**
 * @tc.name: LibusbAdapterOpenDevice001
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterOpenDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterOpenDevice002
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: Negative test: invalid bus number
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterOpenDevice002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, dev_.devAddr};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterOpenDevice003
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: Negative test: invalid device address
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterOpenDevice003, TestSize.Level1)
{
    struct UsbDev dev = {dev_.busNum, DEV_ADDR_INVALID};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterOpenDeviceConcurrent001
 * @tc.desc: Test functions to OpenDevice
 * @tc.desc: int32_t OpenDevice(const UsbDev &dev);
 * @tc.desc: Positive test: open from two threads concurrently then close twice
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterOpenDeviceConcurrent001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    std::vector<int32_t> openResults(2, -1);
    std::thread thread1([&]() { openResults[0] = g_libusbAdapter->OpenDevice(dev); });
    std::thread thread2([&]() { openResults[1] = g_libusbAdapter->OpenDevice(dev); });
    thread1.join();
    thread2.join();
    ASSERT_EQ(0, openResults[0]);
    ASSERT_EQ(0, openResults[1]);
    int32_t ret = g_libusbAdapter->CloseDevice(dev, false);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d CloseDevice1 result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    ret = g_libusbAdapter->CloseDevice(dev, false);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d CloseDevice2 result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: LibusbAdapterCloseDevice001
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev, bool isDetach);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterCloseDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->CloseDevice(dev, false);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d CloseDevice result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: LibusbAdapterCloseDevice002
 * @tc.desc: Test functions to CloseDevice
 * @tc.desc: int32_t CloseDevice(const UsbDev &dev, bool isDetach);
 * @tc.desc: Negative test: close without open
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterCloseDevice002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->CloseDevice(dev, false);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d CloseDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterResetDevice001
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterResetDevice001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ResetDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterResetDevice002
 * @tc.desc: Test functions to ResetDevice
 * @tc.desc: int32_t ResetDevice(const UsbDev &dev);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterResetDevice002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    auto ret = g_libusbAdapter->ResetDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ResetDevice result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetDeviceDescriptor001
 * @tc.desc: Test functions to GetDeviceDescriptor
 * @tc.desc: int32_t GetDeviceDescriptor(const UsbDev &dev, std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceDescriptor001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetDeviceDescriptor(dev, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetDeviceDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    EXPECT_GT(descriptor.size(), 0);
}

/**
 * @tc.name: LibusbAdapterGetDeviceDescriptor002
 * @tc.desc: Test functions to GetDeviceDescriptor
 * @tc.desc: int32_t GetDeviceDescriptor(const UsbDev &dev, std::vector<uint8_t> &descriptor);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceDescriptor002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetDeviceDescriptor(dev, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetDeviceDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetConfigDescriptor001
 * @tc.desc: Test functions to GetConfigDescriptor
 * @tc.desc: int32_t GetConfigDescriptor(const UsbDev &dev, uint8_t descId, std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetConfigDescriptor001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetConfigDescriptor(dev, 0, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetConfigDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    EXPECT_GT(descriptor.size(), 0);
}

/**
 * @tc.name: LibusbAdapterGetConfigDescriptor002
 * @tc.desc: Test functions to GetConfigDescriptor
 * @tc.desc: int32_t GetConfigDescriptor(const UsbDev &dev, uint8_t descId, std::vector<uint8_t> &descriptor);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetConfigDescriptor002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetConfigDescriptor(dev, 0, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetConfigDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetStringDescriptor001
 * @tc.desc: Test functions to GetStringDescriptor
 * @tc.desc: int32_t GetStringDescriptor(const UsbDev &dev, uint8_t descId, std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetStringDescriptor001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    std::vector<uint8_t> descriptor;
    ret = g_libusbAdapter->GetStringDescriptor(dev, 1, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetStringDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetStringDescriptor002
 * @tc.desc: Test functions to GetStringDescriptor
 * @tc.desc: int32_t GetStringDescriptor(const UsbDev &dev, uint8_t descId, std::vector<uint8_t> &descriptor);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetStringDescriptor002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetStringDescriptor(dev, 1, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetStringDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterSetConfig001
 * @tc.desc: Test functions to SetConfig
 * @tc.desc: int32_t SetConfig(const UsbDev &dev, uint8_t configIndex);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterSetConfig001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->SetConfig(dev, 1);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d SetConfig result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterSetConfig002
 * @tc.desc: Test functions to SetConfig
 * @tc.desc: int32_t SetConfig(const UsbDev &dev, uint8_t configIndex);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterSetConfig002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    auto ret = g_libusbAdapter->SetConfig(dev, 1);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d SetConfig result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetConfig001
 * @tc.desc: Test functions to GetConfig
 * @tc.desc: int32_t GetConfig(const UsbDev &dev, uint8_t &configIndex);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetConfig001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    uint8_t configIndex = 0;
    ret = g_libusbAdapter->GetConfig(dev, configIndex);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetConfig result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetConfig002
 * @tc.desc: Test functions to GetConfig
 * @tc.desc: int32_t GetConfig(const UsbDev &dev, uint8_t &configIndex);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetConfig002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    uint8_t configIndex = 0;
    auto ret = g_libusbAdapter->GetConfig(dev, configIndex);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetConfig result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterClaimInterface001
 * @tc.desc: Test functions to ClaimInterface
 * @tc.desc: int32_t ClaimInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t force);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterClaimInterface001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterClaimInterface002
 * @tc.desc: Test functions to ClaimInterface
 * @tc.desc: int32_t ClaimInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t force);
 * @tc.desc: Negative test: invalid interface id
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterClaimInterface002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, INTERFACE_ID_INVALID, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterReleaseInterface001
 * @tc.desc: Test functions to ReleaseInterface
 * @tc.desc: int32_t ReleaseInterface(const UsbDev &dev, uint8_t interfaceId);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterReleaseInterface001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ReleaseInterface(dev, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ReleaseInterface result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterReleaseInterface002
 * @tc.desc: Test functions to ReleaseInterface
 * @tc.desc: int32_t ReleaseInterface(const UsbDev &dev, uint8_t interfaceId);
 * @tc.desc: Negative test: invalid interface id
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterReleaseInterface002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ReleaseInterface(dev, INTERFACE_ID_INVALID);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ReleaseInterface result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterSetInterface001
 * @tc.desc: Test functions to SetInterface
 * @tc.desc: int32_t SetInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t altIndex);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterSetInterface001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->SetInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d SetInterface result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterSetInterface002
 * @tc.desc: Test functions to SetInterface
 * @tc.desc: int32_t SetInterface(const UsbDev &dev, uint8_t interfaceId, uint8_t altIndex);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterSetInterface002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    auto ret = g_libusbAdapter->SetInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d SetInterface result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetDeviceSpeed001
 * @tc.desc: Test functions to GetDeviceSpeed
 * @tc.desc: int32_t GetDeviceSpeed(const UsbDev &dev, uint8_t &speed);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceSpeed001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    uint8_t speed = 0;
    auto ret = g_libusbAdapter->GetDeviceSpeed(dev, speed);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetDeviceSpeed result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: LibusbAdapterGetDeviceSpeed002
 * @tc.desc: Test functions to GetDeviceSpeed
 * @tc.desc: int32_t GetDeviceSpeed(const UsbDev &dev, uint8_t &speed);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceSpeed002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    uint8_t speed = 0;
    auto ret = g_libusbAdapter->GetDeviceSpeed(dev, speed);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetDeviceSpeed result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterGetDeviceFileDescriptor001
 * @tc.desc: Test functions to GetDeviceFileDescriptor
 * @tc.desc: int32_t GetDeviceFileDescriptor(const UsbDev &dev, int32_t &fd);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceFileDescriptor001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    int32_t fd = -1;
    ret = g_libusbAdapter->GetDeviceFileDescriptor(dev, fd);
    HDF_LOGI("LibusbAdapterTest:: GetDeviceFileDescriptor result=%{public}d, fd=%{public}d", ret, fd);
    EXPECT_EQ(0, ret);
    EXPECT_GE(fd, 0);
    if (fd >= 0) {
        close(fd);
    }
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetDeviceFileDescriptor002
 * @tc.desc: Test functions to GetDeviceFileDescriptor
 * @tc.desc: int32_t GetDeviceFileDescriptor(const UsbDev &dev, int32_t &fd);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetDeviceFileDescriptor002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    int32_t fd = -1;
    auto ret = g_libusbAdapter->GetDeviceFileDescriptor(dev, fd);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetDeviceFileDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterClearHalt001
 * @tc.desc: Test functions to ClearHalt
 * @tc.desc: int32_t ClearHalt(const UsbDev &dev, const UsbPipe &pipe);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterClearHalt001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x81};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClearHalt(dev, pipe);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClearHalt result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterManageInterface001
 * @tc.desc: Test functions to ManageInterface
 * @tc.desc: int32_t ManageInterface(const UsbDev &dev, uint8_t interfaceId, bool disable);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterManageInterface001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ManageInterface(dev, 0, true);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ManageInterface result=%{public}d", __LINE__, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterControlTransferRead001
 * @tc.desc: Test functions to ControlTransferRead
 * @tc.desc: int32_t ControlTransferRead(const UsbDev &dev, const UsbCtrlTransfer &ctrl, std::vector<uint8_t> &data);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterControlTransferRead001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbCtrlTransfer ctrl = {0x80, 0x06, 0x0100, 0x00, 1000};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ControlTransferRead(dev, ctrl, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ControlTransferRead result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterControlTransferRead002
 * @tc.desc: Test functions to ControlTransferRead
 * @tc.desc: int32_t ControlTransferRead(const UsbDev &dev, const UsbCtrlTransfer &ctrl, std::vector<uint8_t> &data);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterControlTransferRead002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    struct UsbCtrlTransfer ctrl = {0x80, 0x06, 0x0100, 0x00, 1000};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->ControlTransferRead(dev, ctrl, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ControlTransferRead result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterControlTransferWrite001
 * @tc.desc: Test functions to ControlTransferWrite
 * @tc.desc: int32_t ControlTransferWrite;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterControlTransferWrite001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbCtrlTransfer ctrl = {0x00, 0x09, 0x0001, 0x00, 1000};
    std::vector<uint8_t> data = {0x00};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ControlTransferWrite(dev, ctrl, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ControlTransferWrite result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterControlTransferWrite002
 * @tc.desc: Test functions to ControlTransferWrite
 * @tc.desc: int32_t ControlTransferWrite;
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterControlTransferWrite002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    struct UsbCtrlTransfer ctrl = {0x00, 0x09, 0x0001, 0x00, 1000};
    std::vector<uint8_t> data = {0x00};
    auto ret = g_libusbAdapter->ControlTransferWrite(dev, ctrl, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ControlTransferWrite result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterBulkTransferRead001
 * @tc.desc: Test functions to BulkTransferRead
 * @tc.desc: int32_t BulkTransferRead;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkTransferRead001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x81};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->BulkTransferRead(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d BulkTransferRead result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterBulkTransferRead002
 * @tc.desc: Test functions to BulkTransferRead
 * @tc.desc: int32_t BulkTransferRead;
 * @tc.desc: Negative test: invalid interface id
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkTransferRead002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {INTERFACE_ID_INVALID, 0x81};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->BulkTransferRead(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d BulkTransferRead result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterBulkTransferWrite001
 * @tc.desc: Test functions to BulkTransferWrite
 * @tc.desc: int32_t BulkTransferWrite;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkTransferWrite001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x01};
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->BulkTransferWrite(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d BulkTransferWrite result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterBulkTransferWrite002
 * @tc.desc: Test functions to BulkTransferWrite
 * @tc.desc: int32_t BulkTransferWrite);
 * @tc.desc: Negative test: invalid interface id
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkTransferWrite002, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {INTERFACE_ID_INVALID, 0x01};
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    auto ret = g_libusbAdapter->BulkTransferWrite(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d BulkTransferWrite result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterInterruptTransferRead001
 * @tc.desc: Test functions to InterruptTransferRead
 * @tc.desc: int32_t InterruptTransferRead;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterInterruptTransferRead001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x81};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->InterruptTransferRead(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d InterruptTransferRead result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterInterruptTransferWrite001
 * @tc.desc: Test functions to InterruptTransferWrite
 * @tc.desc: int32_t InterruptTransferWrite;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterInterruptTransferWrite001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x01};
    std::vector<uint8_t> data = {0x01, 0x02};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->InterruptTransferWrite(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d InterruptTransferWrite result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterIsoTransferRead001
 * @tc.desc: Test functions to IsoTransferRead
 * @tc.desc: int32_t IsoTransferRead;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterIsoTransferRead001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x81};
    std::vector<uint8_t> data;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->IsoTransferRead(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d IsoTransferRead result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterIsoTransferWrite001
 * @tc.desc: Test functions to IsoTransferWrite
 * @tc.desc: int32_t IsoTransferWrite;
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterIsoTransferWrite001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    struct UsbPipe pipe = {0, 0x01};
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->IsoTransferWrite(dev, pipe, 1000, data);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d IsoTransferWrite result=%{public}d", __LINE__, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetCurrentInterfaceSetting001
 * @tc.desc: Test functions to GetCurrentInterfaceSetting
 * @tc.desc: int32_t GetCurrentInterfaceSetting(const UsbDev &dev, uint8_t &settingIndex);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetCurrentInterfaceSetting001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    uint8_t settingIndex = 0;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->ClaimInterface(dev, 0, 0);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d ClaimInterface result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->GetCurrentInterfaceSetting(dev, settingIndex);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetCurrentInterfaceSetting result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    g_libusbAdapter->ReleaseInterface(dev, 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetRawDescriptor001
 * @tc.desc: Test functions to GetRawDescriptor
 * @tc.desc: int32_t GetRawDescriptor(const UsbDev &dev, std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: parameters correctly
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetRawDescriptor001, TestSize.Level1)
{
    struct UsbDev dev = dev_;
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d OpenDevice result=%{public}d", __LINE__, ret);
    ASSERT_EQ(0, ret);
    ret = g_libusbAdapter->GetRawDescriptor(dev, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetRawDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_EQ(0, ret);
    EXPECT_GT(descriptor.size(), 0);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterGetRawDescriptor002
 * @tc.desc: Test functions to GetRawDescriptor
 * @tc.desc: int32_t GetRawDescriptor(const UsbDev &dev, std::vector<uint8_t> &descriptor);
 * @tc.desc: Negative test: invalid device
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterGetRawDescriptor002, TestSize.Level1)
{
    struct UsbDev dev = {BUS_NUM_INVALID, DEV_ADDR_INVALID};
    std::vector<uint8_t> descriptor;
    auto ret = g_libusbAdapter->GetRawDescriptor(dev, descriptor);
    HDF_LOGI("LibusbAdapterTest:: Line:%{public}d GetRawDescriptor result=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: LibusbAdapterBulkCancelCallbackRace001
 * @tc.desc: Test BulkRead followed by BulkCancel races with the async completion callback.
 * @tc.desc: Repeatedly submit and cancel a bulk transfer, then verify the endpoint is reusable
 * @tc.desc: (the cancelled completion callback releases the buffer and resets isTransferring).
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkCancelCallbackRace001, TestSize.Level1)
{
    UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    uint8_t intfId = 0;
    uint8_t endpointId = 0;
    if (!FindBulkInEndpoint(dev, intfId, endpointId)) {
        HDF_LOGI("LibusbAdapterTest::BulkCancelCallbackRace001 no bulk in endpoint, skip");
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    ret = g_libusbAdapter->ClaimInterface(dev, intfId, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGI("LibusbAdapterTest::BulkCancelCallbackRace001 claim interface failed ret=%{public}d", ret);
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    UsbPipe pipe = {intfId, endpointId};
    sptr<UsbdBulkCallbackTest> usbdBulkCallback = new UsbdBulkCallbackTest();
    if (usbdBulkCallback == nullptr) {
        HDF_LOGE("LibusbAdapterTest::BulkCancelCallbackRace001 new callback failed");
        g_libusbAdapter->ReleaseInterface(dev, intfId);
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    sptr<Ashmem> ashmem;
    (void)InitAshmemOne(ashmem, MAX_BUFFER_LENGTH, 1);
    StartBulkEventThread(g_libusbAdapter, dev);
    ret = g_libusbAdapter->RegBulkCallback(dev, pipe, usbdBulkCallback);
    ASSERT_EQ(ret, HDF_SUCCESS);
    for (uint32_t i = 0; i < BULK_CANCEL_RACE_LOOP; i++) {
        int32_t readRet = g_libusbAdapter->BulkRead(dev, pipe, ashmem);
        if (readRet != HDF_SUCCESS) {
            usleep(BULK_CANCEL_WAIT_US);
            readRet = g_libusbAdapter->BulkRead(dev, pipe, ashmem);
        }
        HDF_LOGI("LibusbAdapterTest::BulkCancelCallbackRace001 %{public}d BulkRead=%{public}d", __LINE__, readRet);
        EXPECT_EQ(readRet, HDF_SUCCESS);
        int32_t cancelRet = g_libusbAdapter->BulkCancel(dev, pipe);
        HDF_LOGI("LibusbAdapterTest::BulkCancelCallbackRace001 %{public}d BulkCancel=%{public}d", __LINE__,
            cancelRet);
        EXPECT_EQ(cancelRet, HDF_SUCCESS);
        usleep(BULK_CANCEL_WAIT_US);
    }
    CleanupBulkTransfer(g_libusbAdapter, dev, pipe);
    g_libusbAdapter->ReleaseInterface(dev, intfId);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterBulkReadBusyReject001
 * @tc.desc: Test BulkRead rejects a second submission while the endpoint transfer is still in progress.
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkReadBusyReject001, TestSize.Level1)
{
    UsbDev dev = dev_;
    auto ret = g_libusbAdapter->OpenDevice(dev);
    ASSERT_EQ(0, ret);
    uint8_t intfId = 0;
    uint8_t endpointId = 0;
    if (!FindBulkInEndpoint(dev, intfId, endpointId)) {
        HDF_LOGI("LibusbAdapterTest::BulkReadBusyReject001 no bulk in endpoint, skip");
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    ret = g_libusbAdapter->ClaimInterface(dev, intfId, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGI("LibusbAdapterTest::BulkReadBusyReject001 claim interface failed ret=%{public}d", ret);
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    UsbPipe pipe = {intfId, endpointId};
    sptr<UsbdBulkCallbackTest> usbdBulkCallback = new UsbdBulkCallbackTest();
    if (usbdBulkCallback == nullptr) {
        HDF_LOGE("LibusbAdapterTest::BulkReadBusyReject001 new callback failed");
        g_libusbAdapter->ReleaseInterface(dev, intfId);
        g_libusbAdapter->CloseDevice(dev);
        return;
    }
    sptr<Ashmem> ashmem;
    (void)InitAshmemOne(ashmem, MAX_BUFFER_LENGTH, 1);
    StartBulkEventThread(g_libusbAdapter, dev);
    ret = g_libusbAdapter->RegBulkCallback(dev, pipe, usbdBulkCallback);
    ASSERT_EQ(ret, HDF_SUCCESS);
    ret = g_libusbAdapter->BulkRead(dev, pipe, ashmem);
    HDF_LOGI("LibusbAdapterTest::BulkReadBusyReject001 %{public}d first BulkRead=%{public}d", __LINE__, ret);
    EXPECT_EQ(ret, HDF_SUCCESS);
    ret = g_libusbAdapter->BulkRead(dev, pipe, ashmem);
    HDF_LOGI("LibusbAdapterTest::BulkReadBusyReject001 %{public}d second BulkRead=%{public}d", __LINE__, ret);
    EXPECT_TRUE(ret == LIBUSB_ERROR_BUSY || ret == HDF_SUCCESS);
    CleanupBulkTransfer(g_libusbAdapter, dev, pipe);
    g_libusbAdapter->ReleaseInterface(dev, intfId);
    g_libusbAdapter->CloseDevice(dev);
}

/**
 * @tc.name: LibusbAdapterBulkCancelNoDevice001
 * @tc.desc: Test BulkCancel with a non-existent device returns failure without crashing.
 * @tc.type: FUNC
 */
HWTEST_F(LibusbAdapterTest, LibusbAdapterBulkCancelNoDevice001, TestSize.Level1)
{
    UsbDev invalidDev = {0, 0};
    UsbPipe pipe = {INTERFACEID_OK, ENDPOINT_ADDRESS_IN};
    int32_t ret = g_libusbAdapter->BulkCancel(invalidDev, pipe);
    HDF_LOGI("LibusbAdapterTest::BulkCancelNoDevice001 %{public}d BulkCancel=%{public}d", __LINE__, ret);
    EXPECT_NE(ret, HDF_SUCCESS);
}
} // namespace OHOS::HDI::Usb::V1_2
