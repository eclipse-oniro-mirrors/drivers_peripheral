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
 * Mock-libusb tests for LibusbAdapter: asynchronous transfer paths.
 *
 * Scope (batch 2):
 *   - AsyncSubmitTransfer on bulk IN / OUT endpoints (callback fires)
 *   - AsyncSubmitTransfer on interrupt IN endpoint (alt setting 1)
 *   - AsyncSubmitTransfer on isochronous IN endpoint
 *   - AsyncSubmitTransfer failure paths (unopened device, submit fault)
 *   - AsyncCancelTransfer (success, no transfer, unopened device)
 *   - SetAutoComplete / PendingTransferCount (queued transfer lifecycle)
 *   - SetTransferDelayMs (delayed completion)
 *   - FailNextCall on SUBMIT_TRANSFER
 *
 * The tests exercise the real LibusbAdapter source compiled against the mock
 * libusb, so every async path through the adapter is verified without USB
 * hardware.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "ashmem.h"
#include "hdf_base.h"
#include "hdf_log.h"
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
#define ADDR_MISSING 9
#define MAIN_VID 0x1209
#define MAIN_PID 0x0001
#define INTERFACE_ID 0
#define BULK_IN_EP 0x81
#define BULK_OUT_EP 0x02
#define INTERRUPT_IN_EP 0x83
#define BULK_MAX_PACKET_SIZE 512
#define ASYNC_TIMEOUT_MS 1000
#define ASYNC_BUFFER_LENGTH 512
#define ASYNC_ISO_BUFFER_LENGTH 256
#define ASYNC_ISO_PACKETS 2
#define CALLBACK_WAIT_TIMEOUT_MS 3000
#define CALLBACK_POLL_INTERVAL_MS 10
#define TRANSFER_DELAY_MS 50
#define INVALID_NUM 222

namespace {

UsbDev MakeDev(uint8_t address)
{
    return UsbDev{BUS_NUM, address};
}

/*
 * Test callback that records the transfer result via atomic flags.
 * The adapter calls OnTransferReadCallback / OnTransferWriteCallback
 * from the event-handling thread when a transfer completes.
 */
class TestTransferCallback : public IUsbdTransferCallback {
public:
    TestTransferCallback() = default;
    ~TestTransferCallback() = default;

    int32_t OnTransferWriteCallback(int32_t status, int32_t actLength,
        const std::vector<UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        writeStatus_ = status;
        writeActLength_ = actLength;
        writeCalled_ = true;
        return HDF_SUCCESS;
    }

    int32_t OnTransferReadCallback(int32_t status, int32_t actLength,
        const std::vector<UsbIsoPacketDescriptor>& descs, const uint64_t userData) override
    {
        readStatus_ = status;
        readActLength_ = actLength;
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

    void Reset()
    {
        readCalled_ = false;
        writeCalled_ = false;
        readStatus_ = -1;
        readActLength_ = -1;
        writeStatus_ = -1;
        writeActLength_ = -1;
    }

    std::atomic<bool> readCalled_{false};
    std::atomic<int32_t> readStatus_{-1};
    std::atomic<int32_t> readActLength_{-1};
    std::atomic<bool> writeCalled_{false};
    std::atomic<int32_t> writeStatus_{-1};
    std::atomic<int32_t> writeActLength_{-1};
};

/* Create and map an ashmem buffer of the given size. */
sptr<Ashmem> MakeAshmem(int32_t size)
{
    sptr<Ashmem> ashmem = Ashmem::CreateAshmem("async_test_ashmem", size);
    if (ashmem == nullptr) {
        return nullptr;
    }
    ashmem->MapReadAndWriteAshmem();
    return ashmem;
}

class AdapterAsyncTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterAsyncTest::adapter_ = nullptr;

void AdapterAsyncTest::SetUpTestCase()
{
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    /* Ensure auto-completion is enabled by default. */
    mock_usb::SetAutoComplete(true);
}

void AdapterAsyncTest::TearDownTestCase()
{
    if (adapter_ != nullptr) {
        (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
        (void)adapter_->CloseDevice(MakeDev(ADDR_MAIN));
    }
    adapter_ = nullptr;
}

/* ---- AsyncSubmitTransfer: bulk read ---- */

/**
 * @tc.name: AsyncSubmitBulkReadSucceeds
 * @tc.desc: Test AsyncSubmitTransfer with a bulk IN endpoint
 * @tc.desc: int32_t AsyncSubmitTransfer(const UsbDev &dev, const USBTransferInfo &info,
 *          const sptr<IUsbdTransferCallback> &cb, const sptr<Ashmem> &ashmem);
 * @tc.desc: Positive test: the transfer completes and the read callback fires
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitBulkReadSucceeds, TestSize.Level1)
{
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
    /* LIBUSB_TRANSFER_COMPLETED = 0, matching the mock's PATTERN data return. */
    EXPECT_EQ(0, cb->readStatus_.load());
    EXPECT_GT(cb->readActLength_.load(), 0);
}

/**
 * @tc.name: AsyncSubmitBulkWriteSucceeds
 * @tc.desc: Test AsyncSubmitTransfer with a bulk OUT endpoint
 * @tc.desc: Positive test: the write transfer completes and the write callback fires
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitBulkWriteSucceeds, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    /* Fill the ashmem with data for the OUT transfer. */
    std::vector<uint8_t> fillData(ASYNC_BUFFER_LENGTH, 0xAA);
    ashmem->WriteToAshmem(fillData.data(), fillData.size(), 0);

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
    EXPECT_TRUE(cb->WaitForWrite(CALLBACK_WAIT_TIMEOUT_MS));
    EXPECT_EQ(0, cb->writeStatus_.load());
    EXPECT_EQ(ASYNC_BUFFER_LENGTH, cb->writeActLength_.load());
}

/* ---- AsyncSubmitTransfer: interrupt read ---- */

/**
 * @tc.name: AsyncSubmitInterruptReadSucceeds
 * @tc.desc: Test AsyncSubmitTransfer with an interrupt IN endpoint (alt 1)
 * @tc.desc: Positive test: interrupt IN endpoint 0x83 returns data on alt 1
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitInterruptReadSucceeds, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = INTERRUPT_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_INTERRUPT,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = 0,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    EXPECT_EQ(0, cb->readStatus_.load());
    EXPECT_GT(cb->readActLength_.load(), 0);
    /* Restore alt 0 for subsequent tests. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 0));
}

/* ---- AsyncSubmitTransfer: isochronous read ---- */

/**
 * @tc.name: AsyncSubmitIsoReadSucceeds
 * @tc.desc: Test AsyncSubmitTransfer with an isochronous IN endpoint
 * @tc.desc: Positive test: iso read on a bulk IN endpoint delegates to bulk in the adapter
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitIsoReadSucceeds, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(ASYNC_ISO_BUFFER_LENGTH);
    ASSERT_NE(nullptr, ashmem);
    sptr<TestTransferCallback> cb = new TestTransferCallback();
    ASSERT_NE(nullptr, cb);
    USBTransferInfo info = {
        .endpoint = BULK_IN_EP,
        .flags = 0,
        .type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS,
        .timeOut = ASYNC_TIMEOUT_MS,
        .length = ASYNC_ISO_BUFFER_LENGTH,
        .userData = 0,
        .numIsoPackets = ASYNC_ISO_PACKETS,
    };
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    /* The bulk IN endpoint has no maxInLength cap, so both iso packets complete
     * fully: 2 packets * 128 bytes = ASYNC_ISO_BUFFER_LENGTH, status COMPLETED. */
    EXPECT_EQ(0, cb->readStatus_.load());
    EXPECT_EQ(ASYNC_ISO_BUFFER_LENGTH, cb->readActLength_.load());
}

/* ---- AsyncSubmitTransfer failure paths ---- */

/**
 * @tc.name: AsyncSubmitFailsOnUnopenedDevice
 * @tc.desc: Test AsyncSubmitTransfer on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails → LIBUSB_ERROR_NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitFailsOnUnopenedDevice, TestSize.Level1)
{
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
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MISSING), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_ERROR_NO_DEVICE, ret);
}

/**
 * @tc.name: AsyncSubmitFailsOnSubmitError
 * @tc.desc: Test AsyncSubmitTransfer with FailNextCall on SUBMIT_TRANSFER
 * @tc.desc: Negative test: libusb_submit_transfer fails → error returned
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitFailsOnSubmitError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::SUBMIT_TRANSFER, LIBUSB_ERROR_NO_MEM, 1);
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
    EXPECT_EQ(LIBUSB_ERROR_NO_MEM, ret);
}

/* ---- AsyncCancelTransfer ---- */

/**
 * @tc.name: AsyncCancelTransferSucceeds
 * @tc.desc: Test AsyncCancelTransfer after submitting a transfer
 * @tc.desc: Positive test: disable auto-complete, submit, cancel → LIBUSB_SUCCESS
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncCancelTransferSucceeds, TestSize.Level1)
{
    /* Disable auto-completion so the transfer stays queued. */
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
    EXPECT_GT(mock_usb::PendingTransferCount(), 0u);

    ret = adapter_->AsyncCancelTransfer(MakeDev(ADDR_MAIN), BULK_IN_EP);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);

    /* Re-enable auto-completion so the cancelled transfer is processed. */
    mock_usb::SetAutoComplete(true);
    /* Give the event thread time to process the cancelled transfer. */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

/**
 * @tc.name: AsyncCancelFailsOnNoTransfer
 * @tc.desc: Test AsyncCancelTransfer when no transfer has been submitted
 * @tc.desc: Negative test: transfer list is empty → LIBUSB_ERROR_NOT_FOUND
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncCancelFailsOnNoTransfer, TestSize.Level1)
{
    /* The transfer list should be empty at this point (previous transfers
     * completed or cancelled). The adapter returns LIBUSB_ERROR_NOT_FOUND
     * when the list is empty. */
    int32_t ret = adapter_->AsyncCancelTransfer(MakeDev(ADDR_MAIN), BULK_IN_EP);
    EXPECT_EQ(LIBUSB_ERROR_NOT_FOUND, ret);
}

/**
 * @tc.name: AsyncCancelFailsOnUnopenedDevice
 * @tc.desc: Test AsyncCancelTransfer on a device that was never opened
 * @tc.desc: Negative test: GetAsyncWrapper fails → LIBUSB_ERROR_NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncCancelFailsOnUnopenedDevice, TestSize.Level1)
{
    int32_t ret = adapter_->AsyncCancelTransfer(MakeDev(ADDR_MISSING), BULK_IN_EP);
    EXPECT_EQ(LIBUSB_ERROR_NO_DEVICE, ret);
}

/* ---- SetAutoComplete / PendingTransferCount ---- */

/**
 * @tc.name: AsyncSubmitWithAutoCompleteDisabled
 * @tc.desc: Test that a submitted transfer stays pending when auto-complete is off
 * @tc.desc: Positive test: PendingTransferCount > 0 after submit, then 0 after re-enabling
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitWithAutoCompleteDisabled, TestSize.Level1)
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
    /* The transfer should be queued but not completed. */
    EXPECT_EQ(1u, mock_usb::PendingTransferCount());
    EXPECT_FALSE(cb->readCalled_.load());

    /* Re-enable auto-completion and wait for the callback. */
    mock_usb::SetAutoComplete(true);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    EXPECT_EQ(0u, mock_usb::PendingTransferCount());
}

/* ---- SetTransferDelayMs ---- */

/**
 * @tc.name: AsyncSubmitWithTransferDelay
 * @tc.desc: Test that SetTransferDelayMs delays transfer completion
 * @tc.desc: Positive test: the callback fires after the configured delay
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitWithTransferDelay, TestSize.Level1)
{
    mock_usb::SetTransferDelayMs(TRANSFER_DELAY_MS);
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
    auto start = std::chrono::steady_clock::now();
    int32_t ret = adapter_->AsyncSubmitTransfer(MakeDev(ADDR_MAIN), info, cb, ashmem);
    EXPECT_EQ(LIBUSB_SUCCESS, ret);
    EXPECT_TRUE(cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    /* The delay is applied outside the mock lock; allow some scheduling jitter. */
    EXPECT_GE(elapsed, static_cast<int64_t>(TRANSFER_DELAY_MS) - 10);
    /* Disable the delay for subsequent tests. */
    mock_usb::SetTransferDelayMs(0);
}

/* ---- TransferRecord verification ---- */

/**
 * @tc.name: AsyncSubmitRecordsSubmitKind
 * @tc.desc: Test that a submit record is logged in the transfer log
 * @tc.desc: Positive test: RecordCount increases and the record kind is "submit"
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitRecordsSubmitKind, TestSize.Level1)
{
    mock_usb::ClearRecords();
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
    /* At least one "submit" record should exist. */
    bool found = false;
    for (size_t i = 0; i < mock_usb::RecordCount(); i++) {
        auto record = mock_usb::RecordAt(i);
        if (record.kind == "submit" && record.endpoint == BULK_IN_EP) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    /* Wait for completion to avoid interfering with subsequent tests. */
    cb->WaitForRead(CALLBACK_WAIT_TIMEOUT_MS);
}

/* ---- Endpoint fault injection on async path ---- */

/**
 * @tc.name: AsyncSubmitBulkReadWithEndpointFault
 * @tc.desc: Test AsyncSubmitTransfer with SetEndpointBehavior injecting an IN fault
 * @tc.desc: Negative test: the transfer completes with LIBUSB_TRANSFER_STALL (4)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterAsyncTest, AsyncSubmitBulkReadWithEndpointFault, TestSize.Level1)
{
    /* Inject a PIPE error (-9) on the next IN transfer on ep 0x81. */
    mock_usb::EndpointSpec fault;
    fault.address = BULK_IN_EP;
    fault.failInTimes = 1;
    fault.errorIn = -9; /* LIBUSB_ERROR_PIPE */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_IN_EP, fault));

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
    /* LIBUSB_ERROR_PIPE(-9) maps to LIBUSB_TRANSFER_STALL(4) in the mock. */
    EXPECT_EQ(4, cb->readStatus_.load());
}
} // namespace
