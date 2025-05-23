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

#ifndef HDI_SENSOR_MANAGER_H
#define HDI_SENSOR_MANAGER_H

#include <unordered_map>
#include <vector>
#include <set>
#include <hdf_remote_service.h>
#include "v2_0/isensor_interface.h"
#include "isensor_interface_vdi.h"
#include "sensor_client_info.h"
#include "sensor_trace.h"

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V2_1 {

constexpr uint32_t MAX_DUMP_DATA_SIZE = 10;

struct SensorsDataPack {
    int32_t count;
    int32_t pos;
    struct HdfSensorEvents listDumpArray[MAX_DUMP_DATA_SIZE];
};

class SensorClientsManager {
public:
    ~SensorClientsManager();
    void ReportDataCbRegister(int groupId, int serviceId, const sptr<V2_0::ISensorCallback> &callbackObj);
    void ReportDataCbUnRegister(int groupId, int serviceId, const sptr<V2_0::ISensorCallback> &callbackObj);
    void ReportDataCbOneWay(int groupId, int serviceId);
    void SetSensorBestConfig(int sensorId, int64_t &samplingInterval, int64_t &reportInterval);
    void SetSdcSensorBestConfig(int sensorId, int64_t &samplingInterval, int64_t &reportInterval);
    void GetSensorBestConfig(int sensorId, int64_t &samplingInterval, int64_t &reportInterval);
    void EraseSdcSensorBestConfig(int sensorId);
    bool IsUpadateSensorState(int sensorId, int serviceId, bool isOpen);
    static bool IsNotNeedReportData(SensorClientInfo &sensorClientInfo, const int32_t &sensorId,
                                    const int32_t &serviceId);
    std::string ReportEachClient(const V2_0::HdfSensorEvents& event);
    bool GetClients(int groupId, std::unordered_map<int32_t, SensorClientInfo> &client);
    std::set<int32_t> GetServiceIds(int32_t &sensorId);
    bool GetBestSensorConfigMap(std::unordered_map<int32_t, struct BestSensorConfig> &map);
    bool IsClientsEmpty(int groupId);
    bool IsNoSensorUsed();
    std::unordered_map<int32_t, std::set<int32_t>> GetSensorUsed();
    bool IsNeedOpenSensor(int sensorId, int serviceId);
    bool IsNeedCloseSensor(int sensorId, int serviceId);
    bool IsExistSdcSensorEnable(int sensorId);
    void OpenSensor(int sensorId, int serviceId);
    void UpdateSensorConfig(int sensorId, int64_t samplingInterval, int64_t reportInterval);
    void UpdateSdcSensorConfig(int sensorId, int64_t samplingInterval, int64_t reportInterval);
    int GetServiceId(int groupId, const sptr<V2_0::ISensorCallback> &callbackObj);
    static SensorClientsManager* GetInstance();
    std::mutex clientsMutex_;
    std::mutex sensorUsedMutex_;
    std::mutex sensorConfigMutex_;
    std::mutex sdcSensorConfigMutex_;
    std::mutex sensorInfoMutex_;
    std::mutex sensorsDataPackMutex_;
    struct SensorInfoId {
        int32_t sensorId = 0;
        int32_t serviceId = 0;
    };
    void SetClientSenSorConfig(int32_t sensorId, int32_t serviceId, int64_t samplingInterval, int64_t &reportInterval);
    static bool IsSensorContinues(int32_t sensorId);
    void UpdateClientPeriodCount(int sensorId, int64_t samplingInterval, int64_t reportInterval);
    void CopySensorInfo(std::vector<HdfSensorInformation> &info, bool cFlag);
    void GetEventData(struct SensorsDataPack &dataPack);
    void CopyEventData(const struct HdfSensorEvents event);
    void ReSetSensorPrintTime(int32_t sensorId);
    bool IsSensorNeedPrint(int32_t sensorId);
    void SetSensorVdiImpl(OHOS::HDI::Sensor::V1_1::ISensorInterfaceVdi *sensorVdiImpl);
    void EraseClients(int32_t groupId, int32_t serviceId);
    void DeleteService(int32_t sensorId, int32_t serviceId);
    void HdiReportData(const sptr<V2_0::ISensorCallback> &callbackObj, const V2_0::HdfSensorEvents& event,
        std::string &result, const bool &oneway, SensorInfoId sensorInfoId);
private:
    SensorClientsManager();
    static std::mutex instanceMutex_;
    std::unordered_map<int32_t, std::unordered_map<int, SensorClientInfo>> clients_;
    std::unordered_map<int32_t, std::set<int32_t>> sensorUsed_;
    std::unordered_map<int32_t, struct BestSensorConfig> sensorConfig_;
    std::unordered_map<int32_t, struct BestSensorConfig> sdcSensorConfig_;
    std::unordered_map<int32_t, int32_t> sensorPrintTimes_;
    std::mutex sensorPrintTimesMutex_;
    std::vector<HdfSensorInformation> sensorInfo_;
    SensorsDataPack listDump_ = {0};
    OHOS::HDI::Sensor::V1_1::ISensorInterfaceVdi *sensorVdiImpl_ = nullptr;
};

struct BestSensorConfig {
    int64_t samplingInterval;
    int64_t reportInterval;
};

} // V2_1
} // Sensor
} // HDI
} // OHOS

#endif // HDI_SENSOR_MANAGER_H