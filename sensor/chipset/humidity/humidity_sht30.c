/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "humidity_sht30.h"
#include <securec.h>
#include "osal_mem.h"
#include "osal_time.h"
#include "sensor_config_controller.h"
#include "sensor_device_manager.h"
#include "sensor_humidity_driver.h"

#define HDF_LOG_TAG    hdf_sensor_humidity_driver

static struct Sht30DrvData *g_sht30DrvData = NULL;

static struct Sht30DrvData *Sht30GetDrvData(void)
{
    return g_sht30DrvData;
}

static uint8_t Sht30CalcCrc8(const uint8_t *data, uint32_t dataLen)
{
    uint8_t value = SHT30_HUM_CRC8_BASE;

    for (uint32_t i = dataLen; i; --i) {
        value ^= *data++;
        for (uint32_t j = SENSOR_DATA_WIDTH_8_BIT; j; --j) {
            value = (value & SHT30_HUM_CRC8_MASK) ? ((value << SHT30_HUM_SHFIT_1_BIT) ^ SHT30_HUM_CRC8_POLYNOMIAL) : \
                (value << SHT30_HUM_SHFIT_1_BIT);
        }
    }

    return value;
}

static int32_t ReadSht30RawData(struct SensorCfgData *data, struct HumidityData *rawData, uint64_t *timestamp)
{
    OsalTimespec time;
    uint8_t value[SHT30_HUM_DATA_BUF_LEN];
    uint16_t tempValue;

    (void)memset_s(&time, sizeof(time), 0, sizeof(time));

    CHECK_NULL_PTR_RETURN_VALUE(data, HDF_ERR_INVALID_PARAM);

    if (OsalGetTime(&time) != HDF_SUCCESS) {
        HDF_LOGE("%s: Get time failed", __func__);
        return HDF_FAILURE;
    }

    *timestamp = time.sec * SENSOR_SECOND_CONVERT_NANOSECOND + time.usec * SENSOR_CONVERT_UNIT; /* unit nanosecond */

    int32_t ret = ReadSensor(&data->busCfg, SHT30_HUM_DATA_ADDR, value, sizeof(value));
    CHECK_PARSER_RESULT_RETURN_VALUE(ret, "read data");

    tempValue = value[SHT30_HUM_VALUE_INDEX_THREE];
    tempValue <<= SENSOR_DATA_WIDTH_8_BIT;
    tempValue |= value[SHT30_HUM_VALUE_INDEX_FOUR];

    rawData->humidity = ((SHT30_HUM_SLOPE *  tempValue) / SHT30_HUM_RESOLUTION);

    if (value[SHT30_HUM_VALUE_INDEX_FIVE] != \
        Sht30CalcCrc8(value + SHT30_HUM_VALUE_INDEX_THREE, SHT30_HUM_CRC8_LEN)) {
        HDF_LOGE("%s: Calc humidity crc8 failed!", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t ReadSht30Data(struct SensorCfgData *data)
{
    int32_t ret;
    static int32_t humidity;
    struct HumidityData rawData = { 0 };
    OsalTimespec time;
    struct SensorReportEvent event;

    (void)memset_s(&time, sizeof(time), 0, sizeof(time));
    (void)memset_s(&event, sizeof(event), 0, sizeof(event));

    if (OsalGetTime(&time) != HDF_SUCCESS) {
        HDF_LOGE("%s: Get time failed", __func__);
        return HDF_FAILURE;
    }

    event.timestamp = time.sec * SENSOR_SECOND_CONVERT_NANOSECOND + time.usec * SENSOR_CONVERT_UNIT;

    ret = ReadSht30RawData(data, &rawData, &event.timestamp);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: SHT30 read raw data failed", __func__);
        return HDF_FAILURE;
    }

    humidity = rawData.humidity;

    event.sensorId = SENSOR_TAG_HUMIDITY;
    event.mode = SENSOR_WORK_MODE_REALTIME;
    event.dataLen = sizeof(humidity);
    event.data = (uint8_t *)&humidity;
    ret = ReportSensorEvent(&event);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: report data failed", __func__);
    }

    return ret;
}

static int32_t InitSht30(struct SensorCfgData *data)
{
    int32_t ret;

    CHECK_NULL_PTR_RETURN_VALUE(data, HDF_ERR_INVALID_PARAM);
    ret = SetSensorRegCfgArray(&data->busCfg, data->regCfgGroup[SENSOR_INIT_GROUP]);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: sensor init config failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t DispatchSht30(struct HdfDeviceIoClient *client,
    int cmd, struct HdfSBuf *data, struct HdfSBuf *reply)
{
    (void)client;
    (void)cmd;
    (void)data;
    (void)reply;

    return HDF_SUCCESS;
}

static int32_t Sht30BindDriver(struct HdfDeviceObject *device)
{
    CHECK_NULL_PTR_RETURN_VALUE(device, HDF_ERR_INVALID_PARAM);

    struct Sht30DrvData *drvData = (struct Sht30DrvData *)OsalMemCalloc(sizeof(*drvData));
    if (drvData == NULL) {
        HDF_LOGE("%s: malloc drv data fail", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    drvData->ioService.Dispatch = DispatchSht30;
    drvData->device = device;
    device->service = &drvData->ioService;
    g_sht30DrvData = drvData;

    return HDF_SUCCESS;
}

static int32_t Sht30InitDriver(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct HumidityOpsCall ops;

    CHECK_NULL_PTR_RETURN_VALUE(device, HDF_ERR_INVALID_PARAM);
    struct Sht30DrvData *drvData = (struct Sht30DrvData *)device->service;
    CHECK_NULL_PTR_RETURN_VALUE(drvData, HDF_ERR_INVALID_PARAM);

    drvData->sensorCfg = HumidityCreateCfgData(device->property);
    if (drvData->sensorCfg == NULL || drvData->sensorCfg->root == NULL) {
        HDF_LOGE("%s: Creating humidity cfg failed because detection failed", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ops.Init = NULL;
    ops.ReadData = ReadSht30Data;
    ret = HumidityRegisterChipOps(&ops);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: Register humidity failed", __func__);
        return HDF_FAILURE;
    }

    ret = InitSht30(drvData->sensorCfg);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: Init SHT30 humidity sensor failed", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void Sht30ReleaseDriver(struct HdfDeviceObject *device)
{
    CHECK_NULL_PTR_RETURN(device);

    struct Sht30DrvData *drvData = (struct Sht30DrvData *)device->service;
    CHECK_NULL_PTR_RETURN(drvData);

    if (drvData->sensorCfg != NULL) {
        HumidityReleaseCfgData(drvData->sensorCfg);
        drvData->sensorCfg = NULL;
    }
    OsalMemFree(drvData);
}

struct HdfDriverEntry g_humiditySht30DevEntry = {
    .moduleVersion = 1,
    .moduleName = "HDF_SENSOR_HUMIDITY_SHT30",
    .Bind = Sht30BindDriver,
    .Init = Sht30InitDriver,
    .Release = Sht30ReleaseDriver,
};

HDF_INIT(g_humiditySht30DevEntry);
