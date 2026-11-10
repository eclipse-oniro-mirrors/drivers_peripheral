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

#include <cstddef>
#include <cstdint>

#include "hdf_log.h"
#include "securec.h"
#include "usbddksendpiperequest_fuzzer.h"
#include "v1_0/iusb_ddk.h"

using namespace OHOS::HDI::Usb::Ddk::V1_0;

namespace OHOS {
namespace UsbDdk {
/* pipe + size(4) + offset(4) + length(4) */
constexpr size_t MIN_DATA_SIZE = sizeof(UsbRequestPipe) + sizeof(uint32_t) * 3;

template <typename T>
bool ReadField(const uint8_t *data, size_t size, size_t &pos, T &field, const char *fieldName)
{
    if (pos + sizeof(T) > size || memcpy_s(&field, sizeof(T), data + pos, sizeof(T)) != EOK) {
        HDF_LOGE("%{public}s: read %{public}s failed", __func__, fieldName);
        return false;
    }
    pos += sizeof(T);
    return true;
}

bool UsbDdkSendPipeRequestFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size < MIN_DATA_SIZE) {
        HDF_LOGE("%{public}s: invalid data", __func__);
        return false;
    }

    sptr<IUsbDdk> usbDdk = IUsbDdk::Get();
    if (usbDdk == nullptr) {
        HDF_LOGE("%{public}s: get IUsbDdk failed", __func__);
        return false;
    }

    UsbRequestPipe pipe = {};
    if (memcpy_s(&pipe, sizeof(pipe), data, sizeof(pipe)) != EOK) {
        HDF_LOGE("%{public}s: read pipe failed", __func__);
        return false;
    }
    size_t pos = sizeof(pipe);

    uint32_t bufferSize = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    if (!ReadField(data, size, pos, bufferSize, "bufferSize") ||
        !ReadField(data, size, pos, offset, "offset") ||
        !ReadField(data, size, pos, length, "length")) {
        return false;
    }

    uint32_t transferredLength = 0;
    int32_t ret = usbDdk->SendPipeRequest(pipe, bufferSize, offset, length, transferredLength);
    HDF_LOGI("%{public}s: SendPipeRequest ret=%{public}d, interfaceHandle=%{public}llu, endpoint=%{public}u, "
        "size=%{public}u, offset=%{public}u, length=%{public}u, transferredLength=%{public}u",
        __func__, ret, static_cast<unsigned long long>(pipe.interfaceHandle), pipe.endpoint, bufferSize, offset,
        length, transferredLength);
    return true;
}
} // namespace UsbDdk
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < OHOS::UsbDdk::MIN_DATA_SIZE) {
        return 0;
    }
    OHOS::UsbDdk::UsbDdkSendPipeRequestFuzzTest(data, size);
    return 0;
}
