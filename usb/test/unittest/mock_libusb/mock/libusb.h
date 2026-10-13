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
 * Mock libusb header.
 *
 * LibusbAdapter (and this test target) are compiled against this self contained
 * header instead of the upstream libusb one. Every symbol is provided by
 * mock_libusb.cpp, so the components under test can be built and executed
 * without the real libusb library, without a USB device and without any kernel
 * support.
 *
 * Only the API surface used by libusb_adapter.cpp is modelled, with the same
 * semantics as upstream libusb (status codes, transfer states, descriptors).
 */

#ifndef MOCK_LIBUSB_H
#define MOCK_LIBUSB_H

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "mock_libusb_control.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBUSB_CALL
#define LIBUSB_CALL
#endif

/* ---------- descriptor types and sizes ---------- */
#define LIBUSB_DT_DEVICE 0x01
#define LIBUSB_DT_CONFIG 0x02
#define LIBUSB_DT_STRING 0x03
#define LIBUSB_DT_INTERFACE 0x04
#define LIBUSB_DT_ENDPOINT 0x05
#define LIBUSB_DT_DEVICE_SIZE 18
#define LIBUSB_DT_CONFIG_SIZE 9
#define LIBUSB_DT_INTERFACE_SIZE 9
#define LIBUSB_DT_ENDPOINT_SIZE 7
#define LIBUSB_DT_ENDPOINT_AUDIO_SIZE 9

#define LIBUSB_CLASS_HUB 0x09

/* ---------- endpoint and request helpers ---------- */
#define LIBUSB_ENDPOINT_DIR_MASK 0x80
#define LIBUSB_ENDPOINT_IN 0x80
#define LIBUSB_ENDPOINT_OUT 0x00
#define LIBUSB_REQUEST_TYPE_STANDARD (0x00 << 5)
#define LIBUSB_REQUEST_TYPE_CLASS (0x01 << 5)
#define LIBUSB_REQUEST_TYPE_VENDOR (0x02 << 5)
#define LIBUSB_REQUEST_GET_DESCRIPTOR 0x06
#define LIBUSB_REQUEST_GET_STATUS 0x00
#define LIBUSB_REQUEST_SET_CONFIGURATION 0x09

/* ---------- error codes ---------- */
#define LIBUSB_SUCCESS 0
#define LIBUSB_ERROR_IO (-1)
#define LIBUSB_ERROR_INVALID_PARAM (-2)
#define LIBUSB_ERROR_ACCESS (-3)
#define LIBUSB_ERROR_NO_DEVICE (-4)
#define LIBUSB_ERROR_NOT_FOUND (-5)
#define LIBUSB_ERROR_BUSY (-6)
#define LIBUSB_ERROR_TIMEOUT (-7)
#define LIBUSB_ERROR_OVERFLOW (-8)
#define LIBUSB_ERROR_PIPE (-9)
#define LIBUSB_ERROR_INTERRUPTED (-10)
#define LIBUSB_ERROR_NO_MEM (-11)
#define LIBUSB_ERROR_NOT_SUPPORTED (-12)
#define LIBUSB_ERROR_OTHER (-99)

/* ---------- device speed ---------- */
enum libusb_speed {
    LIBUSB_SPEED_UNKNOWN = 0,
    LIBUSB_SPEED_LOW = 1,
    LIBUSB_SPEED_FULL = 2,
    LIBUSB_SPEED_HIGH = 3,
    LIBUSB_SPEED_SUPER = 4,
    LIBUSB_SPEED_SUPER_PLUS = 5,
    LIBUSB_SPEED_SUPER_PLUS_X2 = 6,
};

/* ---------- log ---------- */
enum libusb_log_level {
    LIBUSB_LOG_LEVEL_NONE = 0,
    LIBUSB_LOG_LEVEL_ERROR = 1,
    LIBUSB_LOG_LEVEL_WARNING = 2,
    LIBUSB_LOG_LEVEL_INFO = 3,
    LIBUSB_LOG_LEVEL_DEBUG = 4,
};

enum libusb_log_cb_mode {
    /* Same mode bits as upstream libusb (used by libusb_set_log_cb). */
    LIBUSB_LOG_CB_GLOBAL = (1 << 0),
    LIBUSB_LOG_CB_CONTEXT = (1 << 1),
};

enum libusb_option {
    LIBUSB_OPTION_LOG_LEVEL = 0,
    LIBUSB_OPTION_USE_USBDK = 1,
};

/* ---------- transfers ---------- */
enum libusb_transfer_status {
    LIBUSB_TRANSFER_COMPLETED = 0,
    LIBUSB_TRANSFER_ERROR = 1,
    LIBUSB_TRANSFER_TIMED_OUT = 2,
    LIBUSB_TRANSFER_CANCELLED = 3,
    LIBUSB_TRANSFER_STALL = 4,
    LIBUSB_TRANSFER_NO_DEVICE = 5,
    LIBUSB_TRANSFER_OVERFLOW = 6,
};

enum libusb_transfer_flags {
    LIBUSB_TRANSFER_SHORT_NOT_OK = 1 << 0,
    LIBUSB_TRANSFER_FREE_BUFFER = 1 << 1,
    LIBUSB_TRANSFER_FREE_TRANSFER = 1 << 2,
    LIBUSB_TRANSFER_ADD_ZERO_PACKET = 1 << 3,
};

enum libusb_transfer_type {
    LIBUSB_TRANSFER_TYPE_CONTROL = 0,
    LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 1,
    LIBUSB_TRANSFER_TYPE_BULK = 2,
    LIBUSB_TRANSFER_TYPE_INTERRUPT = 3,
    LIBUSB_TRANSFER_TYPE_BULK_STREAM = 4,
};

#define LIBUSB_TRANSFER_TYPE_MASK 0x03

/* Endpoint transfer type as encoded in bmAttributes (upstream libusb names). */
enum libusb_endpoint_transfer_type {
    LIBUSB_ENDPOINT_TRANSFER_TYPE_CONTROL = 0x0,
    LIBUSB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS = 0x1,
    LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK = 0x2,
    LIBUSB_ENDPOINT_TRANSFER_TYPE_INTERRUPT = 0x3,
};

/* ---------- hotplug ---------- */
enum libusb_hotplug_event {
    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED = 0x01,
    LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT = 0x02,
};

enum libusb_hotplug_flag {
    LIBUSB_HOTPLUG_NO_FLAGS = 0,
    LIBUSB_HOTPLUG_ENUMERATE = 1 << 0,
};

#define LIBUSB_HOTPLUG_MATCH_ANY (-1)

/* ---------- opaque handles ---------- */
struct libusb_context;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

typedef struct libusb_context libusb_context;
typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;
typedef struct libusb_transfer libusb_transfer;

typedef int libusb_hotplug_callback_handle;

/* ---------- descriptors ---------- */
struct libusb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

struct libusb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
    const unsigned char *extra;
    int extra_length;
};

struct libusb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
    const struct libusb_endpoint_descriptor *endpoint;
    const unsigned char *extra;
    int extra_length;
};

struct libusb_interface {
    const struct libusb_interface_descriptor *altsetting;
    int num_altsetting;
};

struct libusb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t MaxPower;
    const struct libusb_interface *interface;
    const unsigned char *extra;
    int extra_length;
};

struct libusb_iso_packet_descriptor {
    unsigned int length;
    unsigned int actual_length;
    enum libusb_transfer_status status;
};

typedef void (LIBUSB_CALL *libusb_transfer_cb_fn)(struct libusb_transfer *transfer);
typedef void (LIBUSB_CALL *libusb_log_cb)(libusb_context *ctx, enum libusb_log_level level, const char *str);
typedef int (LIBUSB_CALL *libusb_hotplug_callback_fn)(libusb_context *ctx, libusb_device *dev,
    enum libusb_hotplug_event event, void *user_data);

struct libusb_transfer {
    libusb_device_handle *dev_handle;
    uint8_t flags;
    unsigned char endpoint;
    unsigned char type;
    unsigned int timeout;
    enum libusb_transfer_status status;
    int length;
    int actual_length;
    libusb_transfer_cb_fn callback;
    void *user_data;
    unsigned char *buffer;
    int num_iso_packets;
    struct libusb_iso_packet_descriptor iso_packet_desc[1];
};

/* ---------- transfer helpers (same behaviour as upstream inline API) ---------- */
static inline void libusb_fill_bulk_transfer(struct libusb_transfer *transfer, libusb_device_handle *dev_handle,
    unsigned char endpoint, unsigned char *buffer, int length, libusb_transfer_cb_fn callback, void *user_data,
    unsigned int timeout)
{
    transfer->dev_handle = dev_handle;
    transfer->endpoint = endpoint;
    transfer->type = LIBUSB_TRANSFER_TYPE_BULK;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->callback = callback;
    transfer->user_data = user_data;
    transfer->flags = 0;
}

static inline void libusb_fill_interrupt_transfer(struct libusb_transfer *transfer, libusb_device_handle *dev_handle,
    unsigned char endpoint, unsigned char *buffer, int length, libusb_transfer_cb_fn callback, void *user_data,
    unsigned int timeout)
{
    transfer->dev_handle = dev_handle;
    transfer->endpoint = endpoint;
    transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->callback = callback;
    transfer->user_data = user_data;
    transfer->flags = 0;
}

static inline void libusb_fill_iso_transfer(struct libusb_transfer *transfer, libusb_device_handle *dev_handle,
    unsigned char endpoint, unsigned char *buffer, int length, int num_iso_packets,
    libusb_transfer_cb_fn callback, void *user_data, unsigned int timeout)
{
    transfer->dev_handle = dev_handle;
    transfer->endpoint = endpoint;
    transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->num_iso_packets = num_iso_packets;
    transfer->callback = callback;
    transfer->user_data = user_data;
    transfer->flags = 0;
}

static inline void libusb_set_iso_packet_lengths(struct libusb_transfer *transfer, unsigned int length)
{
    for (int i = 0; i < transfer->num_iso_packets; i++) {
        transfer->iso_packet_desc[i].length = length;
    }
}

/* ---------- API ---------- */
int LIBUSB_CALL libusb_init(libusb_context **ctx);
void LIBUSB_CALL libusb_exit(libusb_context *ctx);
int LIBUSB_CALL libusb_set_option(libusb_context *ctx, enum libusb_option option, ...);
void LIBUSB_CALL libusb_set_log_cb(libusb_context *ctx, libusb_log_cb cb, int mode);
const char *LIBUSB_CALL libusb_strerror(int code);

ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *ctx, libusb_device ***list);
void LIBUSB_CALL libusb_free_device_list(libusb_device **list, int unref_devices);
libusb_device *LIBUSB_CALL libusb_ref_device(libusb_device *dev);
void LIBUSB_CALL libusb_unref_device(libusb_device *dev);
uint8_t LIBUSB_CALL libusb_get_bus_number(libusb_device *dev);
uint8_t LIBUSB_CALL libusb_get_device_address(libusb_device *dev);
int LIBUSB_CALL libusb_get_device_speed(libusb_device *dev);
int LIBUSB_CALL libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc);
int LIBUSB_CALL libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index,
    struct libusb_config_descriptor **config);
int LIBUSB_CALL libusb_get_active_config_descriptor(libusb_device *dev,
    struct libusb_config_descriptor **config);
void LIBUSB_CALL libusb_free_config_descriptor(struct libusb_config_descriptor *config);
int LIBUSB_CALL libusb_get_configuration(libusb_device_handle *dev_handle, int *config);
int LIBUSB_CALL libusb_set_configuration(libusb_device_handle *dev_handle, int configuration);
libusb_device *LIBUSB_CALL libusb_get_device(libusb_device_handle *dev_handle);
int LIBUSB_CALL libusb_get_max_packet_size(libusb_device *dev, unsigned char endpoint);
int LIBUSB_CALL libusb_get_max_iso_packet_size(libusb_device *dev, unsigned char endpoint);

int LIBUSB_CALL libusb_open(libusb_device *dev, libusb_device_handle **dev_handle);
void LIBUSB_CALL libusb_close(libusb_device_handle *dev_handle);
int LIBUSB_CALL libusb_kernel_driver_active(libusb_device_handle *dev_handle, int interface_number);
int LIBUSB_CALL libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number);
int LIBUSB_CALL libusb_attach_kernel_driver(libusb_device_handle *dev_handle, int interface_number);
int LIBUSB_CALL libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev_handle, int enable);
int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *dev_handle, int interface_number);
int LIBUSB_CALL libusb_release_interface(libusb_device_handle *dev_handle, int interface_number);
int LIBUSB_CALL libusb_set_interface_alt_setting(libusb_device_handle *dev_handle, int interface_number,
    int alternate_setting);
int LIBUSB_CALL libusb_clear_halt(libusb_device_handle *dev_handle, unsigned char endpoint);
int LIBUSB_CALL libusb_reset_device(libusb_device_handle *dev_handle);

int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *dev_handle, uint8_t request_type, uint8_t request,
    uint16_t value, uint16_t index, unsigned char *data, uint16_t length, unsigned int timeout);
int LIBUSB_CALL libusb_bulk_transfer(libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data,
    int length, int *transferred, unsigned int timeout);
int LIBUSB_CALL libusb_interrupt_transfer(libusb_device_handle *dev_handle, unsigned char endpoint,
    unsigned char *data, int length, int *transferred, unsigned int timeout);

struct libusb_transfer *LIBUSB_CALL libusb_alloc_transfer(int iso_packets);
void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *transfer);
int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *transfer);
int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *transfer);
int LIBUSB_CALL libusb_handle_events_completed(libusb_context *ctx, int *completed);

int LIBUSB_CALL libusb_hotplug_register_callback(libusb_context *ctx, int events, int flags, int vendor_id,
    int product_id, int dev_class, libusb_hotplug_callback_fn cb_fn, void *user_data,
    libusb_hotplug_callback_handle *callback_handle);
void LIBUSB_CALL libusb_hotplug_deregister_callback(libusb_context *ctx,
    libusb_hotplug_callback_handle callback_handle);

/* Timeout of the inline string descriptor wrapper (same value as upstream). */
#define LIBUSB_STRING_DESC_TIMEOUT_MS 1000
/* (LIBUSB_DT_STRING << LIBUSB_STRING_DESC_TYPE_SHIFT) | index is the wValue. */
#define LIBUSB_STRING_DESC_TYPE_SHIFT 8

/* Upstream keeps this as a static inline wrapper around libusb_control_transfer. */
static inline int libusb_get_string_descriptor(libusb_device_handle *dev_handle, uint8_t desc_index,
    uint16_t langid, unsigned char *data, int length)
{
    return libusb_control_transfer(dev_handle, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR,
        static_cast<uint16_t>((LIBUSB_DT_STRING << LIBUSB_STRING_DESC_TYPE_SHIFT) | desc_index),
        langid, data, static_cast<uint16_t>(length), LIBUSB_STRING_DESC_TIMEOUT_MS);
}

#ifdef __cplusplus
}
#endif

#endif // MOCK_LIBUSB_H
