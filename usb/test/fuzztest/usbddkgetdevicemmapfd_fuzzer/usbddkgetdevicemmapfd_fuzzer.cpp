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
#include <unistd.h>

#include "hdf_log.h"
#include "securec.h"
#include "usbddkgetdevicemmapfd_fuzzer.h"
#include "v1_0/iusb_ddk.h"

using namespace OHOS::HDI::Usb::Ddk::V1_0;

namespace OHOS {
namespace UsbDdk {
constexpr size_t MIN_DATA_SIZE = sizeof(uint64_t);

bool UsbDdkGetDeviceMemMapFdFuzzTest(const uint8_t *data, size_t size)
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

    uint64_t deviceId = 0;
    if (memcpy_s(&deviceId, sizeof(deviceId), data, sizeof(deviceId)) != EOK) {
        HDF_LOGE("%{public}s: read deviceId failed", __func__);
        return false;
    }

    int fd = -1;
    int32_t ret = usbDdk->GetDeviceMemMapFd(deviceId, fd);
    HDF_LOGI("%{public}s: GetDeviceMemMapFd ret=%{public}d, deviceId=%{public}llu, fd=%{public}d", __func__, ret,
        static_cast<unsigned long long>(deviceId), fd);
    if (ret == HDF_SUCCESS && fd >= 0) {
        close(fd);
    }
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
    OHOS::UsbDdk::UsbDdkGetDeviceMemMapFdFuzzTest(data, size);
    return 0;
}
