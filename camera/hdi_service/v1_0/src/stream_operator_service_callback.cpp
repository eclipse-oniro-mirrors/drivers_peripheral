/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "stream_operator_service_callback.h"
#include "camera_service_type_converter.h"
#include "iproxy_broker.h"

namespace OHOS::Camera {

void StreamCallbackDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    CAMERA_LOGI("StreamCallbackDeathRecipient::OnRemoteDied received");
    if (callback_ != nullptr) {
        callback_->OnCallbackDied();
    }
}

StreamOperatorServiceCallback::StreamOperatorServiceCallback(OHOS::sptr<IStreamOperatorCallback> streamOperatorCallback)
    : streamOperatorCallback_(streamOperatorCallback)
{
    if (streamOperatorCallback_ != nullptr) {
        deathRecipient_ = new StreamCallbackDeathRecipient(this);
        const sptr<IRemoteObject> remote = OHOS::HDI::hdi_objcast<IStreamOperatorCallback>(streamOperatorCallback_);
        if (remote != nullptr) {
            remote->AddDeathRecipient(deathRecipient_);
        }
    }
}

StreamOperatorServiceCallback::~StreamOperatorServiceCallback()
{
    if (deathRecipient_ != nullptr && streamOperatorCallback_ != nullptr) {
        const sptr<IRemoteObject> remote = OHOS::HDI::hdi_objcast<IStreamOperatorCallback>(streamOperatorCallback_);
        if (remote != nullptr) {
            remote->RemoveDeathRecipient(deathRecipient_);
        }
    }
}

void StreamOperatorServiceCallback::OnCallbackDied()
{
    CAMERA_LOGI("StreamOperatorServiceCallback::OnCallbackDied, nulling callback");
    OHOS::sptr<IStreamOperatorCallback> callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = streamOperatorCallback_;
        streamOperatorCallback_ = nullptr;
    }
}

OHOS::sptr<IStreamOperatorCallback> StreamOperatorServiceCallback::GetCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    return streamOperatorCallback_;
}

int32_t StreamOperatorServiceCallback::OnCaptureStarted(int32_t captureId, const std::vector<int32_t> &streamIds)
{
    OHOS::sptr<IStreamOperatorCallback> callback = GetCallback();
    CHECK_IF_PTR_NULL_RETURN_VALUE(callback, OHOS::HDI::Camera::V1_0::INVALID_ARGUMENT);
    return callback->OnCaptureStarted(captureId, streamIds);
}

int32_t StreamOperatorServiceCallback::OnCaptureEnded(int32_t captureId, const std::vector<VdiCaptureEndedInfo> &infos)
{
    OHOS::sptr<IStreamOperatorCallback> callback = GetCallback();
    CHECK_IF_PTR_NULL_RETURN_VALUE(callback, OHOS::HDI::Camera::V1_0::INVALID_ARGUMENT);
    std::vector<CaptureEndedInfo> hdiInfos;
    for (auto info : infos) {
        CaptureEndedInfo hdiInfo;
        ConvertCaptureEndedInfoVdiToHdi(info, hdiInfo);
        hdiInfos.push_back(hdiInfo);
    }
    return callback->OnCaptureEnded(captureId, hdiInfos);
}

int32_t StreamOperatorServiceCallback::OnCaptureError(int32_t captureId, const std::vector<VdiCaptureErrorInfo> &infos)
{
    OHOS::sptr<IStreamOperatorCallback> callback = GetCallback();
    CHECK_IF_PTR_NULL_RETURN_VALUE(callback, OHOS::HDI::Camera::V1_0::INVALID_ARGUMENT);
    std::vector<CaptureErrorInfo> hdiInfos;
    for (auto info : infos) {
        CaptureErrorInfo hdiInfo;
        ConvertCaptureErrorInfoVdiToHdi(info, hdiInfo);
        hdiInfos.push_back(hdiInfo);
    }
    return callback->OnCaptureError(captureId, hdiInfos);
}

int32_t StreamOperatorServiceCallback::OnFrameShutter(int32_t captureId,
    const std::vector<int32_t> &streamIds, uint64_t timestamp)
{
    OHOS::sptr<IStreamOperatorCallback> callback = GetCallback();
    CHECK_IF_PTR_NULL_RETURN_VALUE(callback, OHOS::HDI::Camera::V1_0::INVALID_ARGUMENT);
    return callback->OnFrameShutter(captureId, streamIds, timestamp);
}

} // end namespace OHOS::Camera
