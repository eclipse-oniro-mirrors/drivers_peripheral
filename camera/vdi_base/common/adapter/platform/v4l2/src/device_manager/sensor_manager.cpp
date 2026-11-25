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

#include "sensor_manager.h"
#include "create_sensor_factory.h"

namespace OHOS::Camera {
SensorManager::SensorManager() {}

SensorManager::SensorManager(ManagerId managerId) : IManager(managerId) {}

SensorManager::~SensorManager() {}

RetCode SensorManager::CreateController(ControllerId controllerId, std::string hardwareName)
{
    RetCode rc = RC_OK;
    if (controllerId == DM_C_SENSOR) {
        {
            std::lock_guard<std::mutex> l(sensorMtx_);
            if (CheckCameraIdList(hardwareName)) {
                return RC_OK;
            }
        }
        std::shared_ptr<SensorController> sensor = std::make_shared<SensorController>(hardwareName);
        if (sensor == nullptr) {
            return RC_ERROR;
        }
        rc = sensor->Init();
        if (rc == RC_OK) {
            std::lock_guard<std::mutex> l(sensorMtx_);
            if (!CheckCameraIdList(hardwareName)) {
                sensorList_.push_back(sensor);
            }
        }
    }
    return rc;
}

std::shared_ptr<IController> SensorManager::GetController(ControllerId controllerId, std::string hardwareName)
{
    (void)controllerId;
    std::lock_guard<std::mutex> l(sensorMtx_);
    if (sensorList_.size() == 0) {
        return nullptr;
    }
    for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
        if ((*iter)->GetName() == hardwareName) {
            return *iter;
        }
    }
    return nullptr;
}

void SensorManager::Configure(std::shared_ptr<CameraMetadata> meta)
{
    std::vector<std::shared_ptr<SensorController>> snapshot;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        if (sensorList_.size() == 0) {
            return;
        }
        snapshot = sensorList_;
    }
    for (auto iter = snapshot.cbegin(); iter != snapshot.cend(); iter++) {
        (*iter)->Configure(meta);
    }
}

RetCode SensorManager::PowerUp(std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target == nullptr) {
        return RC_ERROR;
    }
    return target->PowerUp();
}

RetCode SensorManager::PowerDown(std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target == nullptr) {
        return RC_ERROR;
    }
    return target->PowerDown();
}

RetCode SensorManager::Start(std::string hardwareName, int buffCont, DeviceFormat& format)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target == nullptr) {
        return RC_ERROR;
    }
    return target->Start(buffCont, format);
}

RetCode SensorManager::Stop(std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target == nullptr) {
        return RC_ERROR;
    }
    return target->Stop();
}

RetCode SensorManager::SendFrameBuffer(std::shared_ptr<FrameSpec> buffer, std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target == nullptr) {
        return RC_ERROR;
    }
    return target->SendFrameBuffer(buffer);
}

void SensorManager::SetAbilityMetaDataTag(std::vector<int32_t> abilityMetaDataTag, std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target != nullptr) {
        target->SetAbilityMetaDataTag(abilityMetaDataTag);
    }
}

std::shared_ptr<ISensor> SensorManager::GetSensor(std::string sensorName)
{
    CAMERA_LOGI("%s GetSensor %s", __FUNCTION__, sensorName.c_str());
    return GetSensorFactory.GetSensorByName(sensorName);
}

bool SensorManager::CheckCameraIdList(std::string hardwareName)
{
    if (sensorList_.size() == 0) {
        return false;
    }
    for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
        if ((*iter)->GetName() == hardwareName) {
            return true;
        }
    }
    return false;
}

void SensorManager::SetNodeCallBack(const NodeBufferCb cb, std::string hardwareName)
{
    std::shared_ptr<SensorController> target;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                target = *iter;
                break;
            }
        }
    }
    if (target != nullptr) {
        target->SetNodeCallBack(cb);
    }
}

void SensorManager::SetMetaDataCallBack(const MetaDataCb cb, std::string hardwareName)
{
    (void)cb;
    (void)hardwareName;
}

RetCode SensorManager::DestroyController(ControllerId controllerId, std::string hardwareName)
{
    if (controllerId == DM_C_SENSOR) {
        std::lock_guard<std::mutex> l(sensorMtx_);
        for (auto iter = sensorList_.cbegin(); iter != sensorList_.cend(); iter++) {
            if ((*iter)->GetName() == hardwareName) {
                sensorList_.erase(iter);
                return RC_OK;
            }
        }
    }
    return RC_ERROR;
}

void SensorManager::SetMemoryType(uint8_t &memType)
{
    std::vector<std::shared_ptr<SensorController>> snapshot;
    {
        std::lock_guard<std::mutex> l(sensorMtx_);
        if (sensorList_.size() == 0) {
            return;
        }
        snapshot = sensorList_;
    }
    for (auto iter = snapshot.cbegin(); iter != snapshot.cend(); iter++) {
        (*iter)->SetMemoryType(memType);
    }
    return;
}
} // namespace OHOS::Camera
