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

#include "hdf_log.h"
#include "sensor_clients_manager.h"

#define HDF_LOG_TAG uhdf_sensor_service

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V2_0 {

SensorClientsManager* SensorClientsManager::instance = nullptr;
std::mutex SensorClientsManager::instanceMutex_;

SensorClientsManager::SensorClientsManager()
{
}

SensorClientsManager::~SensorClientsManager()
{
    clients_.clear();
    sensorUsed_.clear();
    sensorConfig_.clear();
}

void SensorClientsManager::ReportDataCbRegister(int groupId, int serviceId, const sptr<ISensorCallback> &callbackObj)
{
    std::unique_lock<std::mutex> lock(clientsMutex_);
    if (clients_.find(groupId) == clients_.end() || clients_[groupId].find(serviceId) == clients_[groupId].end()) {
        if (callbackObj == nullptr) {
            HDF_LOGE("%{public}s: the callback of service %{public}d is null", __func__, serviceId);
            return;
        }
        clients_[groupId].emplace(serviceId, callbackObj);
        HDF_LOGI("%{public}s: service %{public}d insert the callback", __func__, serviceId);
        return;
    }

    auto it = clients_[groupId].find(serviceId);
    it -> second.SetReportDataCb(callbackObj);
    HDF_LOGI("%{public}s: service %{public}d update the callback", __func__, serviceId);

    return;
}

void SensorClientsManager::ReportDataCbUnRegister(int groupId, int serviceId, const sptr<ISensorCallback> &callbackObj)
{
    std::unique_lock<std::mutex> lock(clientsMutex_);
    if (clients_.find(groupId) == clients_.end() || clients_[groupId].find(serviceId) == clients_[groupId].end()) {
        HDF_LOGI("%{public}s: service %{public}d already UnRegister", __func__, serviceId);
        return;
    }

    auto it = clients_[groupId].find(serviceId);
    clients_[groupId].erase(it);
    HDF_LOGI("%{public}s: service: %{public}d, UnRegisterCB Success", __func__, serviceId);
    return;
}

void SensorClientsManager::UpdateSensorConfig(int sensorId, int64_t samplingInterval, int64_t reportInterval)
{
    std::unique_lock<std::mutex> lock(sensorConfigMutex_);
    auto it = sensorConfig_.find(sensorId);
    if (it != sensorConfig_.end()) {
        it->second.samplingInterval = samplingInterval <= it->second.samplingInterval ? samplingInterval
         : it->second.samplingInterval;
        it->second.reportInterval = reportInterval <= it->second.reportInterval ? reportInterval
         : it->second.reportInterval;
    } else {
        BestSensorConfig config = {samplingInterval, reportInterval};
        sensorConfig_.emplace(sensorId, config);
    }
    return;
}

void SensorClientsManager::SetSensorBestConfig(int sensorId, int64_t &samplingInterval, int64_t &reportInterval)
{
    std::unique_lock<std::mutex> lock(sensorConfigMutex_);
    auto it = sensorConfig_.find(sensorId);
    if (it == sensorConfig_.end()) {
        HDF_LOGI("%{public}s: sensor: %{public}d is enabled first time", __func__, sensorId);
        return;
    }
    
    samplingInterval = samplingInterval < it->second.samplingInterval ? samplingInterval : it->second.samplingInterval;
    reportInterval = reportInterval < it->second.reportInterval ? reportInterval : it->second.reportInterval;
    return;
}

void SensorClientsManager::OpenSensor(int sensorId, int serviceId)
{
    std::unique_lock<std::mutex> lock(sensorUsedMutex_);
    std::set<int> service = {serviceId};
    sensorUsed_.emplace(sensorId, service);
    HDF_LOGI("%{public}s: service: %{public}d enabled sensor %{public}d", __func__,  serviceId, sensorId);
}

bool SensorClientsManager::IsNeedOpenSensor(int sensorId, int serviceId)
{
    auto it = sensorUsed_.find(sensorId);
    if (it == sensorUsed_.end()) {
        HDF_LOGI("%{public}s: sensor %{public}d is enabled by service: %{public}d", __func__,  sensorId, serviceId);
        return true;
    }
    auto service = sensorUsed_[sensorId].find(serviceId);
    if (service == sensorUsed_[sensorId].end()) {
        sensorUsed_[sensorId].insert(serviceId);
        HDF_LOGI("%{public}s: service: %{public}d enabled sensor %{public}d", __func__,  serviceId, sensorId);
    }
    return false;
}

bool SensorClientsManager::IsNeedCloseSensor(int sensorId, int serviceId)
{
    auto it = sensorUsed_.find(sensorId);
    if (it == sensorUsed_.end()) {
        HDF_LOGE("%{public}s: sensor %{public}d has been disabled  or not support", __func__, sensorId);
        return true;
    }
    sensorUsed_[sensorId].erase(serviceId);
    if (sensorUsed_[sensorId].empty()) {
        sensorUsed_.erase(sensorId);
        sensorConfig_.erase(sensorId);
        HDF_LOGI("%{public}s: disabled sensor %{public}d", __func__, sensorId);
        return true;
    }
    return false;
}

bool SensorClientsManager::IsUpadateSensorState(int sensorId, int serviceId, bool isOpen)
{
    std::unique_lock<std::mutex> lock(sensorUsedMutex_);
    if (isOpen && IsNeedOpenSensor(sensorId, serviceId)) {
        return true;
    }
    if (!isOpen && IsNeedCloseSensor(sensorId, serviceId)) {
        return true;
    }
    return false;
}

bool SensorClientsManager::IsClientsEmpty(int groupId)
{
    std::unique_lock<std::mutex> lock(clientsMutex_);
    if (clients_.find(groupId) == clients_.end() || clients_[groupId].empty()) {
        return true;
    }
    return false;
}

bool SensorClientsManager::GetClients(int groupId, std::unordered_map<int32_t, SensorClientInfo> &client)
{
    std::unique_lock<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(groupId);
    if (it == clients_.end() || it->second.empty()) {
        return false;
    }
    client = it->second;
    return true;
}

void SensorClientsManager::SetClientSenSorConfig(int32_t sensorId, int32_t serviceId, int64_t samplingInterval,
                                                 int64_t &reportInterval)
{
    HDF_LOGI("%{public}s: service %{public}d enter the SetClientSenSorConfig function, sensorId is %{public}d, "
             "samplingInterval is %{public}s, reportInterval is %{public}s", __func__, serviceId, sensorId,
             std::to_string(samplingInterval).c_str(), std::to_string(reportInterval).c_str());

    int32_t groupId = HDF_TRADITIONAL_SENSOR_TYPE;
    if (clients_.find(groupId) == clients_.end() || clients_[groupId].find(serviceId) == clients_[groupId].end()) {
        HDF_LOGI("%{public}s: service %{public}d already UnRegister", __func__, serviceId);
        return;
    }

    auto client = clients_[groupId].find(serviceId);
    SensorConfig sensorConfig = {samplingInterval, reportInterval};
    client -> second.serviceConfigMap_[serviceId][sensorId] = sensorConfig;
    client -> second.curCountMap_[serviceId][sensorId] = 0;
}

bool SensorClientsManager::IsNotNeedReportData(SensorClientInfo &sensorClientInfo, uint32_t serviceId, int32_t sensorId)
{
    HDF_LOGI("%{public}s: enter the IsNotNeedReportData function, sensorClientInfo is %{public}p, "
             "serviceId is %{public}s", __func__, &sensorClientInfo, std::to_string(serviceId).c_str());
    if (sensorConfig_.find(sensorId) == sensorConfig_.end()) {
        return true;
    }
    if (sensorClientInfo.serviceConfigMap_.find(serviceId) == sensorClientInfo.serviceConfigMap_.end() ||
            sensorClientInfo.serviceConfigMap_.find(serviceId).find(sensorId) ==
            sensorClientInfo.serviceConfigMap_.find(serviceId).end()) {
        return true;
    }
    SensorConfig sensorConfig = sensorClientInfo.serviceConfigMap_.find(serviceId).find(sensorId)->second;
    BestSensorConfig bestSensorConfig = sensorConfig_.find(sensorId)->second;
    int32_t periodCount = sensorConfig.reportInterval / bestSensorConfig.reportInterval;

    sensorClientInfo.curCountMap_[serviceId][sensorId]++;
    if (sensorClientInfo.curCountMap_[serviceId][sensorId] >= periodCount) {
        sensorClientInfo.curCountMap_[serviceId][sensorId] = 0;
        HDF_LOGI("%{public}s curCount has been set 0", __func__);
        return false;
    }
    return true;
}

std::unordered_map<int32_t, std::set<int32_t>> SensorClientsManager::GetSensorUsed()
{
    std::unique_lock<std::mutex> lock(sensorUsedMutex_);
    return sensorUsed_;
}

SensorClientsManager* SensorClientsManager::GetInstance()
{
    if (instance == nullptr) {
        std::unique_lock<std::mutex> lock(instanceMutex_);
        if (instance == nullptr) {
            instance = new SensorClientsManager();
        }
    }
    return instance;
}

} // V2_0
} // Sensor
} // HDI
} // OHOS