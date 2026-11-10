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
/* pipe(16 bytes: 8 + 4 + 1 + 3 padding) + size(4) + offset(4) + length(4) */
constexpr size_t MIN_DATA_SIZE = sizeof(UsbRequestPipe) + sizeof(uint32_t) * 3;
constexpr size_t PIPE_PADDING = 3;

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
    size_t pos = 0;
    if (memcpy_s(&pipe.interfaceHandle, sizeof(pipe.interfaceHandle), data + pos,
        sizeof(pipe.interfaceHandle)) != EOK) {
        HDF_LOGE("%{public}s: read interfaceHandle failed", __func__);
        return false;
    }
    pos += sizeof(pipe.interfaceHandle);
    if (memcpy_s(&pipe.timeout, sizeof(pipe.timeout), data + pos, sizeof(pipe.timeout)) != EOK) {
        HDF_LOGE("%{public}s: read timeout failed", __func__);
        return false;
    }
    pos += sizeof(pipe.timeout);
    if (memcpy_s(&pipe.endpoint, sizeof(pipe.endpoint), data + pos, sizeof(pipe.endpoint)) != EOK) {
        HDF_LOGE("%{public}s: read endpoint failed", __func__);
        return false;
    }
    pos += sizeof(pipe.endpoint) + PIPE_PADDING;
    uint32_t bufferSize = 0;
    if (memcpy_s(&bufferSize, sizeof(bufferSize), data + pos, sizeof(bufferSize)) != EOK) {
        HDF_LOGE("%{public}s: read bufferSize failed", __func__);
        return false;
    }
    pos += sizeof(bufferSize);
    uint32_t offset = 0;
    if (memcpy_s(&offset, sizeof(offset), data + pos, sizeof(offset)) != EOK) {
        HDF_LOGE("%{public}s: read offset failed", __func__);
        return false;
    }
    pos += sizeof(offset);
    uint32_t length = 0;
    if (memcpy_s(&length, sizeof(length), data + pos, sizeof(length)) != EOK) {
        HDF_LOGE("%{public}s: read length failed", __func__);
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
