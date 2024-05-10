/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "dcamera_host.h"

#include <cstdlib>
#include "iservice_registry.h"
#include "iproxy_broker.h"
#include "iservmgr_hdi.h"

#include "anonymous_string.h"
#include "distributed_hardware_log.h"
#include "metadata_utils.h"
#include "dcamera.h"

namespace OHOS {
namespace DistributedHardware {
OHOS::sptr<DCameraHost> DCameraHost::instance_ = nullptr;
DCameraHost::AutoRelease DCameraHost::autoRelease_;

extern "C" ICameraHost *CameraHostImplGetInstance(void)
{
    return static_cast<ICameraHost *>(DCameraHost::GetInstance().GetRefPtr());
}

OHOS::sptr<DCameraHost> DCameraHost::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new DCameraHost();
        if (instance_ == nullptr) {
            DHLOGE("Get distributed camera host instance failed.");
            return nullptr;
        }
    }
    return instance_;
}

int32_t DCameraHost::SetCallback(const sptr<ICameraHostCallback> &callbackObj)
{
    if (callbackObj == nullptr) {
        DHLOGE("DCameraHost::SetCallback, input camera host callback is null.");
        return CamRetCode::INVALID_ARGUMENT;
    }
    dCameraHostCallback_ = callbackObj;
    dCameraHostRecipient_ = new DCameraHostRecipient();
    return CamRetCode::NO_ERROR;
}

int32_t DCameraHost::GetCameraIds(std::vector<std::string> &cameraIds)
{
    std::lock_guard<std::mutex> autoLock(deviceMapLock_);
    auto iter = dCameraDeviceMap_.begin();
    while (iter != dCameraDeviceMap_.end()) {
        if (!(iter->first).empty()) {
            cameraIds.push_back(iter->first);
        }
        iter++;
    }
    return CamRetCode::NO_ERROR;
}

int32_t DCameraHost::GetCameraAbility(const std::string &cameraId, std::vector<uint8_t> &cameraAbility)
{
    if (IsCameraIdInvalid(cameraId)) {
        DHLOGE("DCameraHost::GetCameraAbility, input cameraId is invalid.");
        return CamRetCode::INVALID_ARGUMENT;
    }

    DHLOGE("DCameraHost::GetCameraAbility for cameraId: %{public}s", GetAnonyString(cameraId).c_str());

    std::shared_ptr<CameraAbility> ability = nullptr;
    int32_t ret;
    {
        std::lock_guard<std::mutex> autoLock(deviceMapLock_);
        auto iter = dCameraDeviceMap_.find(cameraId);
        ret = (iter->second)->GetDCameraAbility(ability);
        if (ret != CamRetCode::NO_ERROR) {
            DHLOGE("DCameraHost::GetCameraAbility, GetDCameraAbility failed, ret: %{public}d.", ret);
            return ret;
        }
    }
    bool retBool = OHOS::Camera::MetadataUtils::ConvertMetadataToVec(ability, cameraAbility);
    if (!retBool) {
        DHLOGE("DCameraHost::GetCameraAbility, ConvertMetadataToVec failed.");
        return CamRetCode::INVALID_ARGUMENT;
    }

    do {
        camera_metadata_item_t item;
        constexpr uint32_t WIDTH_OFFSET = 1;
        constexpr uint32_t HEIGHT_OFFSET = 2;
        constexpr uint32_t UNIT_LENGTH = 3;
        ret = OHOS::Camera::FindCameraMetadataItem(ability->get(),
            OHOS_ABILITY_STREAM_AVAILABLE_BASIC_CONFIGURATIONS, &item);
        DHLOGI("FindCameraMetadataItem item=%{public}u, count=%{public}u, dataType=%{public}u", item.item,
            item.count, item.data_type);
        if (ret != CAM_META_SUCCESS) {
            DHLOGE("Failed to find stream configuration in camera ability with return code %{public}d", ret);
            break;
        }
        if (item.count % UNIT_LENGTH != 0) {
            DHLOGE("Invalid stream configuration count: %{public}u", item.count);
            break;
        }
        for (uint32_t index = 0; index < item.count; index += UNIT_LENGTH) {
            int32_t format = item.data.i32[index];
            int32_t width = item.data.i32[index + WIDTH_OFFSET];
            int32_t height = item.data.i32[index + HEIGHT_OFFSET];
            DHLOGD("format: %{public}d, width: %{public}d, height: %{public}d", format, width, height);
        }
    } while (0);
    return CamRetCode::NO_ERROR;
}

int32_t DCameraHost::OpenCamera(const std::string &cameraId, const sptr<ICameraDeviceCallback> &callbackObj,
    sptr<ICameraDevice> &device)
{
    if (IsCameraIdInvalid(cameraId) || callbackObj == nullptr) {
        DHLOGE("DCameraHost::OpenCamera, open camera id is invalid or camera device callback is null.");
        return CamRetCode::INVALID_ARGUMENT;
    }

    DHLOGI("DCameraHost::OpenCamera for cameraId: %{public}s", GetAnonyString(cameraId).c_str());

    OHOS::sptr<DCameraDevice> dcameraDevice = nullptr;
    {
        std::lock_guard<std::mutex> autoLock(deviceMapLock_);
        auto iter = dCameraDeviceMap_.find(cameraId);
        if (iter == dCameraDeviceMap_.end()) {
            DHLOGE("DCameraHost::OpenCamera, dcamera device not found.");
            return CamRetCode::INSUFFICIENT_RESOURCES;
        }

        dcameraDevice = iter->second;
        if (dcameraDevice == nullptr) {
            DHLOGE("DCameraHost::OpenCamera, dcamera device is null.");
            return INSUFFICIENT_RESOURCES;
        }
    }

    if (dcameraDevice->IsOpened()) {
        DHLOGE("DCameraHost::OpenCamera, dcamera device %{public}s already opened.", GetAnonyString(cameraId).c_str());
        return CamRetCode::CAMERA_BUSY;
    }

    CamRetCode ret = dcameraDevice->OpenDCamera(callbackObj);
    if (ret != CamRetCode::NO_ERROR) {
        DHLOGE("DCameraHost::OpenCamera, open camera failed.");
        return ret;
    }
    device = dcameraDevice;

    DHLOGI("DCameraHost::OpenCamera, open camera %{public}s success.", GetAnonyString(cameraId).c_str());
    return CamRetCode::NO_ERROR;
}

int32_t DCameraHost::SetFlashlight(const std::string &cameraId, bool isEnable)
{
    (void)cameraId;
    (void)isEnable;
    DHLOGI("DCameraHost::SetFlashlight, distributed camera not support.");

    return CamRetCode::METHOD_NOT_SUPPORTED;
}

DCamRetCode DCameraHost::AddDeviceParamCheck(const DHBase &dhBase, const std::string& sinkAbilityInfo,
    const std::string &sourceCodecInfo, const sptr<IDCameraProviderCallback> &callback)
{
    if (IsDhBaseInfoInvalid(dhBase)) {
        DHLOGE("DCameraHost::AddDCameraDevice, devId or dhId is invalid.");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    DHLOGI("DCameraHost::AddDCameraDevice for {devId: %{public}s, dhId: %{public}s}",
        GetAnonyString(dhBase.deviceId_).c_str(), GetAnonyString(dhBase.dhId_).c_str());

    if (sinkAbilityInfo.empty() || sinkAbilityInfo.length() > ABILITYINFO_MAX_LENGTH) {
        DHLOGE("DCameraHost::AddDCameraDevice, input sinkAbilityInfo is invalid.");
        return DCamRetCode::INVALID_ARGUMENT;
    }

    if (sourceCodecInfo.empty() || sourceCodecInfo.length() > ABILITYINFO_MAX_LENGTH) {
        DHLOGE("DCameraHost::AddDCameraDevice, input sourceCodecInfo is invalid.");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    if (GetCamDevNum() > MAX_DCAMERAS_NUMBER) {
        DHLOGE("DCameraHost::AddDCameraDevice, cameras exceed the upper limit.");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    return DCamRetCode::SUCCESS;
}

DCamRetCode DCameraHost::AddDCameraDevice(const DHBase &dhBase, const std::string& sinkAbilityInfo,
    const std::string &sourceCodecInfo, const sptr<IDCameraProviderCallback> &callback)
{
    if (AddDeviceParamCheck(dhBase, sinkAbilityInfo, sourceCodecInfo, callback) != DCamRetCode::SUCCESS) {
        DHLOGE("DCameraHost::AddDCameraDevice, input param is invalid.");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    std::string dCameraId = dhBase.deviceId_ + "__" + dhBase.dhId_;
    {
        std::lock_guard<std::mutex> autoLock(deviceMapLock_);
        if (dCameraDeviceMap_.find(dCameraId) != dCameraDeviceMap_.end()) {
            if (dCameraDeviceMap_[dCameraId] == nullptr) {
                DHLOGI("AddDCameraDevice device is null");
                return DCamRetCode::INVALID_ARGUMENT;
            }
            dCameraDeviceMap_[dCameraId]->SetDcameraAbility(sinkAbilityInfo);
            DHLOGI("AddDCameraDevice refresh data success");
            return DCamRetCode::SUCCESS;
        }
    }
    OHOS::sptr<DCameraDevice> dcameraDevice(new (std::nothrow) DCameraDevice(dhBase, sinkAbilityInfo,
        sourceCodecInfo));
    if (dcameraDevice == nullptr) {
        DHLOGE("DCameraHost::AddDCameraDevice, create dcamera device failed.");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    {
        std::lock_guard<std::mutex> autoLock(deviceMapLock_);
        dCameraDeviceMap_[dCameraId] = dcameraDevice;
    }
    if (callback == nullptr) {
        DHLOGE("DCameraHost::SetProviderCallback failed, callback is null");
        return DCamRetCode::INVALID_ARGUMENT;
    }
    dcameraDevice->SetProviderCallback(callback);
    if (dCameraHostCallback_ != nullptr) {
        dCameraHostCallback_->OnCameraEvent(dCameraId, CameraEvent::CAMERA_EVENT_DEVICE_ADD);
    }
    sptr<IRemoteObject> remote = OHOS::HDI::hdi_objcast<IDCameraProviderCallback>(callback);
    if (remote != nullptr) {
        remote->AddDeathRecipient(dCameraHostRecipient_);
    }
    DHLOGI("AddDCameraDevice create dcamera device success, dCameraId: %{public}s", GetAnonyString(dCameraId).c_str());
    return DCamRetCode::SUCCESS;
}

size_t DCameraHost::GetCamDevNum()
{
    std::lock_guard<std::mutex> autoLock(deviceMapLock_);
    return dCameraDeviceMap_.size();
}

DCamRetCode DCameraHost::RemoveDCameraDevice(const DHBase &dhBase)
{
    DHLOGI("DCameraHost::RemoveDCameraDevice for {devId: %{public}s, dhId: %{public}s}",
        GetAnonyString(dhBase.deviceId_).c_str(), GetAnonyString(dhBase.dhId_).c_str());

    std::string dCameraId = GetCameraIdByDHBase(dhBase);
    if (dCameraId.empty()) {
        DHLOGE("DCameraHost::RemoveDCameraDevice, dhBase not exist.");
        return DCamRetCode::INVALID_ARGUMENT;
    }

    OHOS::sptr<DCameraDevice> dcameraDevice = GetDCameraDeviceByDHBase(dhBase);
    if (dcameraDevice != nullptr) {
        if (dcameraDevice->IsOpened()) {
            dcameraDevice->Close();
        }
        dcameraDevice->SetProviderCallback(nullptr);
        sptr<IDCameraProviderCallback> callback = dcameraDevice->GetProviderCallback();
        if (callback != nullptr) {
            sptr<IRemoteObject> remoteObj = OHOS::HDI::hdi_objcast<IDCameraProviderCallback>(callback);
            if (remoteObj != nullptr) {
                remoteObj->RemoveDeathRecipient(dCameraHostRecipient_);
            }
        }
    }
    {
        std::lock_guard<std::mutex> autoLock(deviceMapLock_);
        dCameraDeviceMap_.erase(dCameraId);
    }

    if (dCameraHostCallback_ != nullptr) {
        dCameraHostCallback_->OnCameraEvent(dCameraId, CameraEvent::CAMERA_EVENT_DEVICE_RMV);
    }

    DHLOGI("DCameraHost::RemoveDCameraDevice, remove dcamera device success, dCameraId: %{public}s",
        GetAnonyString(dCameraId).c_str());
    return DCamRetCode::SUCCESS;
}

bool DCameraHost::IsCameraIdInvalid(const std::string &cameraId)
{
    if (cameraId.empty() || cameraId.length() > ID_MAX_SIZE) {
        return true;
    }

    std::lock_guard<std::mutex> autoLock(deviceMapLock_);
    auto iter = dCameraDeviceMap_.begin();
    while (iter != dCameraDeviceMap_.end()) {
        if (cameraId == iter->first) {
            return false;
        }
        iter++;
    }
    return true;
}

std::string DCameraHost::GetCameraIdByDHBase(const DHBase &dhBase)
{
    std::string dcameraId = dhBase.deviceId_ + "__" + dhBase.dhId_;
    return dcameraId;
}

OHOS::sptr<DCameraDevice> DCameraHost::GetDCameraDeviceByDHBase(const DHBase &dhBase)
{
    std::string dCameraId = GetCameraIdByDHBase(dhBase);
    if (dCameraId.empty()) {
        DHLOGE("DCameraHost::GetDCameraDeviceByDHBase, dhBase not exist.");
        return nullptr;
    }

    std::lock_guard<std::mutex> autoLock(deviceMapLock_);
    auto iter = dCameraDeviceMap_.find(dCameraId);
    if (iter == dCameraDeviceMap_.end()) {
        DHLOGE("DCameraHost::GetDCameraDeviceByDHBase, dcamera device not found.");
        return nullptr;
    }
    return iter->second;
}

void DCameraHost::NotifyDCameraStatus(const DHBase &dhBase, int32_t result)
{
    std::string dCameraId = GetCameraIdByDHBase(dhBase);
    if (dCameraId.empty()) {
        DHLOGE("DCameraHost::NotifyDCameraStatus, dhBase not exist.");
        return;
    }
    if (dCameraHostCallback_ != nullptr) {
        dCameraHostCallback_->OnCameraStatus(dCameraId, CameraStatus::UN_AVAILABLE);
    }
}

void DCameraHost::DCameraHostRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    DHLOGE("Exit the current process.");
    _Exit(0);
}
} // namespace DistributedHardware
} // namespace OHOS
