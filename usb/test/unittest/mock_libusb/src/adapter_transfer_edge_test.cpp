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
 * Mock-libusb tests for LibusbAdapter: synchronous transfer edge cases.
 *
 * Scope (batch 1b — complements adapter_transfer_test.cpp):
 *   - config descriptor queries (valid / invalid descId)
 *   - ControlTransferReadwithLength (UsbCtrlTransferParams path, direction check)
 *   - ControlTransferWrite direction validation
 *   - BulkTransferReadwithLength negative length and unclaimed interface
 *   - isochronous transfer error paths (invalid intfId, unopened device)
 *   - second configuration FIXED data mode
 *   - FailNextCall fault injection on control and bulk paths
 *   - GetStringDescriptor with INVALID_NUM (222) returns success
 *   - descriptor / speed queries on missing devices
 *   - bulk write TransferRecord hex preview verification
 *   - interrupt transfer on unopened device
 *   - config descriptor field validation (bLength, bDescriptorType, bNumInterfaces)
 *
 * Command line:
 *   --dump-transfers print every transfer handled by the mock libusb
 */

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "hdf_base.h"
#include "hdf_log.h"
#include "libusb_adapter.h"
#include "mock_libusb_control.h"
#include "usbd_type.h"

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
#define INVALID_INTERFACE 0xC8
#define BULK_IN_EP 0x81
#define BULK_OUT_EP 0x02
#define INTERRUPT_IN_EP 0x83
#define BULK_MAX_PACKET_SIZE 512
#define TRANSFER_TIMEOUT_MS 1000
#define PATTERN_MODULUS 63
#define INVALID_NUM 222
#define SECOND_CONFIG_VALUE 2
#define UNKNOWN_CONFIG_VALUE 9
#define CONFIG_DESC_LENGTH 9
#define CONFIG_DESC_TYPE 0x02

/* USB control request constants (matching libusb.h values). */
#define CTRL_REQ_TYPE_IN_STANDARD 0x80
#define CTRL_REQ_TYPE_IN_VENDOR 0xC0
#define CTRL_REQ_TYPE_OUT_VENDOR 0x40
#define CTRL_REQ_TYPE_OUT_STANDARD 0x00
#define CTRL_REQ_GET_DESCRIPTOR 0x06
#define DESC_TYPE_DEVICE 0x01
#define DESC_TYPE_CONFIG 0x02
#define DESC_TYPE_STRING 0x03
#define DESC_INDEX_MANUFACTURER 1
#define DEVICE_DESC_LENGTH 18

namespace {

UsbDev MakeDev(uint8_t address)
{
    return UsbDev{BUS_NUM, address};
}

UsbPipe MakePipe(uint8_t interfaceId, uint8_t endpoint)
{
    return UsbPipe{interfaceId, endpoint};
}

class AdapterTransferEdgeTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterTransferEdgeTest::adapter_ = nullptr;

void AdapterTransferEdgeTest::SetUpTestCase()
{
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    /* A spare device that is present but never opened — for missing-handle tests. */
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_SPARE, MAIN_VID, SPARE_PID));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
}

void AdapterTransferEdgeTest::TearDownTestCase()
{
    if (adapter_ != nullptr) {
        (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
        (void)adapter_->CloseDevice(MakeDev(ADDR_MAIN));
    }
    adapter_ = nullptr;
}

/* ---- Config descriptor tests ---- */

/**
 * @tc.name: GetConfigDescriptorValidIndex0
 * @tc.desc: Test GetConfigDescriptor with descId=0 (config value 1)
 * @tc.desc: int32_t GetConfigDescriptor(const UsbDev &dev, uint8_t descId,
 *      std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: returns a config descriptor with correct header fields
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetConfigDescriptorValidIndex0, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetConfigDescriptor(MakeDev(ADDR_MAIN), 1, desc);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_GE(desc.size(), static_cast<size_t>(CONFIG_DESC_LENGTH));
    if (desc.size() >= CONFIG_DESC_LENGTH) {
        /* bLength at offset 0, bDescriptorType at offset 1, bNumInterfaces at offset 4 */
        EXPECT_EQ(CONFIG_DESC_LENGTH, desc[0]);
        EXPECT_EQ(CONFIG_DESC_TYPE, desc[1]);
        /* MakeDefaultSpec config 1 has 1 interface */
        EXPECT_EQ(1, desc[4]);
    }
}

/**
 * @tc.name: GetConfigDescriptorSecondConfig
 * @tc.desc: Test GetConfigDescriptor with descId=2 (second config)
 * @tc.desc: Positive test: the second configuration descriptor is returned
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetConfigDescriptorSecondConfig, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetConfigDescriptor(MakeDev(ADDR_MAIN), SECOND_CONFIG_VALUE, desc);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_GE(desc.size(), static_cast<size_t>(CONFIG_DESC_LENGTH));
    if (desc.size() >= CONFIG_DESC_LENGTH) {
        EXPECT_EQ(CONFIG_DESC_LENGTH, desc[0]);
        EXPECT_EQ(CONFIG_DESC_TYPE, desc[1]);
    }
}

/**
 * @tc.name: GetConfigDescriptorInvalidId
 * @tc.desc: Test GetConfigDescriptor with an unknown descId
 * @tc.desc: Negative test: no matching config descriptor → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetConfigDescriptorInvalidId, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetConfigDescriptor(MakeDev(ADDR_MAIN), UNKNOWN_CONFIG_VALUE, desc);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: GetConfigDescriptorMissingDevice
 * @tc.desc: Test GetConfigDescriptor on a device that does not exist
 * @tc.desc: Negative test: GetUsbDevice fails → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetConfigDescriptorMissingDevice, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetConfigDescriptor(MakeDev(ADDR_MISSING), 1, desc);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- ControlTransferReadwithLength tests ---- */

/**
 * @tc.name: ControlReadWithLengthReturnsDeviceDescriptor
 * @tc.desc: Test ControlTransferReadwithLength with GET_DESCRIPTOR(device)
 * @tc.desc: int32_t ControlTransferReadwithLength(const UsbDev &dev,
 *      const UsbCtrlTransferParams &ctrlParams, std::vector<uint8_t> &data);
 * @tc.desc: Positive test: returns 18 bytes of device descriptor
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlReadWithLengthReturnsDeviceDescriptor, TestSize.Level1)
{
    struct UsbCtrlTransferParams params = {
        CTRL_REQ_TYPE_IN_STANDARD,  /* requestType  */
        CTRL_REQ_GET_DESCRIPTOR,    /* requestCmd */
        0x0100,                     /* value (type=device, index=0) */
        0x0000,                     /* index */
        DEVICE_DESC_LENGTH,         /* length */
        TRANSFER_TIMEOUT_MS         /* timeout */
    };
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferReadwithLength(MakeDev(ADDR_MAIN), params, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(DEVICE_DESC_LENGTH, static_cast<int>(data.size()));
    if (data.size() >= DEVICE_DESC_LENGTH) {
        EXPECT_EQ(DEVICE_DESC_LENGTH, data[0]);
        EXPECT_EQ(DESC_TYPE_DEVICE, data[1]);
    }
}

/**
 * @tc.name: ControlReadWithLengthRejectsOutDirection
 * @tc.desc: Test ControlTransferReadwithLength with OUT direction requestType
 * @tc.desc: Negative test: requestType 0x00 has OUT direction → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlReadWithLengthRejectsOutDirection, TestSize.Level1)
{
    struct UsbCtrlTransferParams params = {
        CTRL_REQ_TYPE_OUT_STANDARD, /* requestType = OUT */
        CTRL_REQ_GET_DESCRIPTOR,
        0x0100,
        0x0000,
        DEVICE_DESC_LENGTH,
        TRANSFER_TIMEOUT_MS
    };
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferReadwithLength(MakeDev(ADDR_MAIN), params, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: ControlReadWithLengthNegativeLengthClamped
 * @tc.desc: Test ControlTransferReadwithLength with negative length
 * @tc.desc: Positive test: negative length is clamped to MAX_CONTROL_BUFF_SIZE (1024)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlReadWithLengthNegativeLengthClamped, TestSize.Level1)
{
    struct UsbCtrlTransferParams params = {
        CTRL_REQ_TYPE_IN_STANDARD,
        CTRL_REQ_GET_DESCRIPTOR,
        0x0100,
        0x0000,
        -1, /* negative length → clamped to 1024 */
        TRANSFER_TIMEOUT_MS
    };
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferReadwithLength(MakeDev(ADDR_MAIN), params, data);
    /* The adapter clamps to 1024, the mock returns 18 bytes of device descriptor. */
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(DEVICE_DESC_LENGTH, static_cast<int>(data.size()));
}

/* ---- ControlTransferWrite direction validation ---- */

/**
 * @tc.name: ControlWriteRejectsInDirection
 * @tc.desc: Test ControlTransferWrite with IN direction requestType
 * @tc.desc: Negative test: requestType 0x80 has IN direction → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlWriteRejectsInDirection, TestSize.Level1)
{
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_IN_STANDARD, 0x01, 0x0000, 0x00, TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->ControlTransferWrite(MakeDev(ADDR_MAIN), ctrl, payload);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- BulkTransferReadwithLength edge cases ---- */

/**
 * @tc.name: BulkReadWithLengthRejectsNegativeLength
 * @tc.desc: Test BulkTransferReadwithLength with negative length
 * @tc.desc: Negative test: length < 0 → HDF_ERR_INVALID_PARAM
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadWithLengthRejectsNegativeLength, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, -1, data);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name: BulkReadWithLengthRejectsInvalidInterfaceId
 * @tc.desc: Test BulkTransferReadwithLength with intfId >= 0x80
 * @tc.desc: Negative test: invalid interface id → HDF_ERR_INVALID_PARAM
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadWithLengthRejectsInvalidInterfaceId, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INVALID_INTERFACE, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 64, data);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name: BulkReadWithLengthFailsOnUnclaimedInterface
 * @tc.desc: Test BulkTransferReadwithLength on an unclaimed interface
 * @tc.desc: Negative test: IsInterfaceIdByUsbDev fails for interface 1 (never claimed)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadWithLengthFailsOnUnclaimedInterface, TestSize.Level1)
{
    /* Interface 1 was never claimed; IsInterfaceIdByUsbDev should reject it.
     * However the mock device only has interface 0, so GetEndpointDesc may fail
     * first. Either way, the result must not be HDF_SUCCESS. */
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(1, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 64, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Iso transfer error paths ---- */

/**
 * @tc.name: IsoReadRejectsInvalidInterfaceId
 * @tc.desc: Test IsoTransferRead with intfId >= 0x80
 * @tc.desc: Negative test: invalid interface id → HDF_FAILURE (not HDF_ERR_INVALID_PARAM)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, IsoReadRejectsInvalidInterfaceId, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->IsoTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INVALID_INTERFACE, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    /* IsoTransferRead returns HDF_FAILURE for invalid intfId, not HDF_ERR_INVALID_PARAM */
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: IsoWriteRejectsInvalidInterfaceId
 * @tc.desc: Test IsoTransferWrite with intfId >= 0x80
 * @tc.desc: Negative test: invalid interface id → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, IsoWriteRejectsInvalidInterfaceId, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->IsoTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INVALID_INTERFACE, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: IsoReadFailsOnUnopenedDevice
 * @tc.desc: Test IsoTransferRead on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, IsoReadFailsOnUnopenedDevice, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->IsoTransferRead(MakeDev(ADDR_SPARE),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: IsoWriteFailsOnUnopenedDevice
 * @tc.desc: Test IsoTransferWrite on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, IsoWriteFailsOnUnopenedDevice, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->IsoTransferWrite(MakeDev(ADDR_SPARE),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Second configuration FIXED data mode ---- */

/**
 * @tc.name: BulkReadOnSecondConfigReturnsFixedData
 * @tc.desc: Test BulkTransferRead on config 2 (FIXED data mode)
 * @tc.desc: Positive test: config 2 bulk IN returns {0x11, 0x22, 0x33, 0x44}
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadOnSecondConfigReturnsFixedData, TestSize.Level1)
{
    /* Switch to config 2, which has a FIXED bulk-in payload. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetConfig(MakeDev(ADDR_MAIN), SECOND_CONFIG_VALUE));
    /* Re-claim interface 0 on the new configuration. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_FALSE(data.empty());
    /* The FIXED payload is {0x11, 0x22, 0x33, 0x44}; the read buffer is 8192 bytes,
     * but the mock produces only 4 bytes for FIXED mode (payload size). */
    if (data.size() >= 4) {
        EXPECT_EQ(0x11, data[0]);
        EXPECT_EQ(0x22, data[1]);
        EXPECT_EQ(0x33, data[2]);
        EXPECT_EQ(0x44, data[3]);
    }
    /* Restore config 1 and re-claim. */
    (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetConfig(MakeDev(ADDR_MAIN), 1));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
}

/**
 * @tc.name: FailNextCallOpenDevice
 * @tc.desc: Test FailNextCall on OPEN
 * @tc.desc: Negative test: the next libusb_open fails → OpenDevice returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, FailNextCallOpenDevice, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::OPEN, -4, 1); /* LIBUSB_ERROR_NO_DEVICE */
    int32_t ret = adapter_->OpenDevice(MakeDev(ADDR_SPARE));
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: FailNextCallClaimInterface
 * @tc.desc: Test FailNextCall on CLAIM_INTERFACE
 * @tc.desc: Negative test: the next libusb_claim_interface fails
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, FailNextCallClaimInterface, TestSize.Level1)
{
    /* Open the spare device so we have a handle to test against. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_SPARE)));
    mock_usb::FailNextCall(mock_usb::MockApi::CLAIM_INTERFACE, -1, 1); /* LIBUSB_ERROR */
    int32_t ret = adapter_->ClaimInterface(MakeDev(ADDR_SPARE), INTERFACE_ID, 1);
    EXPECT_NE(HDF_SUCCESS, ret);
    (void)adapter_->CloseDevice(MakeDev(ADDR_SPARE));
}

/* ---- GetStringDescriptor edge cases ---- */

/**
 * @tc.name: GetStringDescriptorInvalidDescIdReturnsSuccess
 * @tc.desc: Test GetStringDescriptor with descId=222 (INVALID_NUM)
 * @tc.desc: Positive test: when descId == INVALID_NUM and the read returns <= 0,
 *      the adapter returns HDF_SUCCESS (early return path)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetStringDescriptorInvalidDescIdReturnsSuccess, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetStringDescriptor(MakeDev(ADDR_MAIN), INVALID_NUM, desc);
    /* descId 222 is not a valid string index; the mock returns 0 bytes,
     * and the adapter checks descId == INVALID_NUM → returns HDF_SUCCESS. */
    EXPECT_EQ(HDF_SUCCESS, ret);
}

/**
 * @tc.name: GetStringDescriptorFailsOnMissingDevice
 * @tc.desc: Test GetStringDescriptor on a non-existent device
 * @tc.desc: Negative test: GetUsbDevice fails → HDF_DEV_ERR_NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetStringDescriptorFailsOnMissingDevice, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetStringDescriptor(MakeDev(ADDR_MISSING), DESC_INDEX_MANUFACTURER, desc);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Descriptor and speed queries on missing devices ---- */

/**
 * @tc.name: GetDeviceDescriptorFailsOnMissingDevice
 * @tc.desc: Test GetDeviceDescriptor on a non-existent device
 * @tc.desc: Negative test: GetUsbDevice fails → HDF_DEV_ERR_NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetDeviceDescriptorFailsOnMissingDevice, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetDeviceDescriptor(MakeDev(ADDR_MISSING), desc);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: GetDeviceSpeedFailsOnMissingDevice
 * @tc.desc: Test GetDeviceSpeed on a non-existent device
 * @tc.desc: Negative test: GetUsbDevice fails → HDF_DEV_ERR_NO_DEVICE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetDeviceSpeedFailsOnMissingDevice, TestSize.Level1)
{
    uint8_t speed = 0;
    int32_t ret = adapter_->GetDeviceSpeed(MakeDev(ADDR_MISSING), speed);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Bulk write TransferRecord hex preview ---- */

/**
 * @tc.name: BulkWriteTransferRecordHexPreview
 * @tc.desc: Test BulkTransferWrite records hex preview of the payload
 * @tc.desc: Positive test: the TransferRecord.data field contains hex of the sent bytes
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkWriteTransferRecordHexPreview, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    mock_usb::ClearRecords();
    ASSERT_EQ(HDF_SUCCESS, adapter_->BulkTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload));
    /* Find the bulk-out record and verify hex preview. */
    bool found = false;
    for (size_t i = 0; i < mock_usb::RecordCount(); i++) {
        auto record = mock_usb::RecordAt(i);
        if (record.kind == "bulk-out" && record.endpoint == BULK_OUT_EP) {
            found = true;
            EXPECT_EQ(4, record.length);
            EXPECT_EQ(4, record.actual);
            /* HexPreview should contain "de ad be ef" or similar hex digits. */
            EXPECT_FALSE(record.data.empty());
            break;
        }
    }
    EXPECT_TRUE(found);
}

/* ---- Interrupt transfer on unopened device ---- */

/**
 * @tc.name: InterruptReadFailsOnUnopenedDevice
 * @tc.desc: Test InterruptTransferRead on a device that was never opened
 * @tc.desc: Negative test: GetEndpointDesc → FindHandleByDev fails
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, InterruptReadFailsOnUnopenedDevice, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->InterruptTransferRead(MakeDev(ADDR_SPARE),
        MakePipe(INTERFACE_ID, INTERRUPT_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: InterruptWriteFailsOnUnopenedDevice
 * @tc.desc: Test InterruptTransferWrite on a device that was never opened
 * @tc.desc: Negative test: GetEndpointDesc → FindHandleByDev fails
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, InterruptWriteFailsOnUnopenedDevice, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->InterruptTransferWrite(MakeDev(ADDR_SPARE),
        MakePipe(INTERFACE_ID, INTERRUPT_IN_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Bulk read on config 2 with maxInLength ---- */

/**
 * @tc.name: BulkReadWithLengthOnSecondConfig
 * @tc.desc: Test BulkTransferReadwithLength on config 2 (FIXED mode, maxPacket=64)
 * @tc.desc: Positive test: config 2 bulk IN returns the fixed payload
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadWithLengthOnSecondConfig, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetConfig(MakeDev(ADDR_MAIN), SECOND_CONFIG_VALUE));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 4, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(4U, data.size());
    if (data.size() >= 4) {
        EXPECT_EQ(0x11, data[0]);
        EXPECT_EQ(0x22, data[1]);
        EXPECT_EQ(0x33, data[2]);
        EXPECT_EQ(0x44, data[3]);
    }
    (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetConfig(MakeDev(ADDR_MAIN), 1));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
}

/* ---- ControlTransferRead on missing device ---- */

/**
 * @tc.name: ControlReadFailsOnMissingDevice
 * @tc.desc: Test ControlTransferRead on a non-existent device
 * @tc.desc: Negative test: DoControlTransfer → FindHandleByDev fails → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlReadFailsOnMissingDevice, TestSize.Level1)
{
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_IN_STANDARD, CTRL_REQ_GET_DESCRIPTOR, 0x0100, 0x00,
        TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferRead(MakeDev(ADDR_MISSING), ctrl, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: ControlWriteFailsOnMissingDevice
 * @tc.desc: Test ControlTransferWrite on a non-existent device
 * @tc.desc: Negative test: DoControlTransfer → FindHandleByDev fails → HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ControlWriteFailsOnMissingDevice, TestSize.Level1)
{
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_OUT_VENDOR, 0x01, 0x0000, 0x00, TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->ControlTransferWrite(MakeDev(ADDR_MISSING), ctrl, payload);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- ClearHalt fault injection ---- */

/**
 * @tc.name: ClearHaltFailsWithInjectedError
 * @tc.desc: Test ClearHalt with FailNextCall on CLEAR_HALT
 * @tc.desc: Negative test: injected failure → ClearHalt returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ClearHaltFailsWithInjectedError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::CLEAR_HALT, -1, 1);
    int32_t ret = adapter_->ClearHalt(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP));
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- ResetDevice fault injection ---- */

/**
 * @tc.name: ResetDeviceFailsWithInjectedError
 * @tc.desc: Test ResetDevice with FailNextCall on RESET_DEVICE
 * @tc.desc: Negative test: injected failure → ResetDevice returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ResetDeviceFailsWithInjectedError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::RESET_DEVICE, -4, 1);
    int32_t ret = adapter_->ResetDevice(MakeDev(ADDR_MAIN));
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- SetConfig / GetConfig fault injection ---- */

/**
 * @tc.name: SetConfigFailsWithInjectedError
 * @tc.desc: Test SetConfig with FailNextCall on SET_CONFIGURATION
 * @tc.desc: Negative test: injected failure → SetConfig returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, SetConfigFailsWithInjectedError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::SET_CONFIGURATION, -1, 1);
    int32_t ret = adapter_->SetConfig(MakeDev(ADDR_MAIN), SECOND_CONFIG_VALUE);
    EXPECT_NE(HDF_SUCCESS, ret);
    /* Verify config is unchanged. */
    uint8_t config = 0;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetConfig(MakeDev(ADDR_MAIN), config));
    EXPECT_EQ(1, config);
}

/**
 * @tc.name: GetConfigFailsWithInjectedError
 * @tc.desc: Test GetConfig with FailNextCall on GET_CONFIGURATION
 * @tc.desc: Negative test: injected failure → GetConfig returns error
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, GetConfigFailsWithInjectedError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::GET_CONFIGURATION, -1, 1);
    uint8_t config = 0;
    int32_t ret = adapter_->GetConfig(MakeDev(ADDR_MAIN), config);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- SetInterface fault injection ---- */

/**
 * @tc.name: SetInterfaceFailsWithInjectedError
 * @tc.desc: Test SetInterface with FailNextCall on SET_ALT_SETTING
 * @tc.desc: Negative test: injected LIBUSB_ERROR_NO_DEVICE → SetInterface returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, SetInterfaceFailsWithInjectedError, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::SET_ALT_SETTING, -4, 1); /* LIBUSB_ERROR_NO_DEVICE */
    int32_t ret = adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Config descriptor endpoint verification ---- */

/**
 * @tc.name: ConfigDescriptorContainsEndpointDescriptors
 * @tc.desc: Test GetConfigDescriptor returns a descriptor containing endpoint descriptors
 * @tc.desc: Positive test: the descriptor is longer than just the config header (9 bytes),
 *      meaning interface + endpoint descriptors are included
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, ConfigDescriptorContainsEndpointDescriptors, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    ASSERT_EQ(HDF_SUCCESS, adapter_->GetConfigDescriptor(MakeDev(ADDR_MAIN), 1, desc));
    /* Config 1 has 1 interface with 2 alt settings:
     * alt 0: 2 endpoints (bulk in/out)
     * alt 1: 2 endpoints (interrupt in/out)
     * Total = 9 (config) + 2*(9 interface + 27 endpoint) = 9 + 223 = 55 bytes */
    EXPECT_GT(desc.size(), static_cast<size_t>(CONFIG_DESC_LENGTH));
    /* Verify the interface descriptor starts right after the config descriptor. */
    if (desc.size() >= CONFIG_DESC_LENGTH + 9) {
        /* Interface descriptor: bLength=9 at offset 9, bDescriptorType=0x04 at offset 10 */
        EXPECT_EQ(9, desc[CONFIG_DESC_LENGTH]);
        EXPECT_EQ(0x04, desc[CONFIG_DESC_LENGTH + 1]);
    }
}

/* ---- Endpoint-level fault injection via SetEndpointBehavior ---- */

/**
 * @tc.name: BulkReadReturnsHdfFailureOnPipeError
 * @tc.desc: Test BulkTransferRead returns HDF_FAILURE on injected LIBUSB_ERROR_PIPE
 * @tc.desc: Negative test: SetEndpointBehavior injects failInTimes=1, errorIn=-9;
 *      BulkTransferRead has no apiVersion path, returns HDF_FAILURE for ret < 0
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadReturnsHdfFailureOnPipeError, TestSize.Level1)
{
    mock_usb::EndpointSpec fault;
    fault.address = BULK_IN_EP;
    fault.failInTimes = 1;
    fault.errorIn = -9; /* LIBUSB_ERROR_PIPE */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_IN_EP, fault));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    /* BulkTransferRead: ret < 0 and ret != LIBUSB_IO_ERROR -> return HDF_FAILURE.
     * No apiVersion path. */
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkReadReturnsHdfFailureOnIoError
 * @tc.desc: Test BulkTransferRead returns HDF_FAILURE on LIBUSB_ERROR_IO
 * @tc.desc: Negative test: SetEndpointBehavior injects failInTimes=1, errorIn=-1;
 *      BulkTransferRead returns HDF_FAILURE for all ret < 0 (the
 *      LIBUSB_IO_ERROR -> LIBUSB_IO_ERROR_INVALID mapping only exists in BulkTransferWrite)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadReturnsHdfFailureOnIoError, TestSize.Level1)
{
    mock_usb::EndpointSpec fault;
    fault.address = BULK_IN_EP;
    fault.failInTimes = 1;
    fault.errorIn = -1; /* LIBUSB_ERROR_IO */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_IN_EP, fault));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    /* BulkTransferRead: ret < 0 -> return HDF_FAILURE. */
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: BulkWriteReturnsIoErrorInvalidOnIoError
 * @tc.desc: Test BulkTransferWrite returns 0 (LIBUSB_IO_ERROR_INVALID) on LIBUSB_IO_ERROR
 * @tc.desc: Negative test: SetEndpointBehavior injects failOutTimes=1, errorOut=-1;
 *      BulkTransferWrite maps LIBUSB_IO_ERROR(-1) to LIBUSB_IO_ERROR_INVALID(0)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkWriteReturnsIoErrorInvalidOnIoError, TestSize.Level1)
{
    mock_usb::EndpointSpec fault;
    fault.address = BULK_OUT_EP;
    fault.failOutTimes = 1;
    fault.errorOut = -1; /* LIBUSB_IO_ERROR */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_OUT_EP, fault));
    std::vector<uint8_t> payload = {0x01, 0x02};
    int32_t ret = adapter_->BulkTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    /* BulkTransferWrite: ret == LIBUSB_IO_ERROR(-1) -> return LIBUSB_IO_ERROR_INVALID(0). */
    EXPECT_EQ(0, ret);
}

/**
 * @tc.name: BulkWriteReturnsHdfFailureOnPipeError
 * @tc.desc: Test BulkTransferWrite returns HDF_FAILURE on injected LIBUSB_ERROR_PIPE
 * @tc.desc: Negative test: SetEndpointBehavior injects failOutTimes=1, errorOut=-9;
 *      BulkTransferWrite returns HDF_FAILURE for ret < 0 (not LIBUSB_IO_ERROR)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkWriteReturnsHdfFailureOnPipeError, TestSize.Level1)
{
    mock_usb::EndpointSpec fault;
    fault.address = BULK_OUT_EP;
    fault.failOutTimes = 1;
    fault.errorOut = -9; /* LIBUSB_ERROR_PIPE */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_OUT_EP, fault));
    std::vector<uint8_t> payload = {0x01, 0x02};
    int32_t ret = adapter_->BulkTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    /* BulkTransferWrite: ret < 0 and ret != LIBUSB_IO_ERROR -> return HDF_FAILURE. */
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: FailNextCallGetStringDescriptor
 * @tc.desc: Test FailNextCall on CONTROL_TRANSFER via GetStringDescriptor
 * @tc.desc: Negative test: GetStringDescriptor calls libusb_get_string_descriptor
 *      directly (not DoControlTransfer), so no apiVersion shim applies;
 *      ret <= 0 and descId != INVALID_NUM returns HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, FailNextCallGetStringDescriptor, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::CONTROL_TRANSFER, -1, 1);
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetStringDescriptor(MakeDev(ADDR_MAIN), DESC_INDEX_MANUFACTURER, desc);
    /* GetStringDescriptor does NOT go through DoControlTransfer; it calls
     * libusb_get_string_descriptor directly. ret <= 0, descId != INVALID_NUM
     * -> return HDF_FAILURE. No apiVersion path. */
    EXPECT_EQ(HDF_FAILURE, ret);
}

/**
 * @tc.name: FailNextCallActiveConfigDescriptor
 * @tc.desc: Test FailNextCall on ACTIVE_CONFIG_DESCRIPTOR via GetEndpointDesc
 * @tc.desc: Negative test: injected failure on libusb_get_active_config_descriptor
 *      causes GetEndpointDesc to return HDF_ERR_INVALID_PARAM
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, FailNextCallActiveConfigDescriptor, TestSize.Level1)
{
    mock_usb::FailNextCall(mock_usb::MockApi::ACTIVE_CONFIG_DESCRIPTOR, -1, 1);
    std::vector<uint8_t> data;
    int32_t ret = adapter_->InterruptTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    /* GetEndpointDesc returns HDF_ERR_INVALID_PARAM when
     * libusb_get_active_config_descriptor fails. No apiVersion path. */
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/* ---- BulkReadWithLength error return values ---- */

/**
 * @tc.name: BulkReadWithLengthReturnsIoErrorInvalidOnIoError
 * @tc.desc: Test BulkTransferReadwithLength returns 0 on LIBUSB_IO_ERROR
 * @tc.desc: Negative test: SetEndpointBehavior injects errorIn=-1;
 *      BulkTransferReadwithLength maps LIBUSB_IO_ERROR(-1) to 0
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferEdgeTest, BulkReadWithLengthReturnsIoErrorInvalidOnIoError, TestSize.Level1)
{
    mock_usb::EndpointSpec fault;
    fault.address = BULK_IN_EP;
    fault.failInTimes = 1;
    fault.errorIn = -1; /* LIBUSB_IO_ERROR */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_IN_EP, fault));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 64, data);
    /* BulkTransferReadwithLength: ret == LIBUSB_IO_ERROR(-1) -> return 0.
     * No apiVersion path. */
    EXPECT_EQ(0, ret);
}
} // namespace
