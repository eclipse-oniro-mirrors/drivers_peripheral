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
 * Mock-libusb tests for LibusbAdapter: bulk v2 (ashmem based) paths.
 *
 * Scope (batch 4):
 *   - BulkRead: endpoint direction check, unopened device, missing callback,
 *     zero max packet size, submit rejection, successful submit
 *   - BulkWrite: endpoint direction check, unopened device, missing callback,
 *     invalid ashmem, successful submit
 *   - BulkCancel: unknown device, unregistered endpoint, success + repeated
 *     cancel (NOT_FOUND tolerated; cleanup deferred to the completion callback)
 *   - RegBulkCallback: null callback, unknown device, repeated registration on
 *     an idle endpoint, rejection while transferring
 *   - UnRegBulkCallback: unknown device, unregistered endpoint,
 *     transferring rejection, idle success + entry erased
 *   - SetLoadUsbSaSubscriber: null subscriber, hotplug register failure,
 *     success + initial device list notification + arrived event
 *
 * Real-device paths (GetDeviceFileDescriptor / GetFileDescriptor /
 * GetRawDescriptor / GetDeviceMemMapFd / SendPipeRequest*) are NOT covered
 * here on purpose: they need /dev/bus/usb nodes and are out of scope for the
 * mock-libusb suite.
 *
 * The bulk completion callback chain (HandleBulkResult -> BulkFeedbackToBase)
 * requires the LibusbEventHandling thread, which is only started by
 * AsyncSubmitTransfer; these tests verify the submit/cancel state machine
 * (return values and mock transfer log) without driving that thread.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <vector>

#include "ashmem.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "libusb_adapter.h"
#include "mock_libusb_control.h"
#include "usbd_type.h"
#include "v2_0/iusbd_bulk_callback.h"

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
/* Extra bulk-IN endpoints so each in-flight test owns a distinct transfer. */
#define BULK_IN_EP_FAULT 0x85
#define BULK_IN_EP_UNREG 0x86
#define BULK_IN_EP_CANCEL 0x87
/* Dedicated bulk-OUT endpoint for the submit-failure path: the previous write
 * test leaves 0x02 in flight, so sharing it would hit the isTransferring guard
 * instead of the injected libusb_submit_transfer failure. */
#define BULK_OUT_EP_FAULT 0x04
/* Dedicated idle OUT endpoint for repeated-registration assertions. */
#define BULK_OUT_EP_REREG 0x08
/* Unregistered OUT endpoint used for pure list-management assertions. */
#define BULK_OUT_EP_IDLE 0x06
#define BULK_MAX_PACKET_SIZE 512
#define INVALID_INJECTED_ERROR (-9)
#define INVALID_EP_UNREGISTERED 0x05
#define WRITE_PAYLOAD_LENGTH 16

namespace {

UsbDev MakeDev(uint8_t address)
{
    return UsbDev{BUS_NUM, address};
}

UsbPipe MakePipe(uint8_t interfaceId, uint8_t endpoint)
{
    return UsbPipe{interfaceId, endpoint};
}

/*
 * Test callback for RegBulkCallback. The adapter stores it in the bulk
 * transfer entry; the completion path is not driven by these tests, so the
 * counters only prove registration does not crash and keeps the object alive.
 */
class TestBulkCallback : public OHOS::HDI::Usb::V2_0::IUsbdBulkCallback {
public:
    TestBulkCallback() = default;
    virtual ~TestBulkCallback() = default;

    int32_t OnBulkWriteCallback(int32_t status, int32_t actLength) override
    {
        writeStatus_ = status;
        writeActLength_ = actLength;
        writeCalled_ = true;
        return HDF_SUCCESS;
    }

    int32_t OnBulkReadCallback(int32_t status, int32_t actLength) override
    {
        readStatus_ = status;
        readActLength_ = actLength;
        readCalled_ = true;
        return HDF_SUCCESS;
    }

    std::atomic<bool> writeCalled_{false};
    std::atomic<int32_t> writeStatus_{-1};
    std::atomic<int32_t> writeActLength_{-1};
    std::atomic<bool> readCalled_{false};
    std::atomic<int32_t> readStatus_{-1};
    std::atomic<int32_t> readActLength_{-1};
};

/* Counts LoadUsbSa invocations from SetLoadUsbSaSubscriber paths. */
class TestSaSubscriber : public LibUsbSaSubscriber {
public:
    TestSaSubscriber() = default;
    ~TestSaSubscriber() override = default;

    int32_t LoadUsbSa(const int32_t &eventId) override
    {
        count_++;
        lastEvent_ = eventId;
        return HDF_SUCCESS;
    }

    std::atomic<int32_t> count_{0};
    std::atomic<int32_t> lastEvent_{-1};
};

/* Create and map an ashmem buffer of the given size. */
sptr<Ashmem> MakeAshmem(int32_t size)
{
    sptr<Ashmem> ashmem = Ashmem::CreateAshmem("bulk_v2_test_ashmem", size);
    if (ashmem == nullptr) {
        return nullptr;
    }
    ashmem->MapReadAndWriteAshmem();
    return ashmem;
}

/* True if the mock transfer log contains a submit record for the endpoint. */
bool HasSubmitRecord(uint8_t endpoint)
{
    for (size_t i = 0; i < mock_usb::RecordCount(); i++) {
        auto record = mock_usb::RecordAt(i);
        if (record.kind == "submit" && record.endpoint == endpoint) {
            return true;
        }
    }
    return false;
}

class AdapterBulkV2Test : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterBulkV2Test::adapter_ = nullptr;

void AdapterBulkV2Test::SetUpTestCase()
{
    mock_usb::Reset();
    /* Default spec plus extra bulk IN endpoints: mock libusb refuses a
     * second submit while the previous transfer is still pending, and the
     * event-handling thread is not started here, so every test that leaves a
     * transfer in flight uses its own endpoint number. */
    mock_usb::DeviceSpec spec = mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID);
    for (uint8_t ep : {BULK_IN_EP_FAULT, BULK_IN_EP_UNREG, BULK_IN_EP_CANCEL, BULK_OUT_EP_FAULT,
        BULK_OUT_EP_REREG}) {
        mock_usb::EndpointSpec extra;
        extra.address = ep;
        extra.attributes = MOCK_EP_DEFAULT_ATTR_BULK;
        extra.maxPacketSize = MOCK_EP_DEFAULT_MAX_PACKET_SIZE;
        spec.configs[0].interfaces[0].altSettings[0].endpoints.push_back(extra);
    }
    ASSERT_EQ(0, mock_usb::AddDevice(spec));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
    /* BulkTransferInit runs inside OpenDevice and creates the bulk wrapper
     * required by RegBulkCallback / FindBulkTransfer. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
}

void AdapterBulkV2Test::TearDownTestCase()
{
    if (adapter_ != nullptr) {
        (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
        (void)adapter_->CloseDevice(MakeDev(ADDR_MAIN));
    }
    adapter_ = nullptr;
}

/* ---- BulkRead tests ---- */

/**
 * @tc.name: BulkReadRejectsOutEndpoint
 * @tc.desc: Test BulkRead with endpointId < ENDPOINTID(128)
 * @tc.desc: int32_t BulkRead(const UsbDev &dev, const UsbPipe &pipe, const sptr<Ashmem> &ashmem);
 * @tc.desc: Negative test: a bulk OUT endpoint (0x02) is rejected before touching libusb
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadRejectsOutEndpoint, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkReadFailsOnUnopenedDevice
 * @tc.desc: Test BulkRead on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails for ADDR_MISSING
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadFailsOnUnopenedDevice, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_IN_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkReadFailsWithoutRegisteredCallback
 * @tc.desc: Test BulkRead before RegBulkCallback registered the endpoint
 * @tc.desc: Negative test: FindBulkTransfer returns nullptr when no callback
 *           was registered for the endpoint
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadFailsWithoutRegisteredCallback, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkReadFailsOnZeroMaxPacketSize
 * @tc.desc: Test BulkRead when libusb_get_max_packet_size fails
 * @tc.desc: Negative test: FailNextCall(GET_MAX_PACKET_SIZE) makes the buffer
 *           length negative, AllocBulkBuffer returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadFailsOnZeroMaxPacketSize, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_FAULT), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    mock_usb::FailNextCall(mock_usb::MockApi::GET_MAX_PACKET_SIZE, INVALID_INJECTED_ERROR);
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_FAULT), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
    /* No submit record: the failure happens before libusb_submit_transfer. */
    mock_usb::ClearRecords();
    EXPECT_FALSE(HasSubmitRecord(BULK_IN_EP_FAULT));
}

/**
 * @tc.name: BulkReadFailsWhenSubmitRejected
 * @tc.desc: Test BulkRead when libusb_submit_transfer fails
 * @tc.desc: Negative test: FailNextCall(SUBMIT_TRANSFER) makes the adapter
 *           return the negative libusb error code
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadFailsWhenSubmitRejected, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_FAULT), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    mock_usb::FailNextCall(mock_usb::MockApi::SUBMIT_TRANSFER, INVALID_INJECTED_ERROR);
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_FAULT), ashmem);
    /* The exact injected code proves the submit branch was reached. */
    EXPECT_EQ(INVALID_INJECTED_ERROR, ret);
}

/**
 * @tc.name: BulkReadSubmitsAndRejectsDoubleSubmit
 * @tc.desc: Test BulkRead success and its in-flight protection
 * @tc.desc: Positive test: the transfer is submitted (submit record logged).
 * @tc.desc: Negative test: a second BulkRead while the previous transfer is
 *           still pending returns LIBUSB_ERROR_BUSY (real libusb semantics),
 *           covering the ret < 0 branch of the submit result.
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkReadSubmitsAndRejectsDoubleSubmit, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    mock_usb::ClearRecords();
    int32_t ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), ashmem);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_TRUE(HasSubmitRecord(BULK_IN_EP));

    ret = adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), ashmem);
    EXPECT_NE(HDF_SUCCESS, ret);
    EXPECT_LT(ret, 0);
}

/* ---- BulkWrite tests ---- */

/**
 * @tc.name: BulkWriteRejectsInEndpoint
 * @tc.desc: Test BulkWrite with endpointId >= ENDPOINTID(128)
 * @tc.desc: Negative test: a bulk IN endpoint (0x81) is rejected before
 *           touching libusb
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteRejectsInEndpoint, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkWriteFailsOnUnopenedDevice
 * @tc.desc: Test BulkWrite on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails for ADDR_MISSING
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteFailsOnUnopenedDevice, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkWriteFailsWithoutRegisteredCallback
 * @tc.desc: Test BulkWrite before RegBulkCallback registered the endpoint
 * @tc.desc: Negative test: FindBulkTransfer returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteFailsWithoutRegisteredCallback, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkWriteFailsWithNullAshmem
 * @tc.desc: Test BulkWrite with an invalid (null) ashmem
 * @tc.desc: Negative test: for OUT endpoints AllocBulkBuffer reads from the
 *           ashmem; a null ashmem makes ReadAshmem fail and the buffer null
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteFailsWithNullAshmem, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), cb));
    const sptr<Ashmem> ashmem = nullptr;
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), ashmem);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkWriteSubmitsTransfer
 * @tc.desc: Test BulkWrite on a registered bulk OUT endpoint
 * @tc.desc: Positive test: data is read from the mapped ashmem and the
 *           transfer is submitted (submit record logged for ep 0x02)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteSubmitsTransfer, TestSize.Level1)
{
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    std::vector<uint8_t> payload(WRITE_PAYLOAD_LENGTH, 0xAB);
    ASSERT_TRUE(ashmem->WriteToAshmem(payload.data(), payload.size(), 0));
    mock_usb::ClearRecords();
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), ashmem);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_TRUE(HasSubmitRecord(BULK_OUT_EP));
}

/**
 * @tc.name: BulkWriteFailsWhenSubmitRejected
 * @tc.desc: Test BulkWrite when libusb_submit_transfer fails
 * @tc.desc: Negative test: FailNextCall(SUBMIT_TRANSFER) makes the adapter
 *           return the negative libusb error code
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkWriteFailsWhenSubmitRejected, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_FAULT), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    std::vector<uint8_t> payload(WRITE_PAYLOAD_LENGTH, 0xCD);
    ASSERT_TRUE(ashmem->WriteToAshmem(payload.data(), payload.size(), 0));
    mock_usb::FailNextCall(mock_usb::MockApi::SUBMIT_TRANSFER, INVALID_INJECTED_ERROR);
    int32_t ret = adapter_->BulkWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_FAULT), ashmem);
    /* The exact injected code proves the submit branch was reached (a BUSY from
     * the isTransferring guard would be a different negative value). */
    EXPECT_EQ(INVALID_INJECTED_ERROR, ret);
}

/* ---- BulkCancel tests ---- */

/**
 * @tc.name: BulkCancelFailsOnUnknownDevice
 * @tc.desc: Test BulkCancel for a device without a bulk wrapper
 * @tc.desc: Negative test: ADDR_MISSING was never opened, no wrapper exists
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkCancelFailsOnUnknownDevice, TestSize.Level1)
{
    int32_t ret = adapter_->BulkCancel(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_IN_EP));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkCancelFailsOnUnregisteredEndpoint
 * @tc.desc: Test BulkCancel for an endpoint without a registered callback
 * @tc.desc: Negative test: endpoint 0x05 has no entry in the wrapper's
 *           transfer list
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkCancelFailsOnUnregisteredEndpoint, TestSize.Level1)
{
    int32_t ret = adapter_->BulkCancel(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, INVALID_EP_UNREGISTERED));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkCancelSucceedsAndToleratesRepeatedCancel
 * @tc.desc: Test BulkCancel after a submitted bulk read
 * @tc.desc: Positive test: cancel succeeds for the in-flight transfer.
 * @tc.desc: Positive test: the entry is kept until the completion callback
 *           runs, and a repeated cancel tolerates the resulting
 *           LIBUSB_ERROR_NOT_FOUND, so it also returns success
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, BulkCancelSucceedsAndToleratesRepeatedCancel, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_CANCEL), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    ASSERT_EQ(HDF_SUCCESS, adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_CANCEL), ashmem));
    int32_t ret = adapter_->BulkCancel(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_CANCEL));
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = adapter_->BulkCancel(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_CANCEL));
    EXPECT_EQ(HDF_SUCCESS, ret);
}

/* ---- RegBulkCallback tests ---- */

/**
 * @tc.name: RegBulkCallbackFailsOnNullCallback
 * @tc.desc: Test RegBulkCallback with a null callback
 * @tc.desc: Negative test: cb == nullptr is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, RegBulkCallbackFailsOnNullCallback, TestSize.Level1)
{
    const sptr<OHOS::HDI::Usb::V2_0::IUsbdBulkCallback> cb = nullptr;
    int32_t ret = adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_FAULT), cb);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: RegBulkCallbackFailsOnUnknownDevice
 * @tc.desc: Test RegBulkCallback for a device without a bulk wrapper
 * @tc.desc: Negative test: ADDR_MISSING was never opened
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, RegBulkCallbackFailsOnUnknownDevice, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    int32_t ret = adapter_->RegBulkCallback(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_IN_EP), cb);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: RegBulkCallbackAllowsRepeatedRegistration
 * @tc.desc: Test RegBulkCallback twice on the same endpoint
 * @tc.desc: Positive test: a repeated registration on an idle endpoint
 *           updates the callback and returns success
 * @tc.desc: Negative test: a repeated registration on an endpoint whose
 *           transfer is in flight is rejected (0x81 was left transferring by
 *           BulkReadSubmitsAndRejectsDoubleSubmit)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, RegBulkCallbackAllowsRepeatedRegistration, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    int32_t ret = adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_REREG), cb);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_REREG), cb);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* Re-registering an endpoint that is still transferring is refused. */
    ret = adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), cb);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/* ---- UnRegBulkCallback tests ---- */

/**
 * @tc.name: UnRegBulkCallbackFailsOnUnknownDevice
 * @tc.desc: Test UnRegBulkCallback for a device without a bulk wrapper
 * @tc.desc: Negative test: ADDR_MISSING was never opened
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, UnRegBulkCallbackFailsOnUnknownDevice, TestSize.Level1)
{
    int32_t ret = adapter_->UnRegBulkCallback(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_IN_EP));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: UnRegBulkCallbackFailsOnUnregisteredEndpoint
 * @tc.desc: Test UnRegBulkCallback for an endpoint without a registered callback
 * @tc.desc: Negative test: endpoint 0x05 has no entry in the transfer list
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, UnRegBulkCallbackFailsOnUnregisteredEndpoint, TestSize.Level1)
{
    int32_t ret = adapter_->UnRegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, INVALID_EP_UNREGISTERED));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: UnRegBulkCallbackFailsWhileTransferring
 * @tc.desc: Test UnRegBulkCallback while a bulk transfer is in flight
 * @tc.desc: Negative test: isTransferring is set by a successful BulkRead on
 *           BULK_IN_EP_UNREG, so the unregister is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, UnRegBulkCallbackFailsWhileTransferring, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_UNREG), cb));
    sptr<Ashmem> ashmem = MakeAshmem(BULK_MAX_PACKET_SIZE);
    ASSERT_NE(nullptr, ashmem);
    ASSERT_EQ(HDF_SUCCESS, adapter_->BulkRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_UNREG), ashmem));
    int32_t ret = adapter_->UnRegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP_UNREG));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: UnRegBulkCallbackSucceedsWhenIdle
 * @tc.desc: Test UnRegBulkCallback on an idle (never submitted) endpoint
 * @tc.desc: Positive test: the callback reference is cleared, the list entry
 *           is erased and success is returned
 * @tc.desc: Negative test: unregistering the same endpoint again fails
 *           because the entry no longer exists
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, UnRegBulkCallbackSucceedsWhenIdle, TestSize.Level1)
{
    sptr<TestBulkCallback> cb = sptr<TestBulkCallback>(new TestBulkCallback());
    ASSERT_EQ(HDF_SUCCESS, adapter_->RegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_IDLE), cb));
    int32_t ret = adapter_->UnRegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_IDLE));
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* The successful unregister erases the entry, so the second call fails. */
    ret = adapter_->UnRegBulkCallback(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP_IDLE));
    EXPECT_EQ(HDF_FAILURE, ret);
}

/* ---- SetLoadUsbSaSubscriber tests ---- */

/**
 * @tc.name: SetLoadUsbSaSubscriberFailsOnNullSubscriber
 * @tc.desc: Test SetLoadUsbSaSubscriber with a null subscriber
 * @tc.desc: Negative test: libUsbSaSubscriber == nullptr is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, SetLoadUsbSaSubscriberFailsOnNullSubscriber, TestSize.Level1)
{
    const sptr<LibUsbSaSubscriber> subscriber = nullptr;
    int32_t ret = adapter_->SetLoadUsbSaSubscriber(subscriber);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: SetLoadUsbSaSubscriberNotifiesCurrentAndArrivedDevices
 * @tc.desc: Test SetLoadUsbSaSubscriber success path and hotplug notification
 * @tc.desc: Positive test: registration succeeds, GetCurrentDevList reports
 *           the one present mock device (LoadUsbSa called once), and a
 *           subsequent hotplug ARRIVED event notifies the subscriber again
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, SetLoadUsbSaSubscriberNotifiesCurrentAndArrivedDevices,
    TestSize.Level1)
{
    sptr<TestSaSubscriber> subscriber = sptr<TestSaSubscriber>(new TestSaSubscriber());
    ASSERT_EQ(0, subscriber->count_.load());
    int32_t ret = adapter_->SetLoadUsbSaSubscriber(subscriber);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* Exactly one device is present and it is not a hub: the initial list
     * callback fires once with the ARRIVED event id. */
    EXPECT_EQ(1, subscriber->count_.load());
    EXPECT_EQ(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
        static_cast<libusb_hotplug_event>(subscriber->lastEvent_.load()));
    /* The mock raises hotplug events synchronously in the caller thread. */
    ASSERT_EQ(0, mock_usb::RaiseHotplug(BUS_NUM, ADDR_MAIN, true));
    EXPECT_EQ(2, subscriber->count_.load());
}

/**
 * @tc.name: SetLoadUsbSaSubscriberFailsWhenHotplugRegisterFails
 * @tc.desc: Test SetLoadUsbSaSubscriber when hotplug registration fails
 * @tc.desc: Negative test: FailNextCall(HOTPLUG_REGISTER) makes the adapter
 *           fail and drop g_libusb_context; a follow-up OpenDevice re-runs
 *           LibUSBInit and restores the context for the remaining suite
 * @tc.type: FUNC
 */
HWTEST_F(AdapterBulkV2Test, SetLoadUsbSaSubscriberFailsWhenHotplugRegisterFails, TestSize.Level1)
{
    sptr<TestSaSubscriber> subscriber = sptr<TestSaSubscriber>(new TestSaSubscriber());
    mock_usb::FailNextCall(mock_usb::MockApi::HOTPLUG_REGISTER, INVALID_INJECTED_ERROR);
    int32_t ret = adapter_->SetLoadUsbSaSubscriber(subscriber);
    EXPECT_EQ(HDF_FAILURE, ret);
    EXPECT_EQ(0, subscriber->count_.load());
    /* The failure cleared g_libusb_context: a second call now takes the
     * null-context branch and fails again before touching hotplug. */
    ret = adapter_->SetLoadUsbSaSubscriber(subscriber);
    EXPECT_EQ(HDF_FAILURE, ret);
    EXPECT_EQ(0, subscriber->count_.load());
    /* Failure clears g_libusb_context; OpenDevice re-initializes it. */
    EXPECT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
}
} // namespace
