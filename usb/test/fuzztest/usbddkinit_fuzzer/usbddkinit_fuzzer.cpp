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
#include "usbddkinit_fuzzer.h"
#include "v1_0/iusb_ddk.h"

using namespace OHOS::HDI::Usb::Ddk::V1_0;

namespace OHOS {
namespace UsbDdk {
constexpr size_t MIN_DATA_SIZE = 1;

bool UsbDdkInitFuzzTest(const uint8_t *data, size_t size)
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

    /* Init has no input parameter. Repeated calls stress the listener setup and host sdk init paths. */
    int32_t ret = usbDdk->Init();
    HDF_LOGI("%{public}s: Init ret=%{public}d", __func__, ret);
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
    OHOS::UsbDdk::UsbDdkInitFuzzTest(data, size);
    return 0;
}
