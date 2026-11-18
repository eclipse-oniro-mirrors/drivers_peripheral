/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "camera_device_callback_wrapper.h"
#include <mutex>
#ifdef __cplusplus
extern "C" {
#endif

static OHOS::sptr<OHOS::Camera::ICameraDeviceCallback> g_deviceCallback;
static std::mutex g_deviceCallbackMutex;

void BindCameraDeviceCallback(const OHOS::sptr<OHOS::Camera::ICameraDeviceCallback>& callback)
{
    std::lock_guard<std::mutex> l(g_deviceCallbackMutex);
    g_deviceCallback = callback;
}

void DeviceCBOnError(int type, int errorMsg)
{
    OHOS::sptr<OHOS::Camera::ICameraDeviceCallback> callback;
    {
        std::lock_guard<std::mutex> l(g_deviceCallbackMutex);
        callback = g_deviceCallback;
    }
    if (callback == nullptr) {
        return;
    }

    callback->OnError(static_cast<OHOS::Camera::ErrorType>(type), errorMsg);

    return;
}

void DeviceCBOnResult(uint64_t timestamp, CameraResultCIF* result)
{
    OHOS::sptr<OHOS::Camera::ICameraDeviceCallback> callback;
    {
        std::lock_guard<std::mutex> l(g_deviceCallbackMutex);
        callback = g_deviceCallback;
    }
    if (callback == nullptr) {
        return;
    }

    std::shared_ptr<CameraMetadata> meta = std::make_shared<CameraMetadata>(0, 0);
    callback->OnResult(timestamp, meta);

    return;
}

#ifdef __cplusplus
}
#endif
