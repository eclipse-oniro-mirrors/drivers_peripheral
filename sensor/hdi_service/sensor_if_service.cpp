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

#include "sensor_if_service.h"
#include <refbase.h>
#include <cinttypes>
#include "sensor_uhdf_log.h"
#include "hitrace_meter.h"
#include "sensor_type.h"
#include "sensor_callback_vdi.h"
#include <hdf_remote_service.h>
#include "callback_death_recipient.h"
#include "sensor_hdi_dump.h"
#include "devhost_dump_reg.h"

constexpr int DISABLE_SENSOR = 0;
constexpr int REPORT_INTERVAL = 0;
constexpr int UNREGISTER_SENSOR = 0;
constexpr int REGISTER_SENSOR = 1;
constexpr int ENABLE_SENSOR = 1;
constexpr int COMMON_REPORT_FREQUENCY = 1000000000;
constexpr int COPY_SENSORINFO = 1;
enum BatchSeniorMode {
        SA = 0,
        SDC = 1
};

#define HDF_LOG_TAG uhdf_sensor_service

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V2_0 {
namespace {
    constexpr int32_t CALLBACK_CTOUNT_THRESHOLD = 1;
    using CallBackDeathRecipientMap = std::unordered_map<IRemoteObject *, sptr<CallBackDeathRecipient>>;
    CallBackDeathRecipientMap g_callBackDeathRecipientMap;
}

SensorIfService::SensorIfService()
{
    int32_t ret = GetSensorVdiImpl();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: get sensor vdi instance failed", __func__);
    }
}

SensorIfService::~SensorIfService()
{
    if (vdi_ != nullptr) {
        HdfCloseVdi(vdi_);
    }
    RemoveDeathNotice(TRADITIONAL_SENSOR_TYPE);
    RemoveDeathNotice(MEDICAL_SENSOR_TYPE);
}

void SensorIfService::RegisteDumpHost()
{
    int32_t ret = DevHostRegisterDumpHost(GetSensorDump);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: DevHostRegisterDumpHost error", __func__);
    }
    return;
}

int32_t SensorIfService::GetSensorVdiImpl()
{
    struct OHOS::HDI::Sensor::V1_1::WrapperSensorVdi *wrapperSensorVdi = nullptr;
    uint32_t version = 0;
    vdi_ = HdfLoadVdi(HDI_SENSOR_VDI_LIBNAME);
    if (vdi_ == nullptr || vdi_->vdiBase == nullptr) {
        HDF_LOGE("%{public}s: load sensor vdi failed", __func__);
        return HDF_FAILURE;
    }

    version = HdfGetVdiVersion(vdi_);
    if (version != 1) {
        HDF_LOGE("%{public}s: get sensor vdi version failed", __func__);
        return HDF_FAILURE;
    }

    wrapperSensorVdi = reinterpret_cast<struct OHOS::HDI::Sensor::V1_1::WrapperSensorVdi *>(vdi_->vdiBase);
    sensorVdiImpl_ = wrapperSensorVdi->sensorModule;
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor impl failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t SensorIfService::Init()
{
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }
    int32_t ret = sensorVdiImpl_->Init();
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s Init failed, error code is %{public}d", __func__, ret);
    }
#ifdef SENSOR_DEBUG
    RegisteDumpHost();
#endif
    return ret;
}

sptr<SensorCallbackVdi> SensorIfService::GetSensorCb(int32_t groupId, const sptr<ISensorCallback> &callbackObj,
    bool cbFlag)
{
    if (groupId == TRADITIONAL_SENSOR_TYPE) {
        if (cbFlag) {
            traditionalCb = new SensorCallbackVdi(callbackObj);
        }
        return traditionalCb;
    }
    if (cbFlag) {
        medicalCb = new SensorCallbackVdi(callbackObj);
    }
    return medicalCb;
}

int32_t SensorIfService::GetAllSensorInfo(std::vector<HdfSensorInformation> &info)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    HDF_LOGD("%{public}s: Enter the GetAllSensorInfo function.", __func__);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    std::vector<OHOS::HDI::Sensor::V1_1::HdfSensorInformationVdi> sensorInfoVdi = {};
    StartTrace(HITRACE_TAG_HDF, "GetAllSensorInfo");
    int32_t ret = sensorVdiImpl_->GetAllSensorInfo(sensorInfoVdi);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s GetAllSensors failed, error code is %{public}d", __func__, ret);
        return ret;
    }
    FinishTrace(HITRACE_TAG_HDF);

    if (sensorInfoVdi.empty()) {
        HDF_LOGE("%{public}s no sensor info in list", __func__);
        return HDF_FAILURE;
    }

    for (const auto &it : sensorInfoVdi) {
        struct HdfSensorInformation sensorInfo = {};
        sensorInfo.sensorName = it.sensorName;
        sensorInfo.vendorName = it.vendorName;
        sensorInfo.firmwareVersion = it.firmwareVersion;
        sensorInfo.hardwareVersion = it.hardwareVersion;
        sensorInfo.sensorTypeId = it.sensorTypeId;
        sensorInfo.sensorId = it.sensorId;
        sensorInfo.maxRange = it.maxRange;
        sensorInfo.accuracy = it.accuracy;
        sensorInfo.power = it.power;
        sensorInfo.minDelay = it.minDelay;
        sensorInfo.maxDelay = it.maxDelay;
        sensorInfo.fifoMaxEventCount = it.fifoMaxEventCount;
        info.push_back(std::move(sensorInfo));

        SensorClientsManager::GetInstance()->CopySensorInfo(info, COPY_SENSORINFO);
    }

    return HDF_SUCCESS;
}

int32_t SensorIfService::Enable(int32_t sensorId)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());
    HDF_LOGD("%{public}s:Enter the Enable function, sensorId %{public}d, service %{public}d",
             __func__, sensorId, serviceId);
    SensorCallbackVdi::servicesChanged = true;
    if (!SensorClientsManager::GetInstance()->IsUpadateSensorState(sensorId, serviceId, ENABLE_SENSOR)) {
        return HDF_SUCCESS;
    }
    
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    StartTrace(HITRACE_TAG_HDF, "Enable");
    int32_t ret = sensorVdiImpl_->Enable(sensorId);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s Enable failed, error code is %{public}d", __func__, ret);
    } else {
        SensorClientsManager::GetInstance()->OpenSensor(sensorId, serviceId);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::Disable(int32_t sensorId)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());
    HDF_LOGD("%{public}s:Enter the Disable function, sensorId %{public}d, service %{public}d",
             __func__, sensorId, serviceId);
    SensorCallbackVdi::servicesChanged = true;
    if (!SensorClientsManager::GetInstance()->IsUpadateSensorState(sensorId, serviceId, DISABLE_SENSOR)) {
        HDF_LOGE("%{public}s There are still some services enable", __func__);
        return HDF_SUCCESS;
    }

    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }
    
    int32_t ret;
    if (SensorClientsManager::GetInstance()->IsExistSdcSensorEnable(sensorId)) {
        ret = sensorVdiImpl_->SetSaBatch(sensorId, REPORT_INTERVAL, REPORT_INTERVAL);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s SetBatchSenior SA failed, error code is %{public}d", __func__, ret);
            return ret;
        }
        return HDF_SUCCESS;
    }

    StartTrace(HITRACE_TAG_HDF, "Disable");
    ret = sensorVdiImpl_->Disable(sensorId);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s Disable failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::SetBatch(int32_t sensorId, int64_t samplingInterval, int64_t reportInterval)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    HDF_LOGD("%{public}s: sensorId is %{public}d, samplingInterval is [%{public}" PRId64 "], \
        reportInterval is [%{public}" PRId64 "].", __func__, sensorId, samplingInterval, reportInterval);
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());

    StartTrace(HITRACE_TAG_HDF, "SetBatch");
    int32_t ret = SetBatchSenior(serviceId, sensorId, SA, samplingInterval, reportInterval);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetBatch failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::SetBatchSenior(int32_t serviceId, int32_t sensorId, int32_t mode, int64_t samplingInterval,
                                        int64_t reportInterval)
{
    HDF_LOGI("%{public}s: serviceId is %{public}d, sensorId is %{public}d, mode is %{public}d, samplingInterval is "
             "[%{public}" PRId64 "], reportInterval is [%{public}" PRId64 "].", __func__, serviceId, sensorId, mode,
             samplingInterval, reportInterval);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }
    StartTrace(HITRACE_TAG_HDF, "SetBatchSenior");
    SensorClientsManager::GetInstance()->SetClientSenSorConfig(sensorId, serviceId, samplingInterval, reportInterval);

    int64_t saSamplingInterval = samplingInterval;
    int64_t saReportInterval = reportInterval;
    int64_t sdcSamplingInterval = samplingInterval;
    int64_t sdcReportInterval = reportInterval;

    SensorClientsManager::GetInstance()->SetSensorBestConfig(sensorId, saSamplingInterval, saReportInterval);
    SensorClientsManager::GetInstance()->SetSdcSensorBestConfig(sensorId, sdcSamplingInterval, sdcReportInterval);

    samplingInterval = saSamplingInterval < sdcSamplingInterval ? saSamplingInterval : sdcSamplingInterval;
    reportInterval = saReportInterval < sdcReportInterval ? saReportInterval : sdcReportInterval;

    int32_t ret = sensorVdiImpl_->SetBatch(sensorId, samplingInterval, reportInterval);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetBatch failed, error code is %{public}d", __func__, ret);
        return ret;
    }
    if (mode == SA) {
        SensorClientsManager::GetInstance()->UpdateSensorConfig(sensorId, saSamplingInterval, saReportInterval);
        SensorClientsManager::GetInstance()->UpdateClientPeriodCount(sensorId, saSamplingInterval, saReportInterval);
    }
    if (mode == SDC) {
        SensorClientsManager::GetInstance()->UpdateSdcSensorConfig(sensorId, sdcSamplingInterval, sdcReportInterval);
    }
    SensorClientsManager::GetInstance()->GetSensorBestConfig(sensorId, samplingInterval, reportInterval);
    ret = sensorVdiImpl_->SetSaBatch(sensorId, samplingInterval, samplingInterval);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetBatch failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::SetMode(int32_t sensorId, int32_t mode)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    HDF_LOGD("%{public}s: Enter the SetMode function, sensorId is %{public}d, mode is %{public}d",
        __func__, sensorId, mode);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    StartTrace(HITRACE_TAG_HDF, "SetMode");
    int32_t ret = sensorVdiImpl_->SetMode(sensorId, mode);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetMode failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::SetOption(int32_t sensorId, uint32_t option)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    HDF_LOGD("%{public}s: Enter the SetOption function, sensorId is %{public}d, option is %{public}u",
        __func__, sensorId, option);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    StartTrace(HITRACE_TAG_HDF, "SetOption");
    int32_t ret = sensorVdiImpl_->SetOption(sensorId, option);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s SetOption failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::Register(int32_t groupId, const sptr<ISensorCallback> &callbackObj)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    int32_t ret = HDF_SUCCESS;
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());
    HDF_LOGD("%{public}s:Enter the Register function, groupId %{public}d, service %{public}d",
        __func__, groupId, serviceId);
    int32_t result = AddCallbackMap(groupId, callbackObj);
    if (result !=SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: AddCallbackMap failed groupId[%{public}d]", __func__, groupId);
    }
    if (SensorClientsManager::GetInstance()->IsClientsEmpty(groupId)) {
        if (sensorVdiImpl_ == nullptr) {
            HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
            return HDF_FAILURE;
        }
        StartTrace(HITRACE_TAG_HDF, "Register");
        sptr<SensorCallbackVdi> sensorCb = GetSensorCb(groupId, callbackObj, REGISTER_SENSOR);
        if (sensorCb == nullptr) {
            HDF_LOGE("%{public}s: get sensorcb fail, groupId[%{public}d]", __func__, groupId);
            return HDF_FAILURE;
        }
        ret = sensorVdiImpl_->Register(groupId, sensorCb);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s Register failed, error code is %{public}d", __func__, ret);
            int32_t removeResult = RemoveSensorDeathRecipient(callbackObj);
            if (removeResult != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s: callback RemoveSensorDeathRecipient fail, groupId[%{public}d]",
                    __func__, groupId);
            }
        } else {
            SensorClientsManager::GetInstance()->ReportDataCbRegister(groupId, serviceId, callbackObj);
        }
        FinishTrace(HITRACE_TAG_HDF);
    } else {
        SensorClientsManager::GetInstance()->ReportDataCbRegister(groupId, serviceId, callbackObj);
    }
    SensorCallbackVdi::clientsChanged = true;
    return ret;
}

int32_t SensorIfService::Unregister(int32_t groupId, const sptr<ISensorCallback> &callbackObj)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());
    HDF_LOGD("%{public}s:Enter the Unregister function, groupId %{public}d, service %{public}d",
        __func__, groupId, serviceId);
    int32_t result = RemoveCallbackMap(groupId, serviceId, callbackObj);
    if (result !=SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: RemoveCallbackMap failed groupId[%{public}d]", __func__, groupId);
    }
    SensorClientsManager::GetInstance()->ReportDataCbUnRegister(groupId, serviceId, callbackObj);
    SensorCallbackVdi::clientsChanged = true;
    if (!SensorClientsManager::GetInstance()->IsClientsEmpty(groupId)) {
        HDF_LOGD("%{public}s: clients is not empty, do not unregister", __func__);
        return HDF_SUCCESS;
    }

    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    StartTrace(HITRACE_TAG_HDF, "Unregister");
    sptr<SensorCallbackVdi> sensorCb = GetSensorCb(groupId, callbackObj, UNREGISTER_SENSOR);
    if (sensorCb == nullptr) {
        HDF_LOGE("%{public}s: get sensorcb fail, groupId[%{public}d]", __func__, groupId);
        return HDF_FAILURE;
    }
    int32_t ret = sensorVdiImpl_->Unregister(groupId, sensorCb);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: Unregister failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    return ret;
}

int32_t SensorIfService::AddCallbackMap(int32_t groupId, const sptr<ISensorCallback> &callbackObj)
{
    auto groupCallBackIter = callbackMap.find(groupId);
    if (groupCallBackIter != callbackMap.end()) {
        auto callBackIter =
            find_if(callbackMap[groupId].begin(), callbackMap[groupId].end(),
            [&callbackObj](const sptr<ISensorCallback> &callbackRegistered) {
                const sptr<IRemoteObject> &lhs = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackObj);
                const sptr<IRemoteObject> &rhs = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackRegistered);
                return lhs == rhs;
            });
        if (callBackIter == callbackMap[groupId].end()) {
            int32_t addResult = AddSensorDeathRecipient(callbackObj);
            if (addResult != SENSOR_SUCCESS) {
                return HDF_FAILURE;
            }
            callbackMap[groupId].push_back(callbackObj);
        }
    } else {
        int32_t addResult = AddSensorDeathRecipient(callbackObj);
        if (addResult != SENSOR_SUCCESS) {
            return HDF_FAILURE;
        }
        std::vector<sptr<ISensorCallback>> remoteVec;
        remoteVec.push_back(callbackObj);
        callbackMap[groupId] = remoteVec;
    }
    return SENSOR_SUCCESS;
}

int32_t SensorIfService::RemoveCallbackMap(int32_t groupId, int serviceId, const sptr<ISensorCallback> &callbackObj)
{
    auto groupIdCallBackIter = callbackMap.find(groupId);
    if (groupIdCallBackIter == callbackMap.end()) {
        HDF_LOGE("%{public}s: groupId [%{public}d] callbackObj not registered", __func__, groupId);
        return HDF_FAILURE;
    }
    auto callBackIter =
        find_if(callbackMap[groupId].begin(), callbackMap[groupId].end(),
        [&callbackObj](const sptr<ISensorCallback> &callbackRegistered) {
            const sptr<IRemoteObject> &lhs = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackObj);
            const sptr<IRemoteObject> &rhs = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackRegistered);
            return lhs == rhs;
        });
    if (callBackIter == callbackMap[groupId].end()) {
        HDF_LOGE("%{public}s: groupId [%{public}d] callbackObj not registered", __func__, groupId);
        return HDF_FAILURE;
    }
    int32_t removeResult = RemoveSensorDeathRecipient(*callBackIter);
    if (removeResult != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s: last callback RemoveSensorDeathRecipient fail, groupId[%{public}d]", __func__, groupId);
    }
    if (callbackMap[groupId].size() > CALLBACK_CTOUNT_THRESHOLD) {
        callbackMap[groupId].erase(callBackIter);
    } else {
        callbackMap.erase(groupId);
    }
    std::unordered_map<int, std::set<int>> sensorEnabled = SensorClientsManager::GetInstance()->GetSensorUsed();
    for (auto iter : sensorEnabled) {
        if (iter.second.find(serviceId) == iter.second.end()) {
            continue;
        }
        if (!SensorClientsManager::GetInstance()->IsUpadateSensorState(iter.first, serviceId, DISABLE_SENSOR)) {
            continue;
        }
        std::unordered_map<int, std::set<int>> sensorUsed = SensorClientsManager::GetInstance()->GetSensorUsed();
        if (sensorUsed.find(iter.first) == sensorUsed.end()) {
            if (sensorVdiImpl_ == nullptr) {
                HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
                return HDF_FAILURE;
            }
            int32_t ret = sensorVdiImpl_->Disable(iter.first);
            if (ret != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s Disable failed, error code is %{public}d", __func__, ret);
            }
        }
    }
    return SENSOR_SUCCESS;
}

int32_t SensorIfService::AddSensorDeathRecipient(const sptr<ISensorCallback> &callbackObj)
{
    sptr<CallBackDeathRecipient> callBackDeathRecipient = new CallBackDeathRecipient(this);
    if (callBackDeathRecipient == nullptr) {
        HDF_LOGE("%{public}s: new CallBackDeathRecipient fail", __func__);
        return HDF_FAILURE;
    }
    const sptr<IRemoteObject> &remote = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackObj);
    bool result = remote->AddDeathRecipient(callBackDeathRecipient);
    if (!result) {
        HDF_LOGE("%{public}s: AddDeathRecipient fail", __func__);
        return HDF_FAILURE;
    }
    g_callBackDeathRecipientMap[remote.GetRefPtr()] = callBackDeathRecipient;
    return SENSOR_SUCCESS;
}

int32_t SensorIfService::RemoveSensorDeathRecipient(const sptr<ISensorCallback> &callbackObj)
{
    const sptr<IRemoteObject> &remote = OHOS::HDI::hdi_objcast<ISensorCallback>(callbackObj);
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

void SensorIfService::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    sptr<IRemoteObject> callbackObject = object.promote();
    if (callbackObject == nullptr) {
        return;
    }

    for (int32_t groupId = TRADITIONAL_SENSOR_TYPE; groupId < SENSOR_GROUP_TYPE_MAX; groupId++) {
        auto groupIdIter = callbackMap.find(groupId);
        if (groupIdIter == callbackMap.end()) {
            continue;
        }
        auto callBackIter =
        find_if(callbackMap[groupId].begin(), callbackMap[groupId].end(),
        [&callbackObject](const sptr<ISensorCallback> &callbackRegistered) {
            return callbackObject.GetRefPtr() ==
                (OHOS::HDI::hdi_objcast<ISensorCallback>(callbackRegistered)).GetRefPtr();
        });
        if (callBackIter != callbackMap[groupId].end()) {
            int32_t serviceId = SensorClientsManager::GetInstance()->GetServiceId(groupId, *callBackIter);
            if (serviceId == HDF_FAILURE) {
                HDF_LOGE("%{public}s: GetServiceId failed", __func__);
            }
            int32_t ret = RemoveCallbackMap(groupId, serviceId, *callBackIter);
            if (ret != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s: Unregister failed groupId[%{public}d]", __func__, groupId);
            }
            if (!SensorClientsManager::GetInstance()->IsClientsEmpty(groupId)) {
                HDF_LOGD("%{public}s: clients is not empty, do not unregister", __func__);
                continue;
            }
            if (sensorVdiImpl_ == nullptr) {
                HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
                continue;
            }
            sptr<SensorCallbackVdi> sensorCb = GetSensorCb(groupId, *callBackIter, UNREGISTER_SENSOR);
            if (sensorCb == nullptr) {
                HDF_LOGE("%{public}s: get sensorcb fail, groupId[%{public}d]", __func__, groupId);
                continue;
            }
            ret = sensorVdiImpl_->Unregister(groupId, sensorCb);
            if (ret != SENSOR_SUCCESS) {
                HDF_LOGE("%{public}s: Unregister failed, error code is %{public}d", __func__, ret);
            }
        }
    }
}

void  SensorIfService::RemoveDeathNotice(int32_t sensorType)
{
    auto iter = callbackMap.find(sensorType);
    if (iter != callbackMap.end()) {
        return;
    }
    for (auto callback : callbackMap[sensorType]) {
        const sptr<IRemoteObject> &remote = OHOS::HDI::hdi_objcast<ISensorCallback>(callback);
        auto recipientIter = g_callBackDeathRecipientMap.find(remote.GetRefPtr());
        if (recipientIter != g_callBackDeathRecipientMap.end()) {
            bool removeResult = remote->RemoveDeathRecipient(recipientIter->second);
            if (!removeResult) {
                HDF_LOGE("%{public}s: sensor destroyed, callback RemoveSensorDeathRecipient fail", __func__);
            }
        }
    }
}

int32_t SensorIfService::ReadData(int32_t sensorId, std::vector<HdfSensorEvents> &event)
{
    HDF_LOGD("%{public}s: Enter the ReadData function", __func__);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t SensorIfService::SetSdcSensor(int32_t sensorId, bool enabled, int32_t rateLevel)
{
    HDF_LOGD("%{public}s: Enter the SetSdcSensor function, sensorId is %{public}d, enabled is %{public}u, \
             rateLevel is %{public}u", __func__, sensorId, enabled, rateLevel);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }
    StartTrace(HITRACE_TAG_HDF, "SetSdcSensor");
    int32_t ret;
    if (rateLevel < REPORT_INTERVAL) {
        HDF_LOGE("%{public}s: rateLevel cannot less than zero", __func__);
        return HDF_FAILURE;
    }
    int64_t samplingInterval = rateLevel == REPORT_INTERVAL ? REPORT_INTERVAL : COMMON_REPORT_FREQUENCY / rateLevel;
    int64_t reportInterval = REPORT_INTERVAL;
    uint32_t serviceId = static_cast<uint32_t>(HdfRemoteGetCallingPid());
    if (enabled) {
        std::unique_lock<std::mutex> lock(sensorServiceMutex_);
        ret = SetBatchSenior(serviceId, sensorId, SDC, samplingInterval, reportInterval);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s SetBatchSenior SDC failed, error code is %{public}d", __func__, ret);
            return ret;
        }
        ret = sensorVdiImpl_->Enable(sensorId);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s Enable failed, error code is %{public}d", __func__, ret);
            return ret;
        }
    } else {
        SensorClientsManager::GetInstance()->EraseSdcSensorBestConfig(sensorId);
        ret = Disable(sensorId);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s Disable failed, error code is %{public}d", __func__, ret);
            return ret;
        }
        SensorClientsManager::GetInstance()->GetSensorBestConfig(sensorId, samplingInterval, reportInterval);
        ret = sensorVdiImpl_->SetSaBatch(sensorId, samplingInterval, samplingInterval);
        if (ret != SENSOR_SUCCESS) {
            HDF_LOGE("%{public}s SetSaBatch failed, error code is %{public}d", __func__, ret);
            return ret;
        }
    }
    FinishTrace(HITRACE_TAG_HDF);
    return ret;
}

int32_t SensorIfService::GetSdcSensorInfo(std::vector<SdcSensorInfo>& sdcSensorInfo)
{
    std::unique_lock<std::mutex> lock(sensorServiceMutex_);
    HDF_LOGD("%{public}s: Enter the GetSdcSensorInfo function", __func__);
    if (sensorVdiImpl_ == nullptr) {
        HDF_LOGE("%{public}s: get sensor vdi impl failed", __func__);
        return HDF_FAILURE;
    }

    std::vector<OHOS::HDI::Sensor::V1_1::SdcSensorInfoVdi> sdcSensorInfoVdi;
    StartTrace(HITRACE_TAG_HDF, "GetSdcSensorInfo");
    int32_t ret = sensorVdiImpl_->GetSdcSensorInfo(sdcSensorInfoVdi);
    if (ret != SENSOR_SUCCESS) {
        HDF_LOGE("%{public}s GetSdcSensorInfo failed, error code is %{public}d", __func__, ret);
    }
    FinishTrace(HITRACE_TAG_HDF);

    for (auto infoVdi : sdcSensorInfoVdi) {
        SdcSensorInfo info;
        info.offset = infoVdi.offset;
        info.sensorId = infoVdi.sensorId;
        info.ddrSize = infoVdi.ddrSize;
        info.minRateLevel = infoVdi.minRateLevel;
        info.maxRateLevel = infoVdi.maxRateLevel;
        info.memAddr = infoVdi.memAddr;
        info.reserved = infoVdi.reserved;
        sdcSensorInfo.push_back(std::move(info));
    }

    return ret;
}

extern "C" ISensorInterface *SensorInterfaceImplGetInstance(void)
{
    SensorIfService *impl = new (std::nothrow) SensorIfService();
    if (impl == nullptr) {
        return nullptr;
    }

    int32_t ret = impl->Init();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: service init failed, error code is %{public}d", __func__, ret);
        delete impl;
        return nullptr;
    }

    return impl;
}
} // namespace V2_0
} // namespace Sensor
} // namespace HDI
} // namespace OHOS