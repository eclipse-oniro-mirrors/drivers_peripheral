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

#include "sensor_impl.h"
#include <hdf_base.h>
#include <hdf_log.h>

#define HDF_LOG_TAG    hdf_sensor_dal

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V1_0 {
namespace {
    enum SensorIndex {
        TRADITIONAL_SENSOR_INDEX = 0,
        MEDICAL_SENSOR_INDEX = 1
    };
    constexpr int32_t REMOTE_OBJE_CTOUNT_THRESHOLD = 1;
    constexpr int32_t TRADITIONAL_SENSOR_ID_MIN = 0;
    using RemoteObjectCallBackMap = std::unordered_map<IRemoteObject*, sptr<ISensorCallback>>;
    using RemoteObjectIndexMap = std::unordered_map<IRemoteObject*, int32_t>;
    using IndexRemoteObjectMap = std::unordered_map<int32_t, std::vector<IRemoteObject*>>;
    RemoteObjectCallBackMap g_remoteObjectCallBackMap;
    IndexRemoteObjectMap g_IndexRemoteObjectMap;
    RemoteObjectIndexMap g_remoteObjectIndexMap;
    std::mutex g_mutex;
}

int TradtionalSensorDataCallback(const struct SensorEvents *event)
{
    if (event == nullptr || event->data == nullptr) {
        HDF_LOGE("%{public}s failed, event or event.data is nullptr", __func__);
        return SENSOR_FAILURE;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    auto indexRemoteIter = g_IndexRemoteObjectMap.find(TRADITIONAL_SENSOR_INDEX);
    if (indexRemoteIter == g_IndexRemoteObjectMap.end()) {
        return SENSOR_SUCCESS;
    }

    HdfSensorEvents hdfSensorEvents;
    hdfSensorEvents.sensorId = event->sensorId;
    hdfSensorEvents.version = event->version;
    hdfSensorEvents.timestamp = event->timestamp;
    hdfSensorEvents.option = event->option;
    hdfSensorEvents.mode = event->mode;
    hdfSensorEvents.dataLen = event->dataLen;
    uint32_t len = event->dataLen;
    uint8_t *tmp = event->data;

    while (len--) {
        hdfSensorEvents.data.push_back(*tmp);
        tmp++;
    }

    for (auto remoteObj : g_IndexRemoteObjectMap[TRADITIONAL_SENSOR_INDEX]) {
        auto remoteCallBack = g_remoteObjectCallBackMap.find(remoteObj);
        if (remoteCallBack == g_remoteObjectCallBackMap.end()) {
            continue;
        }
        remoteCallBack->second->OnDataEvent(hdfSensorEvents);
    }

    return SENSOR_SUCCESS;
}

int MedicalSensorDataCallback(const struct SensorEvents *event)
{
    if (event == nullptr || event->data == nullptr) {
        HDF_LOGE("%{public}s failed, event or event.data is nullptr", __func__);
        return SENSOR_FAILURE;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    auto indexRemoteIter = g_IndexRemoteObjectMap.find(MEDICAL_SENSOR_INDEX);
    if (indexRemoteIter == g_IndexRemoteObjectMap.end()) {
        return SENSOR_SUCCESS;
    }

    HdfSensorEvents hdfSensorEvents;
    hdfSensorEvents.sensorId = event->sensorId;
    hdfSensorEvents.version = event->version;
    hdfSensorEvents.timestamp = event->timestamp;
    hdfSensorEvents.option = event->option;
    hdfSensorEvents.mode = event->mode;
    hdfSensorEvents.dataLen = event->dataLen;
    uint32_t len = event->dataLen;
    uint8_t *tmp = event->data;

    while (len--) {
        hdfSensorEvents.data.push_back(*tmp);
        tmp++;
    }

    for (auto remoteObj : g_IndexRemoteObjectMap[MEDICAL_SENSOR_INDEX]) {
        auto remoteCallBack = g_remoteObjectCallBackMap.find(remoteObj);
        if (remoteCallBack == g_remoteObjectCallBackMap.end()) {
            continue;
        }
        remoteCallBack->second->OnDataEvent(hdfSensorEvents);
    }

    return SENSOR_SUCCESS;
}

void SensorImpl::Init()
{
    sensorInterface = NewSensorInterfaceInstance();
    if (sensorInterface == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
    }
}

int32_t SensorImpl::GetAllSensorInfo(std::vector<HdfSensorInformation>& info)
{
    if (sensorInterface == NULL || sensorInterface->GetAllSensors == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    struct SensorInformation *sensorInfo = nullptr;
    struct SensorInformation *tmp = nullptr;
    int32_t count = 0;

    int32_t ret = sensorInterface->GetAllSensors(&sensorInfo, &count);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
        return ret;
    }

    if (count <= 0) {
        HDF_LOGE("%{public}s failed, count<=0", __func__);
        return HDF_FAILURE;
    }

    tmp = sensorInfo;
    while (count--) {
        HdfSensorInformation hdfSensorInfo;
        std::string sensorName(tmp->sensorName);
        hdfSensorInfo.sensorName = sensorName;
        std::string vendorName(tmp->vendorName);
        hdfSensorInfo.vendorName = vendorName;
        std::string firmwareVersion(tmp->firmwareVersion);
        hdfSensorInfo.firmwareVersion = firmwareVersion;
        std::string hardwareVersion(tmp->hardwareVersion);
        hdfSensorInfo.hardwareVersion = hardwareVersion;
        hdfSensorInfo.sensorTypeId = tmp->sensorTypeId;
        hdfSensorInfo.sensorId = tmp->sensorId;
        hdfSensorInfo.maxRange = tmp->maxRange;
        hdfSensorInfo.accuracy = tmp->accuracy;
        hdfSensorInfo.power = tmp->power;
        info.push_back(std::move(hdfSensorInfo));
        tmp++;
    }

    return HDF_SUCCESS;
}

int32_t SensorImpl::Enable(int32_t sensorId)
{
    if (sensorInterface == NULL || sensorInterface->Enable == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = sensorInterface->Enable(sensorId);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::Disable(int32_t sensorId)
{
    if (sensorInterface == NULL || sensorInterface->Disable == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = sensorInterface->Disable(sensorId);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetBatch(int32_t sensorId, int64_t samplingInterval, int64_t reportInterval)
{
    if (sensorInterface == NULL || sensorInterface->SetBatch == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = sensorInterface->SetBatch(sensorId, samplingInterval, reportInterval);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetMode(int32_t sensorId, int32_t mode)
{
    if (sensorInterface == NULL || sensorInterface->SetMode == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = sensorInterface->SetMode(sensorId, mode);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetOption(int32_t sensorId, uint32_t option)
{
    if (sensorInterface == NULL || sensorInterface->SetOption == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    int32_t ret = sensorInterface->SetOption(sensorId, option);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::Register(int32_t sensorId, const sptr<ISensorCallback>& callbackObj)
{
    if (sensorInterface == NULL || sensorInterface->Register == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    IRemoteObject* obj = callbackObj->AsObject().GetRefPtr();
    std::lock_guard<std::mutex> lock(g_mutex);
    auto remoteIter = g_remoteObjectCallBackMap.find(obj);
    if (remoteIter != g_remoteObjectCallBackMap.end()) {
        return SENSOR_FAILURE;
    }

    SensorIndex sensorIndex;
    if (sensorId < TRADITIONAL_SENSOR_ID_MIN) {
        return SENSOR_FAILURE;
    }

    if (sensorId >= SENSOR_TYPE_MEDICAL_BEGIN && sensorId < SENSOR_TYPE_MEDICAL_END) {
        sensorIndex = MEDICAL_SENSOR_INDEX;
    } else {
        sensorIndex = TRADITIONAL_SENSOR_INDEX;
    }

    auto indexRemoteIter = g_IndexRemoteObjectMap.find(sensorIndex);
    if (indexRemoteIter != g_IndexRemoteObjectMap.end()) {
        auto remoteObjectIter =
            find(g_IndexRemoteObjectMap[sensorIndex].begin(), g_IndexRemoteObjectMap[sensorIndex].end(), obj);
        if (remoteObjectIter == g_IndexRemoteObjectMap[sensorIndex].end()) {
            g_IndexRemoteObjectMap[sensorIndex].push_back(obj);
            g_remoteObjectCallBackMap[obj] = callbackObj;
            g_remoteObjectIndexMap[obj] = sensorIndex;
        }
        return SENSOR_SUCCESS;
    }

    int32_t ret;
    if (sensorIndex == TRADITIONAL_SENSOR_INDEX) {
        ret = sensorInterface->Register(sensorIndex, TradtionalSensorDataCallback);
    } else {
        ret = sensorInterface->Register(sensorIndex, MedicalSensorDataCallback);
    }

    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, ret[%{public}d]", __func__, ret);
        return ret;
    }

    std::vector<IRemoteObject*> remoteVec;
    remoteVec.push_back(obj);
    g_remoteObjectCallBackMap[obj] = callbackObj;
    g_remoteObjectIndexMap[obj] = sensorIndex;
    g_IndexRemoteObjectMap[sensorIndex] = remoteVec;

    return ret;
}

int32_t SensorImpl::Unregister(int32_t sensorId, const sptr<ISensorCallback>& callbackObj)
{
    if (sensorInterface == NULL || sensorInterface->Unregister == NULL) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }
    IRemoteObject* obj = callbackObj->AsObject().GetRefPtr();

    std::lock_guard<std::mutex> lock(g_mutex);
    auto remoteIndexIter = g_remoteObjectIndexMap.find(obj);
    if (remoteIndexIter == g_remoteObjectIndexMap.end()) {
        return HDF_FAILURE;
    }
    SensorIndex sensorIndex;
    if (remoteIndexIter->second == TRADITIONAL_SENSOR_INDEX) {
        sensorIndex = TRADITIONAL_SENSOR_INDEX;
    } else {
        sensorIndex = MEDICAL_SENSOR_INDEX;
    }

    auto indexRemoteIter = g_IndexRemoteObjectMap.find(sensorIndex);
    if (indexRemoteIter == g_IndexRemoteObjectMap.end()) {
        return HDF_FAILURE;
    }
    auto remoteObjectIter =
        find(g_IndexRemoteObjectMap[sensorIndex].begin(), g_IndexRemoteObjectMap[sensorIndex].end(), obj);
    if (remoteObjectIter == g_IndexRemoteObjectMap[sensorIndex].end()) {
        return HDF_FAILURE;
    }

    if (g_IndexRemoteObjectMap[sensorIndex].size() > REMOTE_OBJE_CTOUNT_THRESHOLD) {
        g_IndexRemoteObjectMap[sensorIndex].erase(remoteObjectIter);
        g_remoteObjectIndexMap.erase(remoteIndexIter);
        g_remoteObjectCallBackMap.erase(obj);
        return SENSOR_SUCCESS;
    }

    int32_t ret;
    if (sensorIndex == TRADITIONAL_SENSOR_INDEX) {
        ret = sensorInterface->Unregister(sensorIndex, TradtionalSensorDataCallback);
    } else {
        ret = sensorInterface->Unregister(sensorIndex, MedicalSensorDataCallback);
    }

    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
        return ret;
    }
    g_remoteObjectIndexMap.erase(remoteIndexIter);
    g_remoteObjectCallBackMap.erase(obj);
    g_IndexRemoteObjectMap.erase(sensorIndex);

    return ret;
}
} // V1_0
} // Sensor
} // HDI
} // OHOS