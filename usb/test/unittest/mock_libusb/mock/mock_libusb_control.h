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

#ifndef MOCK_LIBUSB_CONTROL_H
#define MOCK_LIBUSB_CONTROL_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

/* Default spec values, named so this header stays free of magic literals. */
#define MOCK_EP_DEFAULT_ADDRESS 0x81
#define MOCK_EP_DEFAULT_ATTR_BULK 0x02
#define MOCK_EP_DEFAULT_MAX_PACKET_SIZE 512
#define MOCK_EP_DEFAULT_ERROR (-9)
#define MOCK_IFACE_DEFAULT_CLASS 0xFF
#define MOCK_CONFIG_DEFAULT_ATTRS 0x80
#define MOCK_CONFIG_DEFAULT_MAX_POWER 50
#define MOCK_DEVICE_DEFAULT_VID 0x1209
#define MOCK_DEVICE_DEFAULT_PID 0x0001
#define MOCK_DEVICE_DEFAULT_BUS 1
#define MOCK_DEVICE_DEFAULT_ADDRESS 2
#define MOCK_DEVICE_DEFAULT_BCD_USB 0x0200
#define MOCK_DEVICE_DEFAULT_BCD_DEVICE 0x0100
#define MOCK_DEVICE_DEFAULT_MAX_PACKET0 64
#define MOCK_DEVICE_DEFAULT_SPEED 3

namespace mock_usb {
/* How an IN endpoint produces its payload. */
enum class DataMode {
    PATTERN,   /* data[i] = (i % 512) % 63, same as the kernel SourceSink function */
    FIXED,     /* replay the fixed payload */
    SINK_ONLY, /* always return a zero length packet */
};

struct EndpointSpec {
    uint8_t address = MOCK_EP_DEFAULT_ADDRESS;      /* 0x81 = IN, 0x02 = OUT */
    uint8_t attributes = MOCK_EP_DEFAULT_ATTR_BULK; /* 2 = bulk, 3 = interrupt */
    uint16_t maxPacketSize = MOCK_EP_DEFAULT_MAX_PACKET_SIZE;
    uint8_t interval = 0;
    DataMode mode = DataMode::PATTERN;
    std::vector<uint8_t> fixed;
    int32_t maxInLength = -1;    /* if >= 0, IN transfers are shortened to this size */
    int32_t failInTimes = 0;     /* return errorIn for the next N IN transfers */
    int32_t failOutTimes = 0;
    int32_t errorIn = MOCK_EP_DEFAULT_ERROR;  /* LIBUSB_ERROR_PIPE by default */
    int32_t errorOut = MOCK_EP_DEFAULT_ERROR;
};

struct AltSettingSpec {
    uint8_t altSetting = 0;
    std::vector<EndpointSpec> endpoints;
};

struct InterfaceSpec {
    uint8_t number = 0;
    uint8_t klass = MOCK_IFACE_DEFAULT_CLASS;
    uint8_t subClass = 0;
    uint8_t protocol = 0;
    std::vector<AltSettingSpec> altSettings;
};

struct ConfigSpec {
    uint8_t value = 1;
    uint8_t attributes = MOCK_CONFIG_DEFAULT_ATTRS;
    uint8_t maxPower = MOCK_CONFIG_DEFAULT_MAX_POWER;
    std::string description;
    std::vector<InterfaceSpec> interfaces;
};

struct DeviceSpec {
    uint16_t vendorId = MOCK_DEVICE_DEFAULT_VID;
    uint16_t productId = MOCK_DEVICE_DEFAULT_PID;
    uint8_t bus = MOCK_DEVICE_DEFAULT_BUS;
    uint8_t address = MOCK_DEVICE_DEFAULT_ADDRESS;
    uint16_t bcdUSB = MOCK_DEVICE_DEFAULT_BCD_USB;
    uint16_t bcdDevice = MOCK_DEVICE_DEFAULT_BCD_DEVICE;
    uint8_t deviceClass = 0;
    uint8_t deviceSubClass = 0;
    uint8_t deviceProtocol = 0;
    uint8_t maxPacketSize0 = MOCK_DEVICE_DEFAULT_MAX_PACKET0;
    int speed = MOCK_DEVICE_DEFAULT_SPEED; /* LIBUSB_SPEED_HIGH */
    std::string manufacturer = "OpenHarmony";
    std::string product = "Mock USB Device";
    std::string serial = "MOCK0001";
    uint8_t activeConfigValue = 1;
    std::vector<ConfigSpec> configs;
};

/* One entry of the transfer log; every call handled by the mock is recorded. */
struct TransferRecord {
    uint64_t seq = 0;
    std::string kind;      /* control-in / bulk-out / submit / complete / hotplug ... */
    uint8_t bus = 0;
    uint8_t address = 0;
    uint8_t endpoint = 0;
    int32_t requestType = 0;
    int32_t request = 0;
    int32_t value = 0;
    int32_t index = 0;
    int32_t length = 0;
    int32_t actual = 0;
    int32_t ret = 0;
    std::string data;      /* hex preview of the payload */
};

/* Build a device with one config, one interface (alt 0 = bulk in/out,
 * alt 1 = interrupt in/out) - enough for most LibusbAdapter paths. */
DeviceSpec MakeDefaultSpec(uint8_t bus, uint8_t address, uint16_t vid, uint16_t pid);

void Reset();
int32_t AddDevice(const DeviceSpec &spec);
int32_t SetEndpointBehavior(uint8_t bus, uint8_t address, uint8_t endpoint, const EndpointSpec &behavior);
int32_t SetControlResponse(uint8_t bus, uint8_t address, int32_t requestType, int32_t request,
    const std::vector<uint8_t> &payload);
int32_t RaiseHotplug(uint8_t bus, uint8_t address, bool arrived);

/* Mark a device as present/absent without raising a hotplug event. Devices that are
 * absent are skipped by libusb_get_device_list(); a LEFT event also clears presence. */
int32_t SetDevicePresent(uint8_t bus, uint8_t address, bool present);

/* When disabled, submitted transfers stay queued until auto completion is
 * enabled again; used to test cancellation of in-flight transfers. */
void SetAutoComplete(bool enabled);
size_t PendingTransferCount();

/* libusb entry points that can be forced to fail once (negative error code). */
enum class MockApi {
    OPEN,
    CLAIM_INTERFACE,
    RELEASE_INTERFACE,
    SET_CONFIGURATION,
    GET_CONFIGURATION,
    SET_ALT_SETTING,
    CLEAR_HALT,
    RESET_DEVICE,
    KERNEL_DRIVER_ACTIVE,
    DETACH_KERNEL_DRIVER,
    ATTACH_KERNEL_DRIVER,
    SET_AUTO_DETACH_KERNEL_DRIVER,
    CONTROL_TRANSFER,
    SUBMIT_TRANSFER,
    HOTPLUG_REGISTER,
    GET_MAX_PACKET_SIZE,
    ACTIVE_CONFIG_DESCRIPTOR,
    HANDLE_EVENTS,
};
void FailNextCall(MockApi api, int32_t errorCode, int32_t times = 1);

/* Block the next call of the given libusb entry point until ReleaseBlockedCall()
 * is invoked. Used to freeze the caller inside the window between "handle was
 * looked up" and "transfer is started". */
void BlockNextCall(MockApi api);
int32_t WaitingCallCount();
void ReleaseBlockedCall();

/* Simulate a device that does not answer: every transfer blocks for the given
 * number of milliseconds before it is completed (0 disables the delay). */
void SetTransferDelayMs(uint32_t delayMs);

/* Hazard detectors for handle lifetime tests:
 * - CloseWhileInFlightCount(): libusb_close() was called while a transfer was
 *   still running on that handle (the real stack would use freed memory here).
 * - UseAfterCloseCount(): a transfer was started on an already closed handle. */
int32_t CloseWhileInFlightCount();
int32_t UseAfterCloseCount();
int32_t InFlightTransferCount();
void ClearHazardCounters();

void ClearRecords();
size_t RecordCount();
TransferRecord RecordAt(size_t index);
void SetDumpEnabled(bool enabled);
bool DumpEnabled();
void Dump(FILE *out);
} // namespace mock_usb

#endif // MOCK_LIBUSB_CONTROL_H
