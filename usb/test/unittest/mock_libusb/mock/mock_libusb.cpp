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
 * Mock libusb implementation.
 *
 * Provides every libusb_* symbol needed by libusb_adapter.cpp, backed by an
 * in-process device model:
 *   - devices, configurations, interfaces, alt settings, endpoints, strings
 *   - control/bulk/interrupt transfers with payload inspection
 *   - asynchronous transfers driven by libusb_handle_events_completed()
 *   - hotplug callback delivery
 *   - fault injection (stall/timeout/no-device) and a full transfer log
 */

#include "libusb.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "securec.h"

/* Named constants: keeps the implementation free of magic numbers. */
/* USB string descriptors: 2 byte header followed by UTF-16LE characters. */
#define MOCK_STRING_DESC_HEAD_BYTES 2
#define MOCK_UTF16_CHAR_BYTES 2
/* Language id descriptor for en-US (0x0409), stored in little endian. */
#define MOCK_LANGID_DESC_LENGTH 4
#define MOCK_LANGID_EN_US_BYTE0 0x09
#define MOCK_LANGID_EN_US_BYTE1 0x04
/* Shift and mask used by the (type << 8) | index control request encoding. */
#define MOCK_BYTE_BITS 8
#define MOCK_BYTE_MASK 0xFF
/* HexPreview(): at most 16 bytes are printed into an 8 byte text buffer. */
#define MOCK_HEX_PREVIEW_LIMIT 16
#define MOCK_HEX_TEXT_BUF_SIZE 8
/* Filler bytes returned for a control-in request without a canned response. */
#define MOCK_DEFAULT_FILL_LENGTH 8
#define MOCK_CONTROL_FILL_BYTE 0x5A
/* data[i] = (i % maxPacketSize) % MOCK_PATTERN_MODULUS, the same pattern as
 * the kernel SourceSink function used by dummy_hcd. */
#define MOCK_PATTERN_MODULUS 63
/* Poll interval used while no transfer is pending, and us/ms conversion. */
#define MOCK_EVENT_POLL_INTERVAL_MS 5
#define MOCK_US_PER_MS 1000
/* Interface limit and endpoint packet size parsing, mirroring upstream libusb. */
#define MOCK_MAX_INTERFACES 32
#define MOCK_MAX_PACKET_SIZE_MASK 0x07FF
#define MOCK_ENDPOINT_TYPE_MASK 0x03
#define MOCK_HIGH_BANDWIDTH_SHIFT 11
#define MOCK_HIGH_BANDWIDTH_MASK 3
/* Device model built by MakeDefaultSpec(). */
#define MOCK_VENDOR_INTERFACE_CLASS 0xFF
#define MOCK_SECOND_CONFIG_VALUE 2
#define MOCK_ENDPOINT_ATTR_BULK 0x02
#define MOCK_ENDPOINT_ATTR_INTERRUPT 0x03
#define MOCK_BULK_IN_ENDPOINT 0x81
#define MOCK_BULK_OUT_ENDPOINT 0x02
#define MOCK_INTERRUPT_IN_ENDPOINT 0x83
#define MOCK_INTERRUPT_OUT_ENDPOINT 0x04
#define MOCK_BULK_MAX_PACKET_SIZE 512
/* Full speed bulk packet size used by the second configuration. */
#define MOCK_BULK_MAX_PACKET_SIZE_FS 64
#define MOCK_INTERRUPT_MAX_PACKET_SIZE 64
#define MOCK_INTERRUPT_INTERVAL 8
/* String descriptor indexes stored in the device descriptor. */
#define MOCK_STR_DESC_INDEX_MANUFACTURER 1
#define MOCK_STR_DESC_INDEX_PRODUCT 2
#define MOCK_STR_DESC_INDEX_SERIAL 3
/* Fixed bulk-in payload of the second configuration. */
#define MOCK_FIXED_PAYLOAD_INIT { 0x11, 0x22, 0x33, 0x44 }

/* Opaque in libusb.h, defined here for the mock. */
struct libusb_context {
    int unused;
};

struct libusb_device {
    int index;
};

struct libusb_device_handle {
    int index;
    /* Bit mask of the interfaces claimed through this handle (upstream tracks the same). */
    unsigned long claimedInterfaces = 0;
};

namespace {
using mock_usb::AltSettingSpec;
using mock_usb::ConfigSpec;
using mock_usb::DataMode;
using mock_usb::DeviceSpec;
using mock_usb::EndpointSpec;
using mock_usb::InterfaceSpec;
using mock_usb::TransferRecord;

/* The descriptor types and request codes share the values declared by the
 * mock libusb.h header; reuse them instead of repeating literals here. */
constexpr int32_t LIBUSB_DT_DEVICE_MOCK = LIBUSB_DT_DEVICE;
constexpr int32_t LIBUSB_DT_CONFIG_MOCK = LIBUSB_DT_CONFIG;
constexpr int32_t LIBUSB_DT_STRING_MOCK = LIBUSB_DT_STRING;
constexpr int32_t LIBUSB_DT_INTERFACE_MOCK = LIBUSB_DT_INTERFACE;
constexpr int32_t LIBUSB_DT_ENDPOINT_MOCK = LIBUSB_DT_ENDPOINT;
constexpr int32_t REQUEST_GET_DESCRIPTOR_MOCK = LIBUSB_REQUEST_GET_DESCRIPTOR;
constexpr uint8_t ENDPOINT_DIR_MASK_MOCK = LIBUSB_ENDPOINT_DIR_MASK;

struct MockEndpoint {
    EndpointSpec spec;
};

struct MockAlt {
    AltSettingSpec spec;
    std::vector<libusb_endpoint_descriptor> eps;
    std::vector<MockEndpoint> epSpecs;
};

struct MockInterface {
    InterfaceSpec spec;
    std::vector<libusb_interface_descriptor> alts;
    std::vector<MockAlt> altSpecs;
};

struct MockConfig {
    ConfigSpec spec;
    libusb_config_descriptor desc;
    std::vector<libusb_interface> ifaces;
    std::vector<MockInterface> interfaceSpecs;
};

struct MockDevice {
    DeviceSpec spec;
    libusb_device dev;
    libusb_device_descriptor desc;
    std::vector<MockConfig> configs;
    std::map<int32_t, std::vector<uint8_t>> controlResponses; /* key = (requestType << 8) | request */
    std::vector<uint8_t> lastControlOut;
    bool present = true; /* a "left" device disappears from libusb_get_device_list() */
};

std::mutex g_lock;
std::vector<std::unique_ptr<MockDevice>> g_devices;
std::vector<TransferRecord> g_records;
uint64_t g_seq = 0;
bool g_dumpEnabled = false;
/* Registered hotplug callbacks; upstream libusb keeps a list per context. */
struct HotplugRegistration {
    int handle;
    int events;
    libusb_hotplug_callback_fn callback;
    void *userData;
};
std::vector<HotplugRegistration> g_hotplugCallbacks;
int g_nextHotplugHandle = 1;
libusb_context g_context;
std::vector<libusb_transfer *> g_pendingTransfers;
std::set<libusb_transfer *> g_cancelledTransfers;
std::map<libusb_transfer *, std::chrono::steady_clock::time_point> g_submitTimes;
bool g_autoComplete = true;
std::map<int32_t, std::pair<int32_t, int32_t>> g_injectedFailures; /* api -> (times, error) */
uint32_t g_transferDelayMs = 0;
std::map<libusb_device_handle *, int32_t> g_inFlight;
std::set<libusb_device_handle *> g_closedHandles; /* quarantined, never freed */
int32_t g_closeWhileInFlight = 0;
int32_t g_useAfterClose = 0;
mock_usb::MockApi g_blockApi = mock_usb::MockApi::OPEN;
bool g_blockArmed = false;
int32_t g_waitingCalls = 0;
bool g_blockReleased = false;
std::condition_variable g_blockCv;

/* Freezes the calling thread inside a specific libusb entry point. The wait
 * happens without holding g_lock so that other threads (CloseDevice) can run. */
void MaybeBlockCall(mock_usb::MockApi api)
{
    std::unique_lock<std::mutex> lock(g_lock);
    if (!g_blockArmed || g_blockApi != api) {
        return;
    }
    g_blockArmed = false;
    g_waitingCalls++;
    g_blockCv.notify_all();
    g_blockCv.wait(lock, []() { return g_blockReleased; });
    g_blockReleased = false;
    g_waitingCalls--;
}

/* Keeps track of a running transfer so that libusb_close() can detect a close
 * while the transfer is still in flight (the hazard the adapter must avoid). */
class InFlightGuard {
public:
    explicit InFlightGuard(libusb_device_handle *handle) : handle_(handle)
    {
        if (handle_ == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_closedHandles.count(handle_) > 0) {
            g_useAfterClose++;
        }
        g_inFlight[handle_]++;
    }

    ~InFlightGuard()
    {
        if (handle_ == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(g_lock);
        auto it = g_inFlight.find(handle_);
        if (it != g_inFlight.end() && --it->second <= 0) {
            g_inFlight.erase(it);
        }
    }

private:
    libusb_device_handle *handle_;
};

/* The delay is applied outside of the mock lock so that a blocked transfer does
 * not hide the adapter level locking behaviour that the tests want to observe. */
void SleepTransferDelay()
{
    uint32_t delay = 0;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        delay = g_transferDelayMs;
    }
    if (delay != 0) {
        usleep(delay * MOCK_US_PER_MS);
    }
}

/* True when the given call/transfer has been outstanding for at least its timeout (0 = no timeout). */
bool IsTimedOut(const std::chrono::steady_clock::time_point &start, unsigned int timeout)
{
    if (timeout == 0) {
        return false;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    return static_cast<int64_t>(elapsed) >= static_cast<int64_t>(timeout);
}

int32_t TakeInjectedFailure(mock_usb::MockApi api)
{
    auto it = g_injectedFailures.find(static_cast<int32_t>(api));
    if (it == g_injectedFailures.end() || it->second.first == 0) {
        return 0;
    }
    if (it->second.first > 0) {
        it->second.first--;
    }
    return it->second.second;
}

std::string HexPreview(const uint8_t *data, int32_t length, int32_t limit = MOCK_HEX_PREVIEW_LIMIT)
{
    if (data == nullptr || length <= 0) {
        return "-";
    }
    std::string text;
    int32_t count = std::min(length, limit);
    char buf[MOCK_HEX_TEXT_BUF_SIZE] = {0};
    for (int32_t i = 0; i < count; i++) {
        (void)snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%02x ", data[i]);
        text += buf;
    }
    if (length > count) {
        text += "...";
    }
    return text;
}

void AddRecord(const TransferRecord &record)
{
    TransferRecord entry = record;
    entry.seq = ++g_seq;
    g_records.push_back(entry);
}

MockDevice *FindDevice(uint8_t bus, uint8_t address)
{
    for (auto &device : g_devices) {
        if (device->spec.bus == bus && device->spec.address == address) {
            return device.get();
        }
    }
    return nullptr;
}

MockDevice *FindDeviceByHandle(libusb_device_handle *handle)
{
    if (handle == nullptr || handle->index < 0 || static_cast<size_t>(handle->index) >= g_devices.size()) {
        return nullptr;
    }
    return g_devices[static_cast<size_t>(handle->index)].get();
}

MockConfig *ActiveConfig(MockDevice &device)
{
    for (auto &config : device.configs) {
        if (config.spec.value == device.spec.activeConfigValue) {
            return &config;
        }
    }
    return device.configs.empty() ? nullptr : &device.configs[0];
}

EndpointSpec *FindEndpointSpec(MockDevice &device, uint8_t address, MockConfig **configOut = nullptr)
{
    MockConfig *config = ActiveConfig(device);
    if (config == nullptr) {
        return nullptr;
    }
    if (configOut != nullptr) {
        *configOut = config;
    }
    for (auto &iface : config->interfaceSpecs) {
        for (auto &alt : iface.altSpecs) {
            for (auto &ep : alt.epSpecs) {
                if (ep.spec.address == address) {
                    return &ep.spec;
                }
            }
        }
    }
    return nullptr;
}

bool InterfaceExists(MockDevice &device, int interfaceNumber)
{
    MockConfig *config = ActiveConfig(device);
    if (config == nullptr) {
        return false;
    }
    for (const auto &iface : config->interfaceSpecs) {
        if (iface.spec.number == interfaceNumber) {
            return true;
        }
    }
    return false;
}

void AllocRecord(TransferRecord &record, MockDevice *device, const char *kind)
{
    record.kind = kind;
    record.bus = device != nullptr ? device->spec.bus : 0;
    record.address = device != nullptr ? device->spec.address : 0;
}

std::vector<uint8_t> MakeStringDescriptor(const std::string &text)
{
    std::vector<uint8_t> descriptor(MOCK_STRING_DESC_HEAD_BYTES + text.size() * MOCK_UTF16_CHAR_BYTES, 0);
    descriptor[0] = static_cast<uint8_t>(descriptor.size());
    descriptor[1] = LIBUSB_DT_STRING_MOCK;
    for (size_t i = 0; i < text.size(); i++) {
        descriptor[MOCK_STRING_DESC_HEAD_BYTES + i * MOCK_UTF16_CHAR_BYTES] = static_cast<uint8_t>(text[i]);
    }
    return descriptor;
}

void BuildConfigDescriptor(MockConfig &config)
{
    uint16_t total = LIBUSB_DT_CONFIG_SIZE;
    for (auto &iface : config.interfaceSpecs) {
        for (auto &alt : iface.altSpecs) {
            total = static_cast<uint16_t>(total + LIBUSB_DT_INTERFACE_SIZE +
                alt.eps.size() * LIBUSB_DT_ENDPOINT_SIZE);
        }
    }
    (void)memset_s(&config.desc, sizeof(config.desc), 0, sizeof(config.desc));
    config.desc.bLength = LIBUSB_DT_CONFIG_SIZE;
    config.desc.bDescriptorType = LIBUSB_DT_CONFIG_MOCK;
    config.desc.wTotalLength = total;
    config.desc.bNumInterfaces = static_cast<uint8_t>(config.ifaces.size());
    config.desc.bConfigurationValue = config.spec.value;
    config.desc.iConfiguration = 0;
    config.desc.bmAttributes = config.spec.attributes;
    config.desc.MaxPower = config.spec.maxPower;
    config.desc.interface = config.ifaces.data();
    config.desc.extra = nullptr;
    config.desc.extra_length = 0;
}

void BuildDeviceDescriptors(MockDevice &device)
{
    const DeviceSpec &spec = device.spec;
    (void)memset_s(&device.desc, sizeof(device.desc), 0, sizeof(device.desc));
    device.desc.bLength = LIBUSB_DT_DEVICE_SIZE;
    device.desc.bDescriptorType = LIBUSB_DT_DEVICE_MOCK;
    device.desc.bcdUSB = spec.bcdUSB;
    device.desc.bDeviceClass = spec.deviceClass;
    device.desc.bDeviceSubClass = spec.deviceSubClass;
    device.desc.bDeviceProtocol = spec.deviceProtocol;
    device.desc.bMaxPacketSize0 = spec.maxPacketSize0;
    device.desc.idVendor = spec.vendorId;
    device.desc.idProduct = spec.productId;
    device.desc.bcdDevice = spec.bcdDevice;
    device.desc.iManufacturer = MOCK_STR_DESC_INDEX_MANUFACTURER;
    device.desc.iProduct = MOCK_STR_DESC_INDEX_PRODUCT;
    device.desc.iSerialNumber = MOCK_STR_DESC_INDEX_SERIAL;
    device.desc.bNumConfigurations = static_cast<uint8_t>(device.configs.size());

    for (auto &config : device.configs) {
        for (auto &iface : config.interfaceSpecs) {
            iface.alts.resize(iface.altSpecs.size());
            for (size_t a = 0; a < iface.altSpecs.size(); a++) {
                MockAlt &alt = iface.altSpecs[a];
                alt.eps.resize(alt.epSpecs.size());
                for (size_t e = 0; e < alt.epSpecs.size(); e++) {
                    const EndpointSpec &epSpec = alt.epSpecs[e].spec;
                    libusb_endpoint_descriptor &ep = alt.eps[e];
                    (void)memset_s(&ep, sizeof(ep), 0, sizeof(ep));
                    ep.bLength = LIBUSB_DT_ENDPOINT_SIZE;
                    ep.bDescriptorType = LIBUSB_DT_ENDPOINT_MOCK;
                    ep.bEndpointAddress = epSpec.address;
                    ep.bmAttributes = epSpec.attributes;
                    ep.wMaxPacketSize = epSpec.maxPacketSize;
                    ep.bInterval = epSpec.interval;
                    ep.extra = nullptr;
                    ep.extra_length = 0;
                }
                libusb_interface_descriptor &desc = iface.alts[a];
                (void)memset_s(&desc, sizeof(desc), 0, sizeof(desc));
                desc.bLength = LIBUSB_DT_INTERFACE_SIZE;
                desc.bDescriptorType = LIBUSB_DT_INTERFACE_MOCK;
                desc.bInterfaceNumber = iface.spec.number;
                desc.bAlternateSetting = alt.spec.altSetting;
                desc.bNumEndpoints = static_cast<uint8_t>(alt.eps.size());
                desc.bInterfaceClass = iface.spec.klass;
                desc.bInterfaceSubClass = iface.spec.subClass;
                desc.bInterfaceProtocol = iface.spec.protocol;
                desc.iInterface = 0;
                desc.endpoint = alt.eps.empty() ? nullptr : alt.eps.data();
                desc.extra = nullptr;
                desc.extra_length = 0;
            }
        }
        config.ifaces.resize(config.interfaceSpecs.size());
        for (size_t i = 0; i < config.interfaceSpecs.size(); i++) {
            MockInterface &mockInterface = config.interfaceSpecs[i];
            libusb_interface &ifaceDesc = config.ifaces[i];
            (void)memset_s(&ifaceDesc, sizeof(ifaceDesc), 0, sizeof(ifaceDesc));
            ifaceDesc.altsetting = mockInterface.alts.data();
            ifaceDesc.num_altsetting = static_cast<int>(mockInterface.alts.size());
        }
        BuildConfigDescriptor(config);
    }
}

int32_t ControlTransferLocked(MockDevice &device, uint8_t requestType, uint8_t request, uint16_t value,
    uint16_t index, uint8_t *data, uint16_t length)
{
    TransferRecord record;
    AllocRecord(record, &device, (requestType & ENDPOINT_DIR_MASK_MOCK) != 0 ? "control-in" : "control-out");
    record.endpoint = 0;
    record.requestType = requestType;
    record.request = request;
    record.value = value;
    record.index = index;
    record.length = length;

    int32_t result = 0;
    if (request == REQUEST_GET_DESCRIPTOR_MOCK && (requestType & ENDPOINT_DIR_MASK_MOCK) != 0) {
        uint8_t type = static_cast<uint8_t>(value >> MOCK_BYTE_BITS);
        uint8_t descIndex = static_cast<uint8_t>(value & MOCK_BYTE_MASK);
        std::vector<uint8_t> blob;
        if (type == LIBUSB_DT_DEVICE_MOCK) {
            blob.assign(reinterpret_cast<uint8_t *>(&device.desc),
                reinterpret_cast<uint8_t *>(&device.desc) + sizeof(device.desc));
            blob.resize(LIBUSB_DT_DEVICE_SIZE);
        } else if (type == LIBUSB_DT_CONFIG_MOCK) {
            /* USB requests the configuration descriptor by index, not by value. */
            if (descIndex < device.configs.size()) {
                MockConfig &config = device.configs[descIndex];
                blob.assign(reinterpret_cast<uint8_t *>(&config.desc),
                    reinterpret_cast<uint8_t *>(&config.desc) + LIBUSB_DT_CONFIG_SIZE);
                for (auto &iface : config.interfaceSpecs) {
                    for (size_t a = 0; a < iface.alts.size(); a++) {
                        const uint8_t *ifacePtr = reinterpret_cast<const uint8_t *>(&iface.alts[a]);
                        blob.insert(blob.end(), ifacePtr, ifacePtr + LIBUSB_DT_INTERFACE_SIZE);
                        for (auto &ep : iface.altSpecs[a].eps) {
                            const uint8_t *epPtr = reinterpret_cast<const uint8_t *>(&ep);
                            blob.insert(blob.end(), epPtr, epPtr + LIBUSB_DT_ENDPOINT_SIZE);
                        }
                    }
                }
            }
        } else if (type == LIBUSB_DT_STRING_MOCK) {
            if (descIndex == 0) {
                blob = {MOCK_LANGID_DESC_LENGTH, LIBUSB_DT_STRING_MOCK, MOCK_LANGID_EN_US_BYTE0,
                    MOCK_LANGID_EN_US_BYTE1};
            } else if (descIndex == MOCK_STR_DESC_INDEX_MANUFACTURER) {
                blob = MakeStringDescriptor(device.spec.manufacturer);
            } else if (descIndex == MOCK_STR_DESC_INDEX_PRODUCT) {
                blob = MakeStringDescriptor(device.spec.product);
            } else if (descIndex == MOCK_STR_DESC_INDEX_SERIAL) {
                blob = MakeStringDescriptor(device.spec.serial);
            }
        }
        if (!blob.empty() && data != nullptr) {
            size_t copyLen = std::min(static_cast<size_t>(length), blob.size());
            (void)memcpy_s(data, static_cast<size_t>(length), blob.data(), copyLen);
            result = static_cast<int32_t>(copyLen);
        }
    } else if ((requestType & ENDPOINT_DIR_MASK_MOCK) != 0) {
        auto it = device.controlResponses.find((static_cast<int32_t>(requestType) << MOCK_BYTE_BITS) | request);
        if (it != device.controlResponses.end()) {
            size_t copyLen = std::min(static_cast<size_t>(length), it->second.size());
            if (data != nullptr && copyLen > 0) {
                (void)memcpy_s(data, static_cast<size_t>(length), it->second.data(), copyLen);
            }
            result = static_cast<int32_t>(copyLen);
        } else {
            size_t copyLen = std::min<size_t>(length, MOCK_DEFAULT_FILL_LENGTH);
            if (data != nullptr) {
                (void)memset_s(data, static_cast<size_t>(length), MOCK_CONTROL_FILL_BYTE, copyLen);
            }
            result = static_cast<int32_t>(copyLen);
        }
    } else {
        if (data != nullptr && length > 0) {
            device.lastControlOut.assign(data, data + length);
        }
        result = length;
    }
    record.actual = result;
    record.ret = result;
    record.data = HexPreview(data, result > 0 ? result : 0);
    AddRecord(record);
    return result;
}

/* Payload byte for IN transfers; shared by the bulk/interrupt and iso paths. */
uint8_t MakePayloadByte(const EndpointSpec &spec, int32_t index)
{
    if (spec.mode == DataMode::FIXED) {
        return index < static_cast<int32_t>(spec.fixed.size()) ? spec.fixed[index] : 0;
    }
    int32_t packetSize = spec.maxPacketSize > 0 ? spec.maxPacketSize : 1;
    return static_cast<uint8_t>((index % packetSize) % MOCK_PATTERN_MODULUS);
}

int32_t BulkTransferLocked(MockDevice &device, uint8_t endpoint, uint8_t *data, int length, int *transferred,
    const char *kind)
{
    TransferRecord record;
    AllocRecord(record, &device, kind);
    record.endpoint = endpoint;
    record.length = length;

    EndpointSpec *spec = FindEndpointSpec(device, endpoint);
    int32_t result = 0;
    int32_t done = 0;
    if (spec == nullptr) {
        result = LIBUSB_ERROR_NOT_FOUND;
    } else if (data == nullptr && length > 0) {
        /* Real backends reject a transfer without a data buffer. */
        result = LIBUSB_ERROR_INVALID_PARAM;
    } else if ((endpoint & ENDPOINT_DIR_MASK_MOCK) != 0) {
        if (spec->failInTimes != 0) {
            if (spec->failInTimes > 0) {
                spec->failInTimes--;
            }
            result = spec->errorIn;
        } else {
            int32_t produce = length;
            if (spec->maxInLength >= 0) {
                produce = std::min(produce, spec->maxInLength);
            }
            if (spec->mode == DataMode::SINK_ONLY) {
                produce = 0;
            } else {
                for (int32_t i = 0; i < produce; i++) {
                    data[i] = MakePayloadByte(*spec, i);
                }
            }
            done = produce;
        }
    } else {
        if (spec->failOutTimes != 0) {
            if (spec->failOutTimes > 0) {
                spec->failOutTimes--;
            }
            result = spec->errorOut;
        } else {
            done = length;
        }
    }
    if (transferred != nullptr) {
        *transferred = done;
    }
    record.actual = done;
    record.ret = result;
    record.data = HexPreview(data, done);
    AddRecord(record);
    return result;
}

/* Completes an isochronous transfer packet by packet, as the real stack reports
 * per-packet status/length while the transfer itself may still read COMPLETED. */
void CompleteIsoTransferLocked(MockDevice &device, libusb_transfer *transfer)
{
    EndpointSpec *spec = FindEndpointSpec(device, transfer->endpoint);
    if (spec == nullptr) {
        transfer->status = LIBUSB_TRANSFER_ERROR;
        transfer->actual_length = 0;
        return;
    }
    bool isIn = (transfer->endpoint & ENDPOINT_DIR_MASK_MOCK) != 0;
    bool fail = isIn ? (spec->failInTimes != 0) : (spec->failOutTimes != 0);
    if (fail) {
        if (isIn && spec->failInTimes > 0) {
            spec->failInTimes--;
        }
        if (!isIn && spec->failOutTimes > 0) {
            spec->failOutTimes--;
        }
    }
    bool sinkOnly = spec->mode == DataMode::SINK_ONLY;
    int32_t offset = 0;
    int32_t done = 0;
    for (int i = 0; i < transfer->num_iso_packets; i++) {
        libusb_iso_packet_descriptor &packet = transfer->iso_packet_desc[i];
        if (fail) {
            packet.status = LIBUSB_TRANSFER_STALL;
            packet.actual_length = 0;
            continue;
        }
        if (sinkOnly) {
            packet.status = LIBUSB_TRANSFER_COMPLETED;
            packet.actual_length = 0;
            continue;
        }
        int32_t packetLen = static_cast<int32_t>(packet.length);
        if (isIn && spec->maxInLength >= 0) {
            packetLen = std::min(packetLen, spec->maxInLength);
        }
        if (isIn && transfer->buffer != nullptr) {
            for (int32_t j = 0; j < packetLen; j++) {
                transfer->buffer[offset + j] = MakePayloadByte(*spec, offset + j);
            }
        }
        offset += packetLen;
        packet.status = LIBUSB_TRANSFER_COMPLETED;
        packet.actual_length = static_cast<unsigned int>(packetLen);
        done += packetLen;
    }
    transfer->status = LIBUSB_TRANSFER_COMPLETED;
    transfer->actual_length = done;
}

void CompleteTransferLocked(libusb_transfer *transfer, bool timedOut)
{
    if (transfer == nullptr) {
        return;
    }
    MockDevice *device = FindDeviceByHandle(transfer->dev_handle);
    TransferRecord record;
    AllocRecord(record, device, "complete");
    record.endpoint = transfer->endpoint;
    record.length = transfer->length;

    bool cancelled = g_cancelledTransfers.erase(transfer) > 0;
    bool disconnected = device == nullptr || !device->present ||
        g_closedHandles.count(transfer->dev_handle) > 0;
    if (cancelled) {
        transfer->status = LIBUSB_TRANSFER_CANCELLED;
        transfer->actual_length = 0;
    } else if (timedOut) {
        transfer->status = LIBUSB_TRANSFER_TIMED_OUT;
        transfer->actual_length = 0;
    } else if (disconnected) {
        transfer->status = LIBUSB_TRANSFER_NO_DEVICE;
        transfer->actual_length = 0;
    } else if (transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
        CompleteIsoTransferLocked(*device, transfer);
    } else {
        int transferred = 0;
        const char *kind = (transfer->endpoint & ENDPOINT_DIR_MASK_MOCK) != 0 ? "bulk-in" : "bulk-out";
        if (transfer->type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
            kind = (transfer->endpoint & ENDPOINT_DIR_MASK_MOCK) != 0 ? "interrupt-in" : "interrupt-out";
        }
        int32_t ret = BulkTransferLocked(*device, transfer->endpoint, transfer->buffer, transfer->length,
            &transferred, kind);
        if (ret == 0) {
            transfer->status = LIBUSB_TRANSFER_COMPLETED;
            transfer->actual_length = transferred;
        } else if (ret == LIBUSB_ERROR_TIMEOUT) {
            transfer->status = LIBUSB_TRANSFER_TIMED_OUT;
            transfer->actual_length = 0;
        } else if (ret == LIBUSB_ERROR_NO_DEVICE) {
            transfer->status = LIBUSB_TRANSFER_NO_DEVICE;
            transfer->actual_length = 0;
        } else if (ret == LIBUSB_ERROR_PIPE) {
            transfer->status = LIBUSB_TRANSFER_STALL;
            transfer->actual_length = 0;
        } else if (ret == LIBUSB_ERROR_OVERFLOW) {
            transfer->status = LIBUSB_TRANSFER_OVERFLOW;
            transfer->actual_length = 0;
        } else {
            /* Any other errno surfaces as a generic transfer error, as in the real stack. */
            transfer->status = LIBUSB_TRANSFER_ERROR;
            transfer->actual_length = 0;
        }
    }
    record.actual = transfer->actual_length;
    record.ret = transfer->status;
    AddRecord(record);
}
} // namespace

namespace mock_usb {
DeviceSpec MakeDefaultSpec(uint8_t bus, uint8_t address, uint16_t vid, uint16_t pid)
{
    DeviceSpec spec;
    spec.bus = bus;
    spec.address = address;
    spec.vendorId = vid;
    spec.productId = pid;

    ConfigSpec config1;
    config1.value = 1;
    config1.description = "mock-config-1";
    InterfaceSpec iface;
    iface.number = 0;
    iface.klass = MOCK_VENDOR_INTERFACE_CLASS;
    AltSettingSpec alt0;
    alt0.altSetting = 0;
    EndpointSpec bulkIn;
    bulkIn.address = MOCK_BULK_IN_ENDPOINT;
    bulkIn.attributes = MOCK_ENDPOINT_ATTR_BULK;
    bulkIn.maxPacketSize = MOCK_BULK_MAX_PACKET_SIZE;
    bulkIn.mode = DataMode::PATTERN;
    EndpointSpec bulkOut;
    bulkOut.address = MOCK_BULK_OUT_ENDPOINT;
    bulkOut.attributes = MOCK_ENDPOINT_ATTR_BULK;
    bulkOut.maxPacketSize = MOCK_BULK_MAX_PACKET_SIZE;
    alt0.endpoints.push_back(bulkIn);
    alt0.endpoints.push_back(bulkOut);
    AltSettingSpec alt1;
    alt1.altSetting = 1;
    EndpointSpec intIn;
    intIn.address = MOCK_INTERRUPT_IN_ENDPOINT;
    intIn.attributes = MOCK_ENDPOINT_ATTR_INTERRUPT;
    intIn.maxPacketSize = MOCK_INTERRUPT_MAX_PACKET_SIZE;
    intIn.interval = MOCK_INTERRUPT_INTERVAL;
    intIn.maxInLength = MOCK_INTERRUPT_MAX_PACKET_SIZE;
    EndpointSpec intOut;
    intOut.address = MOCK_INTERRUPT_OUT_ENDPOINT;
    intOut.attributes = MOCK_ENDPOINT_ATTR_INTERRUPT;
    intOut.maxPacketSize = MOCK_INTERRUPT_MAX_PACKET_SIZE;
    intOut.interval = MOCK_INTERRUPT_INTERVAL;
    alt1.endpoints.push_back(intIn);
    alt1.endpoints.push_back(intOut);
    iface.altSettings.push_back(alt0);
    iface.altSettings.push_back(alt1);
    config1.interfaces.push_back(iface);
    spec.configs.push_back(config1);

    ConfigSpec config2;
    config2.value = MOCK_SECOND_CONFIG_VALUE;
    config2.description = "mock-config-2";
    InterfaceSpec iface2;
    iface2.number = 0;
    iface2.klass = MOCK_VENDOR_INTERFACE_CLASS;
    AltSettingSpec alt2;
    alt2.altSetting = 0;
    EndpointSpec bulkIn2;
    bulkIn2.address = MOCK_BULK_IN_ENDPOINT;
    bulkIn2.attributes = MOCK_ENDPOINT_ATTR_BULK;
    bulkIn2.maxPacketSize = MOCK_BULK_MAX_PACKET_SIZE_FS;
    bulkIn2.mode = DataMode::FIXED;
    bulkIn2.fixed = MOCK_FIXED_PAYLOAD_INIT;
    EndpointSpec bulkOut2;
    bulkOut2.address = MOCK_BULK_OUT_ENDPOINT;
    bulkOut2.attributes = MOCK_ENDPOINT_ATTR_BULK;
    bulkOut2.maxPacketSize = MOCK_BULK_MAX_PACKET_SIZE_FS;
    alt2.endpoints.push_back(bulkIn2);
    alt2.endpoints.push_back(bulkOut2);
    iface2.altSettings.push_back(alt2);
    config2.interfaces.push_back(iface2);
    spec.configs.push_back(config2);
    return spec;
}

void Reset()
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_devices.clear();
    g_records.clear();
    g_seq = 0;
    g_pendingTransfers.clear();
    g_cancelledTransfers.clear();
    g_submitTimes.clear();
    g_hotplugCallbacks.clear();
    g_nextHotplugHandle = 1;
    g_autoComplete = true;
    g_injectedFailures.clear();
    g_transferDelayMs = 0;
    g_inFlight.clear();
    g_closedHandles.clear();
    g_closeWhileInFlight = 0;
    g_useAfterClose = 0;
    g_blockApi = mock_usb::MockApi::OPEN;
    g_blockArmed = false;
    g_blockReleased = false;
    g_waitingCalls = 0;
}

int32_t AddDevice(const DeviceSpec &spec)
{
    std::lock_guard<std::mutex> lock(g_lock);
    auto device = std::make_unique<MockDevice>();
    device->spec = spec;
    device->dev.index = static_cast<int>(g_devices.size());
    for (const auto &configSpec : spec.configs) {
        MockConfig config;
        config.spec = configSpec;
        for (const auto &ifaceSpec : configSpec.interfaces) {
            MockInterface iface;
            iface.spec = ifaceSpec;
            for (const auto &altSpec : ifaceSpec.altSettings) {
                MockAlt alt;
                alt.spec = altSpec;
                for (const auto &epSpec : altSpec.endpoints) {
                    MockEndpoint ep;
                    ep.spec = epSpec;
                    alt.epSpecs.push_back(ep);
                }
                iface.altSpecs.push_back(alt);
            }
            config.interfaceSpecs.push_back(iface);
        }
        device->configs.push_back(config);
    }
    BuildDeviceDescriptors(*device);
    g_devices.push_back(std::move(device));
    return static_cast<int32_t>(g_devices.size() - 1);
}

int32_t SetEndpointBehavior(uint8_t bus, uint8_t address, uint8_t endpoint, const EndpointSpec &behavior)
{
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDevice(bus, address);
    if (device == nullptr) {
        return -1;
    }
    EndpointSpec *spec = FindEndpointSpec(*device, endpoint);
    if (spec == nullptr) {
        return -1;
    }
    /* Keep the stored endpoint address consistent with the key it is stored under. */
    EndpointSpec updated = behavior;
    updated.address = endpoint;
    *spec = updated;
    return 0;
}

int32_t SetControlResponse(uint8_t bus, uint8_t address, int32_t requestType, int32_t request,
    const std::vector<uint8_t> &payload)
{
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDevice(bus, address);
    if (device == nullptr) {
        return -1;
    }
    device->controlResponses[(requestType << MOCK_BYTE_BITS) | request] = payload;
    return 0;
}

int32_t RaiseHotplug(uint8_t bus, uint8_t address, bool arrived)
{
    std::vector<HotplugRegistration> targets;
    libusb_device *devicePtr = nullptr;
    libusb_hotplug_event event =
        arrived ? LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED : LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        MockDevice *device = FindDevice(bus, address);
        if (device == nullptr) {
            return -1;
        }
        /* keep the device table consistent with the event: a left device is no
         * longer enumerated, an arrived device is visible again. */
        device->present = arrived;
        TransferRecord record;
        AllocRecord(record, device, "hotplug");
        record.ret = arrived ? 1 : 0;
        AddRecord(record);
        devicePtr = &device->dev;
        for (const auto &registration : g_hotplugCallbacks) {
            if ((registration.events & event) != 0) {
                targets.push_back(registration);
            }
        }
    }
    if (targets.empty()) {
        /* the device table changed, but nobody is subscribed to this event */
        return -1;
    }
    /* Never invoke callbacks while holding the lock: they may call
     * back into the mock (libusb_get_bus_number and friends). */
    for (const auto &registration : targets) {
        registration.callback(&g_context, devicePtr, event, registration.userData);
    }
    return 0;
}

int32_t SetDevicePresent(uint8_t bus, uint8_t address, bool present)
{
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDevice(bus, address);
    if (device == nullptr) {
        return -1;
    }
    device->present = present;
    return 0;
}

void ClearRecords()
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_records.clear();
}

void SetAutoComplete(bool enabled)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_autoComplete = enabled;
}

void FailNextCall(MockApi api, int32_t errorCode, int32_t times)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_injectedFailures[static_cast<int32_t>(api)] = {times, errorCode};
}

void SetTransferDelayMs(uint32_t delayMs)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_transferDelayMs = delayMs;
}

int32_t CloseWhileInFlightCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_closeWhileInFlight;
}

int32_t UseAfterCloseCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_useAfterClose;
}

int32_t InFlightTransferCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t total = 0;
    for (const auto &entry : g_inFlight) {
        total += entry.second;
    }
    return total;
}

void ClearHazardCounters()
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_closeWhileInFlight = 0;
    g_useAfterClose = 0;
}

void BlockNextCall(MockApi api)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_blockApi = api;
    g_blockArmed = true;
    g_blockReleased = false;
}

int32_t WaitingCallCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_waitingCalls;
}

void ReleaseBlockedCall()
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_blockReleased = true;
    g_blockCv.notify_all();
}

size_t PendingTransferCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_pendingTransfers.size();
}

size_t RecordCount()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_records.size();
}

TransferRecord RecordAt(size_t index)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (index >= g_records.size()) {
        return TransferRecord();
    }
    return g_records[index];
}

void SetDumpEnabled(bool enabled)
{
    g_dumpEnabled = enabled;
}

bool DumpEnabled()
{
    return g_dumpEnabled;
}

void Dump(FILE *out)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (out == nullptr) {
        return;
    }
    (void)fprintf(out, "\n===== mock libusb transfer log (%zu entries) =====\n", g_records.size());
    std::map<std::string, size_t> summary;
    for (const auto &record : g_records) {
        summary[record.kind]++;
        (void)fprintf(out, "#%-4llu %-16s bus=%u addr=%u", static_cast<unsigned long long>(record.seq),
            record.kind.c_str(), record.bus, record.address);
        if (record.endpoint != 0) {
            (void)fprintf(out, " ep=0x%02x", record.endpoint);
        }
        if (record.request != 0 || record.requestType != 0) {
            (void)fprintf(out, " reqType=0x%02x req=0x%02x value=0x%04x index=%d", record.requestType,
                record.request, record.value, record.index);
        }
        if (record.length != 0) {
            (void)fprintf(out, " len=%d actual=%d", record.length, record.actual);
        }
        (void)fprintf(out, " ret=%d", record.ret);
        if (!record.data.empty() && record.data != "-") {
            (void)fprintf(out, " data=[%s]", record.data.c_str());
        }
        (void)fprintf(out, "\n");
    }
    (void)fprintf(out, "----- summary -----\n");
    for (const auto &entry : summary) {
        (void)fprintf(out, "  %-18s %zu\n", entry.first.c_str(), entry.second);
    }
    (void)fprintf(out, "===== end of mock libusb transfer log =====\n\n");
}
} // namespace mock_usb

/* ------------------------------------------------------------------------- */
/* libusb API implementation                                                 */
/* ------------------------------------------------------------------------- */

int libusb_init(libusb_context **ctx)
{
    if (ctx != nullptr) {
        *ctx = &g_context;
    }
    return 0;
}

void libusb_exit(libusb_context *ctx)
{
    (void)ctx;
}

int libusb_set_option(libusb_context *ctx, enum libusb_option option, ...)
{
    (void)ctx;
    (void)option;
    return 0;
}

void libusb_set_log_cb(libusb_context *ctx, libusb_log_cb cb, int mode)
{
    (void)ctx;
    (void)cb;
    (void)mode;
}

const char *libusb_strerror(int code)
{
    switch (code) {
        case LIBUSB_SUCCESS:
            return "Success";
        case LIBUSB_ERROR_IO:
            return "Input/output error";
        case LIBUSB_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case LIBUSB_ERROR_NO_DEVICE:
            return "No such device";
        case LIBUSB_ERROR_NOT_FOUND:
            return "Not found";
        case LIBUSB_ERROR_TIMEOUT:
            return "Timeout";
        case LIBUSB_ERROR_PIPE:
            return "Pipe error";
        case LIBUSB_ERROR_NOT_SUPPORTED:
            return "Not supported";
        default:
            return "Mock error";
    }
}

ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
    (void)ctx;
    std::lock_guard<std::mutex> lock(g_lock);
    libusb_device **devices = static_cast<libusb_device **>(malloc(sizeof(libusb_device *) *
        (g_devices.size() + 1)));
    if (devices == nullptr) {
        return LIBUSB_ERROR_NO_MEM;
    }
    size_t count = 0;
    for (size_t i = 0; i < g_devices.size(); i++) {
        if (!g_devices[i]->present) {
            continue; /* the device has been unplugged */
        }
        devices[count++] = &g_devices[i]->dev;
    }
    devices[count] = nullptr;
    *list = devices;
    return static_cast<ssize_t>(count);
}

void libusb_free_device_list(libusb_device **list, int unref_devices)
{
    (void)unref_devices;
    free(list);
}

libusb_device *libusb_ref_device(libusb_device *dev)
{
    return dev;
}

void libusb_unref_device(libusb_device *dev)
{
    (void)dev;
}

uint8_t libusb_get_bus_number(libusb_device *dev)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || dev->index < 0 || static_cast<size_t>(dev->index) >= g_devices.size()) {
        return 0;
    }
    return g_devices[static_cast<size_t>(dev->index)]->spec.bus;
}

uint8_t libusb_get_device_address(libusb_device *dev)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || dev->index < 0 || static_cast<size_t>(dev->index) >= g_devices.size()) {
        return 0;
    }
    return g_devices[static_cast<size_t>(dev->index)]->spec.address;
}

int libusb_get_device_speed(libusb_device *dev)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || dev->index < 0 || static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_SPEED_UNKNOWN;
    }
    return g_devices[static_cast<size_t>(dev->index)]->spec.speed;
}

int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || desc == nullptr || dev->index < 0 ||
        static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    *desc = g_devices[static_cast<size_t>(dev->index)]->desc;
    return 0;
}

int libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index, struct libusb_config_descriptor **config)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || config == nullptr || dev->index < 0 ||
        static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    MockDevice *device = g_devices[static_cast<size_t>(dev->index)].get();
    if (config_index >= device->configs.size()) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    *config = &device->configs[config_index].desc;
    return 0;
}

int libusb_get_active_config_descriptor(libusb_device *dev, struct libusb_config_descriptor **config)
{
    /* Blocking point used by the stale handle test: at this moment the adapter has
     * already obtained the raw device handle but has not started the transfer yet. */
    MaybeBlockCall(mock_usb::MockApi::ACTIVE_CONFIG_DESCRIPTOR);
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || config == nullptr || dev->index < 0 ||
        static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    MockDevice *device = g_devices[static_cast<size_t>(dev->index)].get();
    for (auto &cfg : device->configs) {
        if (cfg.spec.value == device->spec.activeConfigValue) {
            *config = &cfg.desc;
            return 0;
        }
    }
    if (device->configs.empty()) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    *config = &device->configs[0].desc;
    return 0;
}

void libusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
    (void)config; /* descriptors are owned by the mock device model */
}

int libusb_get_configuration(libusb_device_handle *dev_handle, int *config)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::GET_CONFIGURATION);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr || config == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    *config = device->spec.activeConfigValue;
    return 0;
}

int libusb_set_configuration(libusb_device_handle *dev_handle, int configuration)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::SET_CONFIGURATION);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    for (const auto &cfg : device->configs) {
        if (cfg.spec.value == configuration) {
            device->spec.activeConfigValue = static_cast<uint8_t>(configuration);
            return 0;
        }
    }
    return LIBUSB_ERROR_NOT_FOUND;
}

libusb_device *libusb_get_device(libusb_device_handle *dev_handle)
{
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDeviceByHandle(dev_handle);
    return device == nullptr ? nullptr : &device->dev;
}

int libusb_open(libusb_device *dev, libusb_device_handle **dev_handle)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::OPEN);
    if (injected != 0) {
        return injected;
    }
    if (dev == nullptr || dev_handle == nullptr || dev->index < 0 ||
        static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (!g_devices[static_cast<size_t>(dev->index)]->present) {
        /* A device that has been unplugged cannot be opened, as in the real stack. */
        return LIBUSB_ERROR_NO_DEVICE;
    }
    libusb_device_handle *handle = new (std::nothrow) libusb_device_handle();
    if (handle == nullptr) {
        return LIBUSB_ERROR_NO_MEM;
    }
    handle->index = dev->index;
    *dev_handle = handle;
    return 0;
}

void libusb_close(libusb_device_handle *dev_handle)
{
    if (dev_handle == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_lock);
        auto inFlight = g_inFlight.find(dev_handle);
        if (inFlight != g_inFlight.end() && inFlight->second > 0) {
            /* The real stack would free the handle here and any running transfer
             * would touch freed memory. Record the hazard and quarantine the
             * handle instead of freeing it so the test process stays alive. */
            g_closeWhileInFlight++;
        }
        MockDevice *device = FindDeviceByHandle(dev_handle);
        if (device != nullptr) {
            TransferRecord record;
            AllocRecord(record, device, "close");
            AddRecord(record);
        }
        g_closedHandles.insert(dev_handle);
    }
}

int libusb_kernel_driver_active(libusb_device_handle *dev_handle, int interface_number)
{
    (void)dev_handle;
    (void)interface_number;
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::KERNEL_DRIVER_ACTIVE);
    if (injected != 0) {
        return injected;
    }
    return 0;
}

int libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::DETACH_KERNEL_DRIVER);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (!InterfaceExists(*device, interface_number)) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    return 0;
}

int libusb_attach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::ATTACH_KERNEL_DRIVER);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (!InterfaceExists(*device, interface_number)) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    return 0;
}

int libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev_handle, int enable)
{
    (void)dev_handle;
    (void)enable;
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::SET_AUTO_DETACH_KERNEL_DRIVER);
    if (injected != 0) {
        return injected;
    }
    return 0;
}

int libusb_claim_interface(libusb_device_handle *dev_handle, int interface_number)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::CLAIM_INTERFACE);
    if (injected != 0) {
        return injected;
    }
    if (interface_number < 0 || interface_number >= MOCK_MAX_INTERFACES) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (!InterfaceExists(*device, interface_number)) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    /* A repeated claim of the same interface succeeds as a no-op, as upstream does. */
    dev_handle->claimedInterfaces |= (1UL << interface_number);
    TransferRecord record;
    AllocRecord(record, device, "claim");
    record.ret = interface_number;
    AddRecord(record);
    return 0;
}

int libusb_release_interface(libusb_device_handle *dev_handle, int interface_number)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::RELEASE_INTERFACE);
    if (injected != 0) {
        return injected;
    }
    if (interface_number < 0 || interface_number >= MOCK_MAX_INTERFACES) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (!InterfaceExists(*device, interface_number)) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    /* Upstream reports NOT_FOUND when the interface is not claimed by this handle. */
    if ((dev_handle->claimedInterfaces & (1UL << interface_number)) == 0) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    dev_handle->claimedInterfaces &= ~(1UL << interface_number);
    TransferRecord record;
    AllocRecord(record, device, "release");
    record.ret = interface_number;
    AddRecord(record);
    return 0;
}

int libusb_set_interface_alt_setting(libusb_device_handle *dev_handle, int interface_number, int alternate_setting)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::SET_ALT_SETTING);
    if (injected != 0) {
        return injected;
    }
    if (interface_number < 0 || interface_number >= MOCK_MAX_INTERFACES ||
        alternate_setting < 0 || alternate_setting > MOCK_BYTE_MASK) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    /* Upstream requires the interface to be claimed before its alt setting can be set. */
    if ((dev_handle->claimedInterfaces & (1UL << interface_number)) == 0) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    /* Same validation as the real stack: the alt setting must exist in the active
     * configuration, otherwise NOT_FOUND is reported. */
    bool altFound = false;
    MockConfig *config = ActiveConfig(*device);
    if (config != nullptr) {
        for (const auto &iface : config->interfaceSpecs) {
            if (iface.spec.number != interface_number) {
                continue;
            }
            for (const auto &alt : iface.altSpecs) {
                if (alt.spec.altSetting == alternate_setting) {
                    altFound = true;
                    break;
                }
            }
        }
    }
    if (!altFound) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    TransferRecord record;
    AllocRecord(record, device, "set-alt");
    record.index = interface_number;
    record.ret = alternate_setting;
    AddRecord(record);
    return 0;
}

int libusb_clear_halt(libusb_device_handle *dev_handle, unsigned char endpoint)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::CLEAR_HALT);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    EndpointSpec *spec = FindEndpointSpec(*device, endpoint);
    if (spec == nullptr) {
        /* Upstream fails the clear-feature request for an endpoint that is not part
         * of the (active) descriptors; the exact errno is backend dependent. */
        TransferRecord record;
        AllocRecord(record, device, "clear-halt");
        record.endpoint = endpoint;
        record.ret = LIBUSB_ERROR_NOT_FOUND;
        AddRecord(record);
        return LIBUSB_ERROR_NOT_FOUND;
    }
    spec->failInTimes = 0;
    spec->failOutTimes = 0;
    TransferRecord record;
    AllocRecord(record, device, "clear-halt");
    record.endpoint = endpoint;
    AddRecord(record);
    return 0;
}

int libusb_reset_device(libusb_device_handle *dev_handle)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::RESET_DEVICE);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    TransferRecord record;
    AllocRecord(record, device, "reset");
    AddRecord(record);
    return 0;
}

int libusb_get_max_packet_size(libusb_device *dev, unsigned char endpoint)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::GET_MAX_PACKET_SIZE);
    if (injected != 0) {
        return injected;
    }
    if (dev == nullptr || dev->index < 0 || static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    EndpointSpec *spec = FindEndpointSpec(*g_devices[static_cast<size_t>(dev->index)], endpoint);
    return spec == nullptr ? LIBUSB_ERROR_NOT_FOUND : spec->maxPacketSize;
}

int libusb_get_max_iso_packet_size(libusb_device *dev, unsigned char endpoint)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dev == nullptr || dev->index < 0 || static_cast<size_t>(dev->index) >= g_devices.size()) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    EndpointSpec *spec = FindEndpointSpec(*g_devices[static_cast<size_t>(dev->index)], endpoint);
    if (spec == nullptr) {
        return LIBUSB_ERROR_NOT_FOUND;
    }
    /* Same parsing as upstream get_endpoint_max_packet_size() for non SuperSpeed
     * devices: bits 0..10 hold the size, bits 11..12 the high bandwidth multiplier. */
    int32_t value = spec->maxPacketSize & MOCK_MAX_PACKET_SIZE_MASK;
    int32_t type = spec->attributes & MOCK_ENDPOINT_TYPE_MASK;
    if (type == LIBUSB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS || type == LIBUSB_ENDPOINT_TRANSFER_TYPE_INTERRUPT) {
        value = value * (1 + ((spec->maxPacketSize >> MOCK_HIGH_BANDWIDTH_SHIFT) & MOCK_HIGH_BANDWIDTH_MASK));
    }
    return value;
}

int libusb_control_transfer(libusb_device_handle *dev_handle, uint8_t request_type, uint8_t request, uint16_t value,
    uint16_t index, unsigned char *data, uint16_t length, unsigned int timeout)
{
    auto start = std::chrono::steady_clock::now();
    InFlightGuard inFlight(dev_handle);
    SleepTransferDelay();
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::CONTROL_TRANSFER);
    if (injected != 0) {
        return injected;
    }
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        TransferRecord record;
        AllocRecord(record, nullptr, "control");
        record.ret = LIBUSB_ERROR_NO_DEVICE;
        AddRecord(record);
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (IsTimedOut(start, timeout)) {
        TransferRecord record;
        AllocRecord(record, device, "control");
        record.length = length;
        record.ret = LIBUSB_ERROR_TIMEOUT;
        AddRecord(record);
        return LIBUSB_ERROR_TIMEOUT;
    }
    return ControlTransferLocked(*device, request_type, request, value, index, data, length);
}

int libusb_bulk_transfer(libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data, int length,
    int *transferred, unsigned int timeout)
{
    auto start = std::chrono::steady_clock::now();
    InFlightGuard inFlight(dev_handle);
    SleepTransferDelay();
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        TransferRecord record;
        AllocRecord(record, nullptr, "bulk");
        record.ret = LIBUSB_ERROR_NO_DEVICE;
        AddRecord(record);
        if (transferred != nullptr) {
            *transferred = 0;
        }
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (IsTimedOut(start, timeout)) {
        TransferRecord record;
        AllocRecord(record, device, "bulk");
        record.endpoint = endpoint;
        record.length = length;
        record.ret = LIBUSB_ERROR_TIMEOUT;
        AddRecord(record);
        if (transferred != nullptr) {
            *transferred = 0;
        }
        return LIBUSB_ERROR_TIMEOUT;
    }
    return BulkTransferLocked(*device, endpoint, data, length, transferred,
        (endpoint & ENDPOINT_DIR_MASK_MOCK) != 0 ? "bulk-in" : "bulk-out");
}

int libusb_interrupt_transfer(libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data,
    int length, int *transferred, unsigned int timeout)
{
    auto start = std::chrono::steady_clock::now();
    InFlightGuard inFlight(dev_handle);
    SleepTransferDelay();
    std::lock_guard<std::mutex> lock(g_lock);
    MockDevice *device = FindDeviceByHandle(dev_handle);
    if (device == nullptr) {
        if (transferred != nullptr) {
            *transferred = 0;
        }
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (IsTimedOut(start, timeout)) {
        TransferRecord record;
        AllocRecord(record, device, "interrupt");
        record.endpoint = endpoint;
        record.length = length;
        record.ret = LIBUSB_ERROR_TIMEOUT;
        AddRecord(record);
        if (transferred != nullptr) {
            *transferred = 0;
        }
        return LIBUSB_ERROR_TIMEOUT;
    }
    return BulkTransferLocked(*device, endpoint, data, length, transferred,
        (endpoint & ENDPOINT_DIR_MASK_MOCK) != 0 ? "interrupt-in" : "interrupt-out");
}

struct libusb_transfer *libusb_alloc_transfer(int iso_packets)
{
    int count = iso_packets > 0 ? iso_packets : 0;
    /* struct libusb_transfer already embeds one iso packet descriptor. */
    size_t extraPackets = count > 1 ? static_cast<size_t>(count - 1) : 0;
    size_t size = sizeof(struct libusb_transfer) +
        sizeof(struct libusb_iso_packet_descriptor) * extraPackets;
    struct libusb_transfer *transfer = static_cast<struct libusb_transfer *>(calloc(1, size));
    if (transfer != nullptr) {
        transfer->num_iso_packets = count;
    }
    return transfer;
}

void libusb_free_transfer(struct libusb_transfer *transfer)
{
    if (transfer != nullptr) {
        if ((transfer->flags & LIBUSB_TRANSFER_FREE_BUFFER) != 0) {
            free(transfer->buffer);
        }
        std::lock_guard<std::mutex> lock(g_lock);
        /* Forget any bookkeeping for this address: a later allocation may reuse it. */
        g_cancelledTransfers.erase(transfer);
        g_submitTimes.erase(transfer);
        auto pending = std::find(g_pendingTransfers.begin(), g_pendingTransfers.end(), transfer);
        if (pending != g_pendingTransfers.end()) {
            g_pendingTransfers.erase(pending);
            auto inFlight = g_inFlight.find(transfer->dev_handle);
            if (inFlight != g_inFlight.end() && --inFlight->second <= 0) {
                g_inFlight.erase(inFlight);
            }
        }
    }
    free(transfer);
}

int libusb_submit_transfer(struct libusb_transfer *transfer)
{
    std::lock_guard<std::mutex> lock(g_lock);
    int32_t injected = TakeInjectedFailure(mock_usb::MockApi::SUBMIT_TRANSFER);
    if (injected != 0) {
        return injected;
    }
    if (transfer == nullptr || transfer->dev_handle == nullptr) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    if (std::find(g_pendingTransfers.begin(), g_pendingTransfers.end(), transfer) != g_pendingTransfers.end()) {
        /* Upstream refuses a second submit while this transfer is still in flight. */
        return LIBUSB_ERROR_BUSY;
    }
    if (g_closedHandles.count(transfer->dev_handle) > 0) {
        /* Same hazard as InFlightGuard: the handle was released before the submit. */
        g_useAfterClose++;
    }
    MockDevice *device = FindDeviceByHandle(transfer->dev_handle);
    if (device == nullptr || !device->present) {
        /* An unplugged device rejects new transfers, as in the real stack. */
        TransferRecord record;
        AllocRecord(record, device, "submit");
        record.endpoint = transfer->endpoint;
        record.length = transfer->length;
        record.ret = LIBUSB_ERROR_NO_DEVICE;
        AddRecord(record);
        return LIBUSB_ERROR_NO_DEVICE;
    }
    TransferRecord record;
    AllocRecord(record, device, "submit");
    record.endpoint = transfer->endpoint;
    record.length = transfer->length;
    AddRecord(record);
    g_inFlight[transfer->dev_handle]++;
    g_pendingTransfers.push_back(transfer);
    g_submitTimes[transfer] = std::chrono::steady_clock::now();
    return 0;
}

int libusb_cancel_transfer(struct libusb_transfer *transfer)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (transfer == nullptr) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    bool inFlight = std::find(g_pendingTransfers.begin(), g_pendingTransfers.end(), transfer) !=
        g_pendingTransfers.end();
    bool cancelling = g_cancelledTransfers.count(transfer) > 0;
    if (!inFlight || cancelling) {
        /* Upstream reports NOT_FOUND for a transfer that is not in flight anymore. */
        return LIBUSB_ERROR_NOT_FOUND;
    }
    if (g_closedHandles.count(transfer->dev_handle) > 0) {
        /* The handle is gone; upstream reports NO_DEVICE for a disconnected device. */
        return LIBUSB_ERROR_NO_DEVICE;
    }
    MockDevice *device = FindDeviceByHandle(transfer->dev_handle);
    TransferRecord record;
    AllocRecord(record, device, "cancel");
    record.endpoint = transfer->endpoint;
    AddRecord(record);
    g_cancelledTransfers.insert(transfer);
    return 0;
}

int libusb_handle_events_completed(libusb_context *ctx, int *completed)
{
    (void)ctx;
    if (completed != nullptr && *completed != 0) {
        /* Upstream returns as soon as the completion flag is set. */
        return 0;
    }
    libusb_transfer *transfer = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        int32_t injected = TakeInjectedFailure(mock_usb::MockApi::HANDLE_EVENTS);
        if (injected != 0) {
            return injected;
        }
        if (!g_autoComplete) {
            transfer = nullptr;
        } else if (!g_pendingTransfers.empty()) {
            transfer = g_pendingTransfers.front();
            g_pendingTransfers.erase(g_pendingTransfers.begin());
        }
    }
    if (transfer == nullptr) {
        usleep(MOCK_EVENT_POLL_INTERVAL_MS * MOCK_US_PER_MS);
        return 0;
    }
    /* The delay models a device that only answers after a while: it must not be applied
     * while the adapter still holds a lock that would block CloseDevice. */
    SleepTransferDelay();
    libusb_transfer_cb_fn callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        bool timedOut = false;
        auto submitTime = g_submitTimes.find(transfer);
        if (submitTime != g_submitTimes.end()) {
            timedOut = IsTimedOut(submitTime->second, transfer->timeout);
            g_submitTimes.erase(submitTime);
        }
        CompleteTransferLocked(transfer, timedOut);
        callback = transfer->callback;
        auto inFlight = g_inFlight.find(transfer->dev_handle);
        if (inFlight != g_inFlight.end() && --inFlight->second <= 0) {
            g_inFlight.erase(inFlight);
        }
    }
    if (callback != nullptr) {
        callback(transfer);
    }
    if ((transfer->flags & LIBUSB_TRANSFER_FREE_TRANSFER) != 0) {
        libusb_free_transfer(transfer);
    }
    return 0;
}

int libusb_hotplug_register_callback(libusb_context *ctx, int events, int flags, int vendor_id, int product_id,
    int dev_class, libusb_hotplug_callback_fn cb_fn, void *user_data, libusb_hotplug_callback_handle *callback_handle)
{
    (void)ctx;
    (void)vendor_id;
    (void)product_id;
    (void)dev_class;
    const int validEvents = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
    if (events == 0 || (events & ~validEvents) != 0 || (flags & ~LIBUSB_HOTPLUG_ENUMERATE) != 0 ||
        cb_fn == nullptr) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    int handle = 0;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        int32_t injected = TakeInjectedFailure(mock_usb::MockApi::HOTPLUG_REGISTER);
        if (injected != 0) {
            return injected;
        }
        handle = g_nextHotplugHandle;
        g_nextHotplugHandle++;
        HotplugRegistration registration = {handle, events, cb_fn, user_data};
        g_hotplugCallbacks.push_back(registration);
    }
    if (callback_handle != nullptr) {
        *callback_handle = handle;
    }
    /* Like upstream with LIBUSB_HOTPLUG_ENUMERATE, first report the devices that
     * are already present; invoked outside of the mock lock. */
    if ((flags & LIBUSB_HOTPLUG_ENUMERATE) != 0 && (events & LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) != 0) {
        std::vector<libusb_device *> present;
        {
            std::lock_guard<std::mutex> lock(g_lock);
            for (auto &device : g_devices) {
                if (device->present) {
                    present.push_back(&device->dev);
                }
            }
        }
        for (auto *device : present) {
            cb_fn(&g_context, device, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, user_data);
        }
    }
    return 0;
}

void libusb_hotplug_deregister_callback(libusb_context *ctx, libusb_hotplug_callback_handle callback_handle)
{
    (void)ctx;
    std::lock_guard<std::mutex> lock(g_lock);
    for (auto it = g_hotplugCallbacks.begin(); it != g_hotplugCallbacks.end(); ++it) {
        if (it->handle == callback_handle) {
            g_hotplugCallbacks.erase(it);
            return;
        }
    }
}
