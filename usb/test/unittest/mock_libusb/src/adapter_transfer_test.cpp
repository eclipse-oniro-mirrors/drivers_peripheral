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
 * Mock-libusb tests for LibusbAdapter: synchronous transfer paths.
 *
 * Scope:
 *   - bulk read / read-with-length / write (pattern data, fault injection)
 *   - interrupt read / write (alt setting 1, endpoint type validation)
 *   - control read / write (device descriptor, vendor request, direction check)
 *   - iso read / write (delegates to libusb_bulk_transfer in the adapter)
 *   - descriptor queries (device descriptor, string descriptor, device speed)
 *
 * The tests exercise the real LibusbAdapter source compiled against the mock
 * libusb, so every path through the adapter is verified without USB hardware.
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
#define ADDR_MISSING 9
#define MAIN_VID 0x1209
#define MAIN_PID 0x0001
#define INTERFACE_ID 0
#define INVALID_INTERFACE 0xC8
#define BULK_IN_EP 0x81
#define BULK_OUT_EP 0x02
#define INTERRUPT_IN_EP 0x83
#define INTERRUPT_OUT_EP 0x04
#define BULK_MAX_PACKET_SIZE 512
#define INTERRUPT_MAX_PACKET_SIZE 64
#define TRANSFER_TIMEOUT_MS 1000
#define PATTERN_MODULUS 63
#define INVALID_NUM 222
/* UTF-16LE string descriptor header length in bytes (bLength + bDescriptorType). */
#define STRING_DESC_HEADER_SIZE 2

/* USB control request constants (matching libusb.h values). */
#define CTRL_REQ_TYPE_IN_STANDARD 0x80
#define CTRL_REQ_TYPE_OUT_VENDOR 0x40
#define CTRL_REQ_GET_DESCRIPTOR 0x06
#define CTRL_REQ_TYPE_OUT_STANDARD 0x00
#define DESC_TYPE_DEVICE 0x01
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

/* Verify data[i] == (i % maxPacketSize) % 63, the kernel SourceSink pattern. */
bool IsPatternData(const std::vector<uint8_t> &data, int32_t maxPacketSize)
{
    if (maxPacketSize == 0 || PATTERN_MODULUS == 0) {
        return false;
    }
    for (size_t i = 0; i < data.size(); i++) {
        uint8_t expected = static_cast<uint8_t>(
            (static_cast<int32_t>(i) % maxPacketSize) % PATTERN_MODULUS);
        if (data[i] != expected) {
            return false;
        }
    }
    return true;
}

/* Decode a UTF-16LE string descriptor payload (skipping the 2-byte header).
 * The adapter copies USB_MAX_DESCRIPTOR_SIZE (256) bytes into the descriptor
 * vector, padding with zeros beyond the actual string length, so stop at the
 * first NUL character. */
std::string DecodeUtf16Le(const std::vector<uint8_t> &descriptor)
{
    /* Skip the STRING_DESC_HEADER_SIZE-byte header (bLength + bDescriptorType). */
    if (descriptor.size() < STRING_DESC_HEADER_SIZE) {
        return "";
    }
    std::string result;
    /* Each UTF-16LE code unit is 2 bytes; iterate one code unit at a time. */
    for (size_t i = STRING_DESC_HEADER_SIZE; i + 1 < descriptor.size(); i += STRING_DESC_HEADER_SIZE) {
        char ch = static_cast<char>(descriptor[i]);
        if (ch == '\0') {
            break;
        }
        result += ch;
    }
    return result;
}

class AdapterTransferTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static std::shared_ptr<LibusbAdapter> adapter_;
};

std::shared_ptr<LibusbAdapter> AdapterTransferTest::adapter_ = nullptr;

void AdapterTransferTest::SetUpTestCase()
{
    mock_usb::Reset();
    mock_usb::AddDevice(mock_usb::MakeDefaultSpec(BUS_NUM, ADDR_MAIN, MAIN_VID, MAIN_PID));
    adapter_ = LibusbAdapter::GetInstance();
    ASSERT_NE(nullptr, adapter_);
    /* Open the device and claim interface 0 so transfer paths can proceed. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->OpenDevice(MakeDev(ADDR_MAIN)));
    ASSERT_EQ(HDF_SUCCESS, adapter_->ClaimInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
}

void AdapterTransferTest::TearDownTestCase()
{
    if (adapter_ != nullptr) {
        (void)adapter_->ReleaseInterface(MakeDev(ADDR_MAIN), INTERFACE_ID);
        (void)adapter_->CloseDevice(MakeDev(ADDR_MAIN));
    }
    adapter_ = nullptr;
}

/* ---- Bulk transfer tests ---- */

/**
 * @tc.name: BulkReadReturnsPatternData
 * @tc.desc: Test BulkTransferRead on a bulk IN endpoint
 * @tc.desc: int32_t BulkTransferRead(const UsbDev &dev, const UsbPipe &pipe, int32_t timeout,
 *      std::vector<uint8_t> &data);
 * @tc.desc: Positive test: the mock returns SourceSink pattern data on ep 0x81
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkReadReturnsPatternData, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MAIN),
    MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(IsPatternData(data, BULK_MAX_PACKET_SIZE));
}

/**
 * @tc.name: BulkReadRejectsInvalidInterfaceId
 * @tc.desc: Test BulkTransferRead with an interface id >= 0x80
 * @tc.desc: Negative test: invalid interface id is rejected before touching libusb
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkReadRejectsInvalidInterfaceId, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INVALID_INTERFACE, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name: BulkReadFailsOnUnopenedDevice
 * @tc.desc: Test BulkTransferRead on a device that was never opened
 * @tc.desc: Negative test: FindHandleByDev fails for ADDR_MISSING
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkReadFailsOnUnopenedDevice, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferRead(MakeDev(ADDR_MISSING),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/**
 * @tc.name: BulkWriteSucceeds
 * @tc.desc: Test BulkTransferWrite on a bulk OUT endpoint
 * @tc.desc: int32_t BulkTransferWrite(const UsbDev &dev, const UsbPipe &pipe, int32_t timeout,
 *      const std::vector<uint8_t> &data);
 * @tc.desc: Positive test: a bulk write completes and a bulk-out record is logged
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkWriteSucceeds, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    mock_usb::ClearRecords();
    int32_t ret = adapter_->BulkTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* At least one bulk-out record should exist. */
    bool found = false;
    for (size_t i = 0; i < mock_usb::RecordCount(); i++) {
        auto record = mock_usb::RecordAt(i);
        if (record.kind == "bulk-out" && record.endpoint == BULK_OUT_EP) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

/**
 * @tc.name: BulkWriteRejectsInvalidInterfaceId
 * @tc.desc: Test BulkTransferWrite with an interface id >= 0x80
 * @tc.desc: Negative test: invalid interface id is rejected
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkWriteRejectsInvalidInterfaceId, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x01, 0x02};
    int32_t ret = adapter_->BulkTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INVALID_INTERFACE, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name: BulkReadWithLengthAndEndpointFault
 * @tc.desc: Test BulkTransferReadwithLength with endpoint fault injection
 * @tc.desc: int32_t BulkTransferReadwithLength(const UsbDev &dev, const UsbPipe &pipe,
 *      int32_t timeout, int32_t length, std::vector<uint8_t> &data);
 * @tc.desc: Positive test: a short read is returned for a small length
 * @tc.desc: Negative test: SetEndpointBehavior injects an IN fault
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, BulkReadWithLengthAndEndpointFault, TestSize.Level1)
{
    /* Positive: request 64 bytes, get 64 bytes of pattern data. */
    std::vector<uint8_t> data;
    int32_t ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 64, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(64U, data.size());
    EXPECT_TRUE(IsPatternData(data, BULK_MAX_PACKET_SIZE));

    /* Negative: inject an IN fault on ep 0x81. */
    mock_usb::EndpointSpec fault;
    fault.address = BULK_IN_EP;
    fault.failInTimes = 1;
    fault.errorIn = -9; /* LIBUSB_ERROR_PIPE */
    ASSERT_EQ(0, mock_usb::SetEndpointBehavior(BUS_NUM, ADDR_MAIN, BULK_IN_EP, fault));
    ret = adapter_->BulkTransferReadwithLength(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, 64, data);
    /* The adapter maps LIBUSB_IO_ERROR(-1) to 0, but -9 is LIBUSB_ERROR_PIPE and
     * is returned as-is (a negative value). */
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Interrupt transfer tests ---- */

/**
 * @tc.name: InterruptReadOnAltSetting1
 * @tc.desc: Test InterruptTransferRead after switching to alt setting 1
 * @tc.desc: int32_t InterruptTransferRead(const UsbDev &dev, const UsbPipe &pipe,
 *      int32_t timeout, std::vector<uint8_t> &data);
 * @tc.desc: Positive test: interrupt IN endpoint 0x83 returns data on alt 1
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, InterruptReadOnAltSetting1, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    std::vector<uint8_t> data;
    int32_t ret = adapter_->InterruptTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, INTERRUPT_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_FALSE(data.empty());
    /* Restore alt 0 for subsequent tests. */
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 0));
}

/**
 * @tc.name: InterruptWriteOnAltSetting1
 * @tc.desc: Test InterruptTransferWrite after switching to alt setting 1
 * @tc.desc: int32_t InterruptTransferWrite(const UsbDev &dev, const UsbPipe &pipe,
 *      int32_t timeout, const std::vector<uint8_t> &data);
 * @tc.desc: Positive test: interrupt OUT endpoint 0x04 accepts data on alt 1
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, InterruptWriteOnAltSetting1, TestSize.Level1)
{
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 1));
    std::vector<uint8_t> payload = {0x55, 0xAA};
    int32_t ret = adapter_->InterruptTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, INTERRUPT_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ASSERT_EQ(HDF_SUCCESS, adapter_->SetInterface(MakeDev(ADDR_MAIN), INTERFACE_ID, 0));
}

/**
 * @tc.name: InterruptReadRejectsBulkEndpoint
 * @tc.desc: Test InterruptTransferRead on a bulk endpoint (wrong transfer type)
 * @tc.desc: Negative test: ep 0x81 is bulk, not interrupt -> HDF_ERR_INVALID_PARAM
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, InterruptReadRejectsBulkEndpoint, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->InterruptTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name: InterruptWriteRejectsBulkEndpoint
 * @tc.desc: Test InterruptTransferWrite on a bulk endpoint (wrong transfer type)
 * @tc.desc: Negative test: ep 0x02 is bulk, not interrupt -> HDF_ERR_INVALID_PARAM
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, InterruptWriteRejectsBulkEndpoint, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x01};
    int32_t ret = adapter_->InterruptTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_ERR_INVALID_PARAM, ret);
}

/* ---- Control transfer tests ---- */

/**
 * @tc.name: ControlReadReturnsDeviceDescriptor
 * @tc.desc: Test ControlTransferRead with GET_DESCRIPTOR(device)
 * @tc.desc: int32_t ControlTransferRead(const UsbDev &dev, const UsbCtrlTransfer &ctrl,
 *      std::vector<uint8_t> &data);
 * @tc.desc: Positive test: the device descriptor is returned with correct VID/PID
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, ControlReadReturnsDeviceDescriptor, TestSize.Level1)
{
    /* requestType=0x80 (IN, standard), request=0x06 (GET_DESCRIPTOR),
     * value=0x0100 (type=device, index=0) */
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_IN_STANDARD, CTRL_REQ_GET_DESCRIPTOR, 0x0100, 0x00,
        TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferRead(MakeDev(ADDR_MAIN), ctrl, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(DEVICE_DESC_LENGTH, static_cast<int>(data.size()));
    if (data.size() >= DEVICE_DESC_LENGTH) {
        /* bLength at offset 0, idVendor at offset 8, idProduct at offset 10. */
        EXPECT_EQ(DEVICE_DESC_LENGTH, data[0]);
        uint16_t vid = static_cast<uint16_t>(data[8]) | (static_cast<uint16_t>(data[9]) << 8);
        uint16_t pid = static_cast<uint16_t>(data[10]) | (static_cast<uint16_t>(data[11]) << 8);
        EXPECT_EQ(MAIN_VID, vid);
        EXPECT_EQ(MAIN_PID, pid);
    }
}

/**
 * @tc.name: ControlWriteSucceeds
 * @tc.desc: Test ControlTransferWrite with a vendor OUT request
 * @tc.desc: int32_t ControlTransferWrite(const UsbDev &dev, const UsbCtrlTransfer &ctrl,
 *      const std::vector<uint8_t> &data);
 * @tc.desc: Positive test: a vendor OUT control write is accepted
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, ControlWriteSucceeds, TestSize.Level1)
{
    /* requestType=0x40 (OUT, vendor), request=0x01, value=0, index=0 */
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_OUT_VENDOR, 0x01, 0x0000, 0x00, TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    int32_t ret = adapter_->ControlTransferWrite(MakeDev(ADDR_MAIN), ctrl, payload);
    EXPECT_EQ(HDF_SUCCESS, ret);
}

/**
 * @tc.name: ControlReadRejectsWrongDirection
 * @tc.desc: Test ControlTransferRead with an OUT direction request type
 * @tc.desc: Negative test: requestType 0x00 has OUT direction -> HDF_FAILURE
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, ControlReadRejectsWrongDirection, TestSize.Level1)
{
    struct UsbCtrlTransfer ctrl = {CTRL_REQ_TYPE_OUT_STANDARD, CTRL_REQ_GET_DESCRIPTOR, 0x0100, 0x00,
        TRANSFER_TIMEOUT_MS};
    std::vector<uint8_t> data;
    int32_t ret = adapter_->ControlTransferRead(MakeDev(ADDR_MAIN), ctrl, data);
    EXPECT_NE(HDF_SUCCESS, ret);
}

/* ---- Isochronous transfer tests ---- */

/**
 * @tc.name: IsoReadReturnsData
 * @tc.desc: Test IsoTransferRead on a bulk IN endpoint (iso delegates to bulk in the adapter)
 * @tc.desc: int32_t IsoTransferRead(const UsbDev &dev, const UsbPipe &pipe, int32_t timeout,
 *      std::vector<uint8_t> &data);
 * @tc.desc: Positive test: data is returned from the IN endpoint
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, IsoReadReturnsData, TestSize.Level1)
{
    std::vector<uint8_t> data;
    int32_t ret = adapter_->IsoTransferRead(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_IN_EP), TRANSFER_TIMEOUT_MS, data);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_FALSE(data.empty());
}

/**
 * @tc.name: IsoWriteSucceeds
 * @tc.desc: Test IsoTransferWrite on a bulk OUT endpoint
 * @tc.desc: int32_t IsoTransferWrite(const UsbDev &dev, const UsbPipe &pipe, int32_t timeout,
 *      const std::vector<uint8_t> &data);
 * @tc.desc: Positive test: the write completes successfully
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, IsoWriteSucceeds, TestSize.Level1)
{
    std::vector<uint8_t> payload = {0x77, 0x88, 0x99};
    int32_t ret = adapter_->IsoTransferWrite(MakeDev(ADDR_MAIN),
        MakePipe(INTERFACE_ID, BULK_OUT_EP), TRANSFER_TIMEOUT_MS, payload);
    EXPECT_EQ(HDF_SUCCESS, ret);
}

/* ---- Descriptor and speed tests ---- */

/**
 * @tc.name: GetDeviceDescriptorVerifiesFields
 * @tc.desc: Test GetDeviceDescriptor returns a well-formed device descriptor
 * @tc.desc: int32_t GetDeviceDescriptor(const UsbDev &dev, std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: bLength, VID, PID, bcdUSB are correct
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, GetDeviceDescriptorVerifiesFields, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetDeviceDescriptor(MakeDev(ADDR_MAIN), desc);
    EXPECT_EQ(HDF_SUCCESS, ret);
    EXPECT_EQ(DEVICE_DESC_LENGTH, static_cast<int>(desc.size()));
    if (desc.size() >= DEVICE_DESC_LENGTH) {
        EXPECT_EQ(DEVICE_DESC_LENGTH, desc[0]);
        /* bDescriptorType at offset 1 = 0x01 (device) */
        EXPECT_EQ(DESC_TYPE_DEVICE, desc[1]);
        uint16_t vid = static_cast<uint16_t>(desc[8]) | (static_cast<uint16_t>(desc[9]) << 8);
        uint16_t pid = static_cast<uint16_t>(desc[10]) | (static_cast<uint16_t>(desc[11]) << 8);
        EXPECT_EQ(MAIN_VID, vid);
        EXPECT_EQ(MAIN_PID, pid);
        /* bcdUSB at offset 2-3 = 0x0200 */
        uint16_t bcdUsb = static_cast<uint16_t>(desc[2]) | (static_cast<uint16_t>(desc[3]) << 8);
        EXPECT_EQ(0x0200, bcdUsb);
    }
}

/**
 * @tc.name: GetStringDescriptorReturnsManufacturer
 * @tc.desc: Test GetStringDescriptor for the manufacturer string (index 1)
 * @tc.desc: int32_t GetStringDescriptor(const UsbDev &dev, uint8_t descId,
 *      std::vector<uint8_t> &descriptor);
 * @tc.desc: Positive test: the descriptor decodes to "OpenHarmony"
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, GetStringDescriptorReturnsManufacturer, TestSize.Level1)
{
    std::vector<uint8_t> desc;
    int32_t ret = adapter_->GetStringDescriptor(MakeDev(ADDR_MAIN), DESC_INDEX_MANUFACTURER, desc);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* The adapter copies USB_MAX_DESCRIPTOR_SIZE (256) bytes into the descriptor. */
    EXPECT_FALSE(desc.empty());
    std::string text = DecodeUtf16Le(desc);
    EXPECT_EQ("OpenHarmony", text);
}

/**
 * @tc.name: GetDeviceSpeedReturnsHighSpeed
 * @tc.desc: Test GetDeviceSpeed returns the mock device speed
 * @tc.desc: int32_t GetDeviceSpeed(const UsbDev &dev, uint8_t &speed);
 * @tc.desc: Positive test: speed == LIBUSB_SPEED_HIGH (3)
 * @tc.type: FUNC
 */
HWTEST_F(AdapterTransferTest, GetDeviceSpeedReturnsHighSpeed, TestSize.Level1)
{
    uint8_t speed = 0;
    int32_t ret = adapter_->GetDeviceSpeed(MakeDev(ADDR_MAIN), speed);
    EXPECT_EQ(HDF_SUCCESS, ret);
    /* The mock default speed is 3 (LIBUSB_SPEED_HIGH). */
    EXPECT_EQ(3, static_cast<int>(speed));
}
} // namespace
