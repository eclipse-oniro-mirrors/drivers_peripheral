/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <cinttypes>
#include <unordered_map>
#include <mutex>
#include <iproxy_broker.h>
#include <hdf_log.h>
#include "callback_death_recipient.h"
#include "hitrace_meter.h"
#include "sensor_dump.h"

#define HDF_LOG_TAG uhdf_sensor_service

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V1_1 {
namespace {
    constexpr int32_t CALLBACK_CTOUNT_THRESHOLD = 1;
    using GroupIdCallBackMap = std::unordered_map<int32_t, std::vector<sptr<ISensorCallbackVdi>>>;
    using CallBackDeathRecipientMap = std::unordered_map<IRemoteObject *, sptr<CallBackDeathRecipient>>;
    GroupIdCallBackMap g_groupIdCallBackMap;
    CallBackDeathRecipientMap g_callBackDeathRecipientMap;
    std::mutex g_mutex;
} // namespace

int32_t ReportSensorEventsData(int32_t sensorType, const struct SensorEvents *event)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto groupCallBackIter = g_groupIdCallBackMap.find(sensorType);
    if (groupCallBackIter == g_groupIdCallBackMap.end()) {
        return SENSOR_SUCCESS;
    }

    HdfSensorEventsVdi hdfSensorEvents;
    hdfSensorEvents.sensorId = event->sensorId;
    hdfSensorEvents.version = event->version;
    hdfSensorEvents.timestamp = event->timestamp;
    hdfSensorEvents.option = event->option;
    hdfSensorEvents.mode = event->mode;
    hdfSensorEvents.dataLen = event->dataLen;
    uint32_t len = event->dataLen;
    uint8_t *tmp = event->data;

    while ((len--) != 0) {
        hdfSensorEvents.data.push_back(*tmp);
        tmp++;
    }

    for (auto callBack : g_groupIdCallBackMap[sensorType]) {
        callBack->OnDataEventVdi(hdfSensorEvents);
    }

    return SENSOR_SUCCESS;
}

int32_t TradtionalSensorDataCallback(const struct SensorEvents *event)
{
    if (event == nullptr || event->data == nullptr) {
        HDF_LOGE("%{public}s failed, event or event.data is nullptr", __func__);
        return SENSOR_FAILURE;
    }

    (void)ReportSensorEventsData(TRADITIONAL_SENSOR_TYPE, event);

    return SENSOR_SUCCESS;
}

int32_t MedicalSensorDataCallback(const struct SensorEvents *event)
{
    if (event == nullptr || event->data == nullptr) {
        HDF_LOGE("%{public}s failed, event or event.data is nullptr", __func__);
        return SENSOR_FAILURE;
    }

    (void)ReportSensorEventsData(MEDICAL_SENSOR_TYPE, event);

    return SENSOR_SUCCESS;
}

void  SensorImpl::RemoveDeathNotice(int32_t sensorType)
{
    auto iter = g_groupIdCallBackMap.find(sensorType);
    if (iter != g_groupIdCallBackMap.end()) {
        for (auto callback : g_groupIdCallBackMap[sensorType]) {
            const sptr<IRemoteObject> &remote = callback->HandleCallbackDeath();
            auto recipientIter = g_callBackDeathRecipientMap.find(remote.GetRefPtr());
            if (recipientIter != g_callBackDeathRecipientMap.end()) {
                bool removeResult = remote->RemoveDeathRecipient(recipientIter->second);
                if (!removeResult) {
                    HDF_LOGE("%{public}s: sensor destroyed, callback RemoveSensorDeathRecipient fail", __func__);
                }
            }
        }
    }
}

SensorImpl::~SensorImpl()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    RemoveDeathNotice(TRADITIONAL_SENSOR_TYPE);
    RemoveDeathNotice(MEDICAL_SENSOR_TYPE);

    FreeSensorInterfaceInstance();
}

int32_t SensorImpl::Init()
{
    sensorInterface = NewSensorInterfaceInstance();
    if (sensorInterface == nullptr) {
        HDF_LOGE("%{public}s: get sensor Module instance failed", __func__);
        return HDF_FAILURE;
    }

    SensorDevRegisterDump();
    return HDF_SUCCESS;
}

int32_t SensorImpl::GetAllSensorInfo(std::vector<HdfSensorInformationVdi> &info)
{
    HDF_LOGI("%{public}s: Enter the GetAllSensorInfo function.", __func__);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->GetAllSensors);

    struct SensorInformation *sensorInfo = nullptr;
    struct SensorInformation *tmp = nullptr;
    int32_t count = 0;

    StartTrace(HITRACE_TAG_SENSORS, "GetAllSensorInfo");
    int32_t ret = sensorInterface->GetAllSensors(&sensorInfo, &count);
    FinishTrace(HITRACE_TAG_SENSORS);
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
        HdfSensorInformationVdi hdfSensorInfo;
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
        hdfSensorInfo.minDelay = tmp->minDelay;
        hdfSensorInfo.maxDelay = tmp->maxDelay;
        info.push_back(std::move(hdfSensorInfo));
        tmp++;
    }

    return HDF_SUCCESS;
}

int32_t SensorImpl::Enable(int32_t sensorId)
{
    HDF_LOGI("%{public}s: Enter the Enable function, sensorId is %{public}d", __func__, sensorId);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->Enable);

    StartTrace(HITRACE_TAG_SENSORS, "Enable");
    int32_t ret = sensorInterface->Enable(sensorId);
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::Disable(int32_t sensorId)
{
    HDF_LOGI("%{public}s: Enter the Disable function, sensorId is %{public}d", __func__, sensorId);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->Disable);

    StartTrace(HITRACE_TAG_SENSORS, "Disable");
    int32_t ret = sensorInterface->Disable(sensorId);
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetBatch(int32_t sensorId, int64_t samplingInterval, int64_t reportInterval)
{
    HDF_LOGI("%{public}s: sensorId is %{public}d, samplingInterval is [%{public}" PRId64 "], \
        reportInterval is [%{public}" PRId64 "].", __func__, sensorId, samplingInterval, reportInterval);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->SetBatch);

    StartTrace(HITRACE_TAG_SENSORS, "SetBatch");
    int32_t ret = sensorInterface->SetBatch(sensorId, samplingInterval, reportInterval);
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetMode(int32_t sensorId, int32_t mode)
{
    HDF_LOGI("%{public}s: Enter the SetMode function, sensorId is %{public}d, mode is %{public}d",
        __func__, sensorId, mode);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->SetMode);

    StartTrace(HITRACE_TAG_SENSORS, "SetMode");
    int32_t ret = sensorInterface->SetMode(sensorId, mode);
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetMode failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::SetOption(int32_t sensorId, uint32_t option)
{
    HDF_LOGI("%{public}s: Enter the SetOption function, sensorId is %{public}d, option is %{public}u",
        __func__, sensorId, option);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->SetOption);

    StartTrace(HITRACE_TAG_SENSORS, "SetOption");
    int32_t ret = sensorInterface->SetOption(sensorId, option);
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
    }

    return ret;
}

int32_t SensorImpl::Register(int32_t groupId, const sptr<ISensorCallbackVdi> &callbackObj)
{
    HDF_LOGI("%{public}s: Enter the Register function, groupId is %{public}d", __func__, groupId);
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->Register);

    if (groupId < TRADITIONAL_SENSOR_TYPE || groupId > MEDICAL_SENSOR_TYPE) {
        HDF_LOGE("%{public}s: groupId [%{public}d] out of range", __func__, groupId);
        return SENSOR_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    auto groupCallBackIter = g_groupIdCallBackMap.find(groupId);
    if (groupCallBackIter != g_groupIdCallBackMap.end()) {
        auto callBackIter =
            find_if(g_groupIdCallBackMap[groupId].begin(), g_groupIdCallBackMap[groupId].end(),
            [&callbackObj](const sptr<ISensorCallbackVdi> &callbackRegistered) {
                const sptr<IRemoteObject> &lhs = callbackObj->HandleCallbackDeath();
                const sptr<IRemoteObject> &rhs = callbackRegistered->HandleCallbackDeath();
                return lhs == rhs;
            });
        if (callBackIter == g_groupIdCallBackMap[groupId].end()) {
            int32_t addResult = AddSensorDeathRecipient(callbackObj);
            if (addResult != SENSOR_SUCCESS) {
                return HDF_FAILURE;
            }
            g_groupIdCallBackMap[groupId].push_back(callbackObj);
        }
        return SENSOR_SUCCESS;
    }

    int32_t addResult = AddSensorDeathRecipient(callbackObj);
    if (addResult != SENSOR_SUCCESS) {
        return HDF_FAILURE;
    }
    int32_t ret = HDF_FAILURE;
    StartTrace(HITRACE_TAG_SENSORS, "Register");
    if (groupId == TRADITIONAL_SENSOR_TYPE) {
        ret = sensorInterface->Register(groupId, TradtionalSensorDataCallback);
    } else if (groupId == MEDICAL_SENSOR_TYPE) {
        ret = sensorInterface->Register(groupId, MedicalSensorDataCallback);
    }

    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        int32_t removeResult = RemoveSensorDeathRecipient(callbackObj);
        if (removeResult != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s: callback RemoveSensorDeathRecipient fail, groupId[%{public}d]", __func__, groupId);
        }
        return ret;
    }
    std::vector<sptr<ISensorCallbackVdi>> remoteVec;
    remoteVec.push_back(callbackObj);
    g_groupIdCallBackMap[groupId] = remoteVec;
    return ret;
}

int32_t SensorImpl::Unregister(int32_t groupId, const sptr<ISensorCallbackVdi> &callbackObj)
{
    HDF_LOGI("%{public}s: Enter the Unregister function, groupId is %{public}d", __func__, groupId);
    std::lock_guard<std::mutex> lock(g_mutex);
    const sptr<IRemoteObject> &remote = callbackObj->HandleCallbackDeath();
    StartTrace(HITRACE_TAG_SENSORS, "Unregister");
    int32_t ret = UnregisterImpl(groupId, remote.GetRefPtr());
    FinishTrace(HITRACE_TAG_SENSORS);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: Unregister failed groupId[%{public}d]", __func__, groupId);
    }

    return ret;
}

int32_t SensorImpl::UnregisterImpl(int32_t groupId, IRemoteObject *callbackObj)
{
    CHECK_SENSOR_MODULE_INSTANCE(sensorInterface, sensorInterface->Unregister);

    if (groupId < TRADITIONAL_SENSOR_TYPE || groupId > MEDICAL_SENSOR_TYPE) {
        HDF_LOGE("%{public}s: groupId [%{public}d] out of range", __func__, groupId);
        return SENSOR_INVALID_PARAM;
    }

    auto groupIdCallBackIter = g_groupIdCallBackMap.find(groupId);
    if (groupIdCallBackIter == g_groupIdCallBackMap.end()) {
        HDF_LOGE("%{public}s: groupId [%{public}d] callbackObj not registered", __func__, groupId);
        return HDF_FAILURE;
    }

    auto callBackIter =
        find_if(g_groupIdCallBackMap[groupId].begin(), g_groupIdCallBackMap[groupId].end(),
        [&callbackObj](const sptr<ISensorCallbackVdi> &callbackRegistered) {
            return callbackObj == (callbackRegistered->HandleCallbackDeath()).GetRefPtr();
        });
    if (callBackIter == g_groupIdCallBackMap[groupId].end()) {
        HDF_LOGE("%{public}s: groupId [%{public}d] callbackObj not registered", __func__, groupId);
        return HDF_FAILURE;
    }

    /**
     * when there is only one item in the vector,can call the Unregister function.
     * when there is more than one item in the vector, only need to remove the  callbackObj
     * from the vector
     */
    if (g_groupIdCallBackMap[groupId].size() > CALLBACK_CTOUNT_THRESHOLD) {
        int32_t removeResult = RemoveSensorDeathRecipient(*callBackIter);
        if (removeResult != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s: callback RemoveSensorDeathRecipient fail, groupId[%{public}d]", __func__, groupId);
        }
        g_groupIdCallBackMap[groupId].erase(callBackIter);
        return SENSOR_SUCCESS;
    }

    int32_t ret = HDF_FAILURE;
    if (groupId == TRADITIONAL_SENSOR_TYPE) {
        ret = sensorInterface->Unregister(groupId, TradtionalSensorDataCallback);
    } else if (groupId == MEDICAL_SENSOR_TYPE) {
        ret = sensorInterface->Unregister(groupId, MedicalSensorDataCallback);
    }

    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s failed, error code is %{public}d", __func__, ret);
        return ret;
    }

    int32_t removeResult = RemoveSensorDeathRecipient(*callBackIter);
    if (removeResult != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: last callback RemoveSensorDeathRecipient fail, groupId[%{public}d]", __func__, groupId);
    }
    g_groupIdCallBackMap.erase(groupId);

    return ret;
}

int32_t SensorImpl::AddSensorDeathRecipient(const sptr<ISensorCallbackVdi> &callbackObj)
{
    sptr<CallBackDeathRecipient> callBackDeathRecipient = new CallBackDeathRecipient(this);
    if (callBackDeathRecipient == nullptr) {
        HDF_LOGE("%{public}s: new CallBackDeathRecipient fail", __func__);
        return HDF_FAILURE;
    }
    const sptr<IRemoteObject> &remote = callbackObj->HandleCallbackDeath();
    bool result = remote->AddDeathRecipient(callBackDeathRecipient);
    if (!result) {
        HDF_LOGE("%{public}s: AddDeathRecipient fail", __func__);
        return HDF_FAILURE;
    }
    g_callBackDeathRecipientMap[remote.GetRefPtr()] = callBackDeathRecipient;
    return SENSOR_SUCCESS;
}

int32_t SensorImpl::RemoveSensorDeathRecipient(const sptr<ISensorCallbackVdi> &callbackObj)
{
    const sptr<IRemoteObject> &remote = callbackObj->HandleCallbackDeath();
    auto callBackDeathRecipientIter = g_callBackDeathRecipientMap.find(remote.GetRefPtr());
    if (callBackDeathRecipientIter == g_callBackDeathRecipientMap.end()) {
        HDF_LOGE("%{public}s: not find recipient", __func__);
        return HDF_FAILURE;
    }
    bool result = remote->RemoveDeathRecipient(callBackDeathRecipientIter->second);
    g_callBackDeathRecipientMap.erase(callBackDeathRecipientIter);
    if (!result) {
        HDF_LOGE("%{public}s: RemoveDeathRecipient fail", __func__);
        return HDF_FAILURE;
    }
    return SENSOR_SUCCESS;
}

void SensorImpl::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    HDF_LOGI("%{public}s: sensor OnRemoteDied enter", __func__);
    std::lock_guard<std::mutex> lock(g_mutex);
    sptr<IRemoteObject> callbackObject = object.promote();
    if (callbackObject == nullptr) {
        return;
    }

    int32_t groupId = TRADITIONAL_SENSOR_TYPE;
    auto traditionIter = g_groupIdCallBackMap.find(groupId);
    if (traditionIter != g_groupIdCallBackMap.end()) {
        auto callBackIter =
        find_if(g_groupIdCallBackMap[groupId].begin(), g_groupIdCallBackMap[groupId].end(),
        [&callbackObject](const sptr<ISensorCallbackVdi> &callbackRegistered) {
            return callbackObject.GetRefPtr() == (callbackRegistered->HandleCallbackDeath()).GetRefPtr();
        });
        if (callBackIter != g_groupIdCallBackMap[groupId].end()) {
            int32_t ret = UnregisterImpl(groupId, callbackObject.GetRefPtr());
            if (ret != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s: traditional Unregister failed groupId[%{public}d]", __func__, groupId);
            }
        }
    }

    groupId = MEDICAL_SENSOR_TYPE;
    auto medicalIter = g_groupIdCallBackMap.find(groupId);
    if (medicalIter != g_groupIdCallBackMap.end()) {
        auto callBackIter =
        find_if(g_groupIdCallBackMap[groupId].begin(), g_groupIdCallBackMap[groupId].end(),
        [&callbackObject](const sptr<ISensorCallbackVdi> &callbackRegistered) {
            return callbackObject.GetRefPtr() == (callbackRegistered->HandleCallbackDeath()).GetRefPtr();
        });
        if (callBackIter != g_groupIdCallBackMap[groupId].end()) {
            int32_t ret = UnregisterImpl(groupId, callbackObject.GetRefPtr());
            if (ret != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s: medical Unregister failed groupId[%{public}d]", __func__, groupId);
            }
        }
    }
}

static int32_t CreateSensorVdiInstance(struct HdfVdiBase *vdiBase)
{
    HDF_LOGI("%{public}s", __func__);
    if (vdiBase == nullptr) {
        HDF_LOGI("%{public}s: parameter vdiBase is NULL", __func__);
        return HDF_FAILURE;
    }

    struct WrapperSensorVdi *sensorVdi = reinterpret_cast<struct WrapperSensorVdi *>(vdiBase);
    sensorVdi->sensorModule = new SensorImpl();
    if (sensorVdi->sensorModule == nullptr) {
        HDF_LOGI("%{public}s: new sensorModule failed!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t DestorySensorVdiInstance(struct HdfVdiBase *vdiBase)
{
    HDF_LOGI("%{public}s", __func__);
    if (vdiBase == nullptr) {
        HDF_LOGI("%{public}s: parameter vdiBase is NULL", __func__);
        return HDF_FAILURE;
    }

    struct WrapperSensorVdi *sensorVdi = reinterpret_cast<struct WrapperSensorVdi *>(vdiBase);
    SensorImpl *impl = reinterpret_cast<SensorImpl *>(sensorVdi->sensorModule);
    if (impl != nullptr) {
        delete impl;
        sensorVdi->sensorModule = nullptr;
    }
    return HDF_SUCCESS;
}

static struct WrapperSensorVdi g_sensorVdi = {
    .base = {
        .moduleVersion = 1,
        .moduleName = "sensor_Service",
        .CreateVdiInstance = CreateSensorVdiInstance,
        .DestoryVdiInstance = DestorySensorVdiInstance,
    },
    .sensorModule = nullptr,
};

extern "C" HDF_VDI_INIT(g_sensorVdi);

} // namespace V1_1
} // namespace Sensor
} // namespace HDI
} // namespace OHOS