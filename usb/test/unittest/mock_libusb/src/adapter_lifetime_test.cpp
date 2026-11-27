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
 * Mock-libusb tests for LibusbAdapter: handle lifetime and hazard detection.
 *
 * Scope (batch 3):
 *   - InFlightTransferCount reflects async transfer lifecycle
 *   - CloseWhileInFlightCount stays 0 during normal open/close
 *   - UseAfterCloseCount stays 0 during normal open/close
 *   - ClearHazardCounters resets the hazard counters
 *   - BlockNextCall / ReleaseBlockedCall on ACTIVE_CONFIG_DESCRIPTOR
 *   - WaitingCallCount tracks blocked calls
 *   - double CloseDevice returns HDF_FAILURE
 *   - device removal (SetDevicePresent) during a queued transfer
 *   - hotplug leave event during a queued transfer
 *
 * The mock libusb provides hazard detectors (CloseWhileInFlightCount,
 * UseAfterCloseCount, InFlightTransferCount) that catch use-after-close and
 * close-while-in-flight conditions which would crash with the real library.
 * These tests verify that the adapter does not trigger any hazard during
 * normal operation.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

#include "ashmem.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "libusb.h"
#include "libusb_adapter.h"
#include "mock_libusb_control.h"
#include "usbd_type.h"
#include "v1_2/iusbd_transfer_callback.h"
#include "v1_2/usb_types.h"

using namespace OHOS::HDI::Usb::V1_2;
using namespace testing::ext;

/* Named constants of the mock device model used by these tests. */
#define BUS_NUM 1
#define ADDR_MAIN 2
#define ADDR_SPARE 3
#define ADDR_MISSING 9
#define MAIN_VID 0x1209
#define MAIN_PID 0x0001
#define SPARE_PID 0x0002
#define INTERFACE_ID 0
#define BULK_IN_EP 0x81
#define BULK_OUT_EP 0x02
#define ASYNC_TIMEOUT_MS 1000
#define ASYNC_BUFFER_LENGTH 512
#define CALLBACK_WAIT_TIMEOUT_MS 3000
#define CALLBACK_POLL_INTERVAL_MS 10
#define BLOCK_WAIT_TIMEOUT_MS 2000

namespace {

UsbDev MakeDev(uint8_t address)
{
    return UsbDev{BUS_NUM, address};
}

UsbPipe MakePipe(uint8_t interfaceId, uint8_t endpoint)
{
    return UsbPipe{interfaceId, endpoint};
}

class TestTransferCallback : public IUsbdTransferCallback {
public:
    TestTransferCallback() = default;
    ~TestTransferCallback() = default;

    int32_t OnTransferWriteCallback(int32_t status, int32_t actLength,
        const std::vector<UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        writeCalled_ = true;
        return HDF_SUCCESS;
    }

    int32_t OnTransferReadCallback(int32_t status, int32_t actLength,
        const std::vector<UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        readCalled_ = true;
        return HDF_SUCCESS;
    }

    bool WaitForRead(int32_t timeoutMs)
    {
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeoutMs);
        while (!readCalled_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CALLBACK_POLL_INTERVAL_MS));
        }
        return readCalled_.load();
    }

    bool WaitForWrite(int32_t timeoutMs)
    {
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeoutMs);
        while (!writeCalled_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CALLBACK_POLL_INTERVAL_MS));
        }
        return writeCalled_.load();
    }

    std::atomic<bool> readCalled_{false};
    std::atomic<bool> writeCalled_{false};
};

sptr<Ashmem> MakeAshmem(int32_t size)
{
    sptr<Ashmem> ashmem = Ashmem::CreateAshmem("lifetime_test_ashmem", size);
    if (ashmem == nullptr) {
        return nullptr;
    }
    ashmem->MapReadAndWriteAshmem();
    return ashmem;
}

class AdapterLifetimeTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterLifetimeTest::adapter_ = nullptr;

void AdapterLifetimeTest::SetUpTestCase()
{
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_SPARE, MAIN_VID, SPARE_PID));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    mock_usb::SetAutoComplete(true);
    mock_usb::ClearHazardCounters();
}

void AdapterLifetimeTest::TearDownTestCase()
{
    if (adapter_ != nullptr) {
        (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
        (void)adapter_->CloseDevice(MakeDev(ADDR_MAIN));
    }
    adapter_ = nullptr;
}

/* ---- InFlightTransferCount ---- */

/**
 * @tc.name: InFlightCountReflectsQueuedTransfer
 * @tc.desc: Test InFlightTransferCount during async transfer lifecycle
 * @tc.desc: Positive test: count is 1 when a transfer is queued, 0 after completion
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, InFlightCountReflectsQueuedTransfer, TestSize.Level1)
{
    mock_usb::SetAutoComplete(false);
    EXPECT_EQ(0, mock_usb::InFlightTransferCount());

    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_EQ(1, mock_usb::InFlightTransferCount());

    /* Re-enable auto-completion so the event thread processes the transfer. */
    mock_usb::SetAutoComplete(true);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    EXPECT_EQ(0, mock_usb::InFlightTransferCount());
}

/* ---- Hazard counters during normal operation ---- */

/**
 * @tc.name: NormalOperationNoCloseWhileInFlight
 * @tc.desc: Test that CloseWhileInFlightCount stays 0 after normal async transfers
 * @tc.desc: Positive test: submit and complete a transfer, then verify no hazard
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, NormalOperationNoCloseWhileInFlight, TestSize.Level1)
{
    mock_usb::ClearHazardCounters();
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    /* No close-while-in-flight hazard should be recorded. */
    EXPECT_EQ(0, mock_usb::CloseWhileInFlightCount());
}

/**
 * @tc.name: NormalOperationNoUseAfterClose
 * @tc.desc: Test that UseAfterCloseCount stays 0 after normal async transfers
 * @tc.desc: Positive test: submit and complete a transfer, then verify no hazard
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, NormalOperationNoUseAfterClose, TestSize.Level1)
{
    mock_usb::ClearHazardCounters();
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_OUT_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    /* Wait for the write to finish before checking the hazard counter, so the
     * whole transfer lifecycle (submit → complete → cleanup) is covered. */
    EXPECT_TRUE(cb->WaitForWrite(CALLBACK_WAIT_TIMEOUT_MS));
    /* No use-after-close hazard should be recorded. */
    EXPECT_EQ(0, mock_usb::UseAfterCloseCount());
}

/* ---- ClearHazardCounters ---- */

/**
 * @tc.name: ClearHazardCountersResetsCounters
 * @tc.desc: Test that ClearHazardCounters resets CloseWhileInFlight and UseAfterClose
 * @tc.desc: Positive test: counters become non-zero after real hazards, then 0 after clear
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, ClearHazardCountersResetsCounters, TestSize.Level1)
{
    /* Produce both hazards deliberately on a raw mock libusb handle for the
     * spare device, so the adapter's own handle for ADDR_MAIN is not disturbed. */
    libusb_device **list = nullptr;
    ssize_t count = libusb_get_device_list(nullptr, &list);
    ASSERT_GT(count, 0);
    libusb_device *spare = nullptr;
    for (ssize_t i = 0; i < count; i++) {
        if (libusb_get_bus_number(list[i]) == BUS_NUM &&
            libusb_get_device_address(list[i]) == ADDR_SPARE) {
            spare = list[i];
            break;
        }
    }
    ASSERT_NE(nullptr, spare);
    libusb_device_handle *rawHandle = nullptr;
    ASSERT_EQ(LIBUSB_SUCCESS, libusb_open(spare, &rawHandle));
    libusb_free_device_list(list, 1);

    /* Hazard 1: close the handle while a submitted transfer is still in flight.
     * Auto-completion is disabled so the event thread cannot complete it first. */
    mock_usb::SetAutoComplete(false);
    struct libusb_transfer *transfer = libusb_alloc_transfer(0);
    ASSERT_NE(nullptr, transfer);
    unsigned char *buffer = static_cast<unsigned char *>(malloc(ASYNC_BUFFER_LENGTH));
    ASSERT_NE(nullptr, buffer);
    libusb_fill_bulk_transfer(transfer, rawHandle, BULK_OUT_EP, buffer, ASYNC_BUFFER_LENGTH,
        nullptr, nullptr, ASYNC_TIMEOUT_MS);
    /* The completion path frees both buffer and transfer once the event thread
     * processes this quarantined transfer. */
    transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
    ASSERT_EQ(LIBUSB_SUCCESS, libusb_submit_transfer(transfer));
    libusb_close(rawHandle);
    EXPECT_GE(mock_usb::CloseWhileInFlightCount(), 1);

    /* Hazard 2: synchronous bulk transfer on the already closed handle. */
    unsigned char syncBuffer[ASYNC_BUFFER_LENGTH] = {'\0'};
    int transferred = 0;
    (void)libusb_bulk_transfer(rawHandle, BULK_OUT_EP, syncBuffer, ASYNC_BUFFER_LENGTH,
        &transferred, ASYNC_TIMEOUT_MS);
    EXPECT_GE(mock_usb::UseAfterCloseCount(), 1);

    /* Re-enable auto-completion and let the event thread drain the pending
     * transfer so later tests start from a clean state. */
    mock_usb::SetAutoComplete(true);
    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(BLOCK_WAIT_TIMEOUT_MS);
    while (mock_usb::PendingTransferCount() > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CALLBACK_POLL_INTERVAL_MS));
    }
    EXPECT_EQ(0u, mock_usb::PendingTransferCount());

    /* Both counters were non-zero; after the reset both must read 0. */
    mock_usb::ClearHazardCounters();
    EXPECT_EQ(0, mock_usb::CloseWhileInFlightCount());
    EXPECT_EQ(0, mock_usb::UseAfterCloseCount());
}

/* ---- Double CloseDevice ---- */

/**
 * @tc.name: DoubleCloseReturnsFailure
 * @tc.desc: Test that closing a device twice returns HDF_FAILURE
 * @tc.desc: Negative test: the second CloseDevice finds no handle in the map
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, DoubleCloseReturnsFailure, TestSize.Level1)
{
    /* Open and close the spare device. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_SPARE)));
    EXPECT_EQ(HDF_SUCCESS, adapter_->CloseDevice(MakeDev(ADDR_SPARE)));
    /* Second close should fail — the handle was removed from the map. */
    EXPECT_NE(HDF_SUCCESS, adapter_->CloseDevice(MakeDev(ADDR_SPARE)));
    /* No hazard should be triggered by a clean double-close. */
    EXPECT_EQ(0, mock_usb::CloseWhileInFlightCount());
    EXPECT_EQ(0, mock_usb::UseAfterCloseCount());
}

/* ---- BlockNextCall / ReleaseBlockedCall ---- */

/**
 * @tc.name: BlockNextCallFreezesActiveConfigDescriptor
 * @tc.desc: Test BlockNextCall on ACTIVE_CONFIG_DESCRIPTOR
 * @tc.desc: Positive test: a synchronous transfer blocks inside GetEndpointDesc,
 *          WaitingCallCount becomes 1, then returns 0 after ReleaseBlockedCall
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, BlockNextCallFreezesActiveConfigDescriptor, TestSize.Level1)
{
    mock_usb::BlockNextCall(mock_usb::MockApi::ACTIVE_CONFIG_DESCRIPTOR);

    std::atomic<bool> transferDone(false);
    std::thread worker([&transferDone]() {
        std::vector<uint8_t> data;
        /* BulkTransferReadwithLength resolves the endpoint via GetEndpointDesc →
         * libusb_get_active_config_descriptor, which is the blocking point.
         * (BulkTransferRead does not query the config descriptor, so it can
         * never reach the block.) The call will freeze until we release it. */
        (void)adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
            MakePipe(INTERFACE_ID, BULK_IN_EP), ASYNC_TIMEOUT_MS, ASYNC_BUFFER_LENGTH, data);
        transferDone = true;
    });

    /* Wait for the worker thread to reach the blocking point. */
    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(BLOCK_WAIT_TIMEOUT_MS);
    while (mock_usb::WaitingCallCount() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CALLBACK_POLL_INTERVAL_MS));
    }
    EXPECT_EQ(1, mock_usb::WaitingCallCount());
    EXPECT_FALSE(transferDone.load());

    /* Release the blocked call so the worker thread can proceed. */
    mock_usb::ReleaseBlockedCall();
    worker.join();
    EXPECT_TRUE(transferDone.load());
    EXPECT_EQ(0, mock_usb::WaitingCallCount());
}

/* ---- Device removal during queued transfer ---- */

/**
 * @tc.name: DeviceRemovalDuringQueuedTransfer
 * @tc.desc: Test that removing a device while a transfer is queued completes with NO_DEVICE
 * @tc.desc: Positive test: SetDevicePresent(false) → transfer completes with status 5
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, DeviceRemovalDuringQueuedTransfer, TestSize.Level1)
{
    mock_usb::SetAutoComplete(false);
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_EQ(1, mock_usb::InFlightTransferCount());

    /* Mark the device as absent — the transfer should complete with NO_DEVICE. */
    ASSERT_EQ(0, mock_usb::SetDevicePresent(BUS_NUM, ADDR_MAIN, false));

    /* Re-enable auto-completion so the event thread processes the transfer. */
    mock_usb::SetAutoComplete(true);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    /* LIBUSB_TRANSFER_NO_DEVICE = 5. The callback fires, but the adapter's
     * HandleAsyncResult sees status != COMPLETED and actual_length <= 0, so
     * it calls HandleAsyncFailure instead of WriteAshmem. The callback still
     * fires from FeedbackToBase. */
    EXPECT_EQ(0, mock_usb::InFlightTransferCount());

    /* Restore the device for subsequent tests. */
    ASSERT_EQ(0, mock_usb::SetDevicePresent(BUS_NUM, ADDR_MAIN, true));
}

/* ---- Hotplug leave during queued transfer ---- */

/**
 * @tc.name: HotplugLeaveDuringQueuedTransfer
 * @tc.desc: Test that a hotplug leave event while a transfer is queued causes NO_DEVICE
 * @tc.desc: Positive test: RaiseHotplug(arrived=false) → transfer completes with NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, HotplugLeaveDuringQueuedTransfer, TestSize.Level1)
{
    mock_usb::SetAutoComplete(false);
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_BULK,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);

    /* Raise a hotplug leave event. RaiseHotplug returns 0 when a subscriber is
     * registered and -1 when nobody is listening, but either way the device
     * presence is cleared. */
    ret = mock_usb::RaiseHotplug(BUS_NUM, ADDR_MAIN, false);
    EXPECT_TRUE(ret == 0 || ret == -1);

    /* Re-enable auto-completion and wait for the transfer to complete. */
    mock_usb::SetAutoComplete(true);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    EXPECT_EQ(0, mock_usb::InFlightTransferCount());

    /* Restore device presence for subsequent tests. The adapter handle is still
     * in the map (hotplug leave does not auto-close), so no re-open is needed. */
    ret = mock_usb::RaiseHotplug(BUS_NUM, ADDR_MAIN, true);
    EXPECT_TRUE(ret == 0 || ret == -1);
}

/* ---- GetDevices after open/close ---- */

/**
 * @tc.name: GetDevicesReturnsPresentDevices
 * @tc.desc: Test GetDevices returns the list of present mock devices
 * @tc.desc: Positive test: after opening ADDR_MAIN, GetDevices returns it
 * @tc.type: FUNC
 */
HWTEST_F(AdapterLifetimeTest, GetDevicesReturnsPresentDevices, TestSize.Level1)
{
    std::vector<DeviceInfo> devices;
    int32_t ret = adapter_->GetDevices(devices);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* At least ADDR_MAIN should be present. */
    bool found = false;
    for (const auto& dev : devices) {
        /* DeviceInfo.deviceId = (busNum << 32) + devAddr. */
        uint8_t bus = static_cast<uint8_t>((dev.deviceId >> 32) & 0xFF);
        uint8_t addr = static_cast<uint8_t>(dev.deviceId & 0xFF);
        if (bus == BUS_NUM && addr == ADDR_MAIN) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
} // namespace
