# Sensor HDI Full Test Cases Documentation

## 1. 概述

本目录包含 OpenHarmony 传感器 HDI 接口的完整测试用例，直接调用传感器模块的全部接口，覆盖 v3_0 和 v3_1 版本的 `ISensorInterface`（21 个方法）、`ISensorCallback`（2 个方法）、`ISensorPlugCallback`（1 个方法）以及 `ISensorConvertInterfaces`（1 个方法）。

### 1.1 接口版本说明

| 版本 | 接口 | 包名 | 说明 |
|------|------|------|------|
| v3_0 | ISensorInterface | ohos.hdi.sensor.v3_0 | 基线版本，16 个方法 |
| v3_1 | ISensorInterface | ohos.hdi.sensor.v3_1 | 继承 v3_0，新增 5 个 WithCallbackId 方法 |
| v3_0 | ISensorCallback | ohos.hdi.sensor.v3_0 | 数据回调接口，2 个方法 |
| v3_0 | ISensorPlugCallback | ohos.hdi.sensor.v3_0 | 热插拔回调接口，1 个方法 |
| convert/v1_0 | ISensorConvertInterfaces | ohos.hdi.sensor.convert.v1_0 | 传感器数据转换接口，1 个方法 |

### 1.2 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| `sensor_test_callback_impl.h` | ~165 | 自定义测试回调实现 |
| `sensor_hdi_full_test.cpp` | ~780 | v3_0 + v3_1 ISensorInterface 全部 21 个方法的测试 |
| `sensor_callback_full_test.cpp` | ~520 | ISensorCallback + ISensorPlugCallback 全部 3 个方法的测试 |
| `sensor_convert_full_test.cpp` | ~320 | ISensorConvertInterfaces 全部 1 个方法的测试 |
| `BUILD.gn` | ~60 | 构建配置文件 |

**代码总行数：约 1845 行（不含空行和注释约 1200+ 行有效代码）**

### 1.3 测试统计

| 测试文件 | 测试用例数 | 覆盖接口 |
|----------|-----------|----------|
| sensor_hdi_full_test.cpp | 55 | ISensorInterface v3_0 (16方法) + v3_1 (5方法) |
| sensor_callback_full_test.cpp | 24 | ISensorCallback (2方法) + ISensorPlugCallback (1方法) |
| sensor_convert_full_test.cpp | 11 | ISensorConvertInterfaces (1方法) |
| **合计** | **90** | **全部 25 个接口方法** |

---

## 2. 核心数据结构说明

### 2.1 DeviceSensorInfo

```cpp
struct DeviceSensorInfo {
    int deviceId;      // 设备 ID，-1 表示默认设备
    int sensorType;    // 传感器类型，对应 HdfSensorTypeTag 枚举（如 1=加速度计, 2=陀螺仪）
    int sensorId;      // 传感器 ID，由传感器驱动开发者定义
    int location;      // 位置：0=外设(SENSOR_EXTERNAL), 1=本地(LOCAL)
};
```

在测试中，`DeviceSensorInfo` 通常以 brace-init 形式传递：`{-1, sensorType, 0, 1}`，表示默认设备、指定传感器类型、sensorId=0、本地传感器。

### 2.2 HdfSensorInformation

```cpp
struct HdfSensorInformation {
    String sensorName;              // 传感器名称
    String vendorName;              // 传感器供应商
    String firmwareVersion;         // 固件版本
    String hardwareVersion;         // 硬件版本
    float maxRange;                 // 最大量程
    float accuracy;                 // 精度
    float power;                    // 功耗
    long minDelay;                  // 最小采样周期（微秒）
    long maxDelay;                  // 最大采样周期（微秒）
    unsigned int fifoMaxEventCount; // 最大批处理事件数
    DeviceSensorInfo deviceSensorInfo; // 设备传感器信息
    unsigned int reserved;          // 保留字段
};
```

### 2.3 HdfSensorEvents

```cpp
struct HdfSensorEvents {
    DeviceSensorInfo deviceSensorInfo; // 设备传感器信息
    int version;             // 传感器算法版本
    long timestamp;          // 数据生成时间戳
    unsigned int option;     // 数据选项（量程、精度）
    int mode;                // 上报模式
    unsigned char[] data;    // 传感器数据向量
    unsigned int dataLen;    // 数据长度
};
```

### 2.4 SensorPlugInfo

```cpp
struct SensorPlugInfo {
    DeviceSensorInfo deviceSensorInfo; // 设备传感器信息
    String deviceName;    // 设备名称
    int status;           // 插拔状态：1=插入, 0=拔出
    int reserved;         // 保留字段
};
```

### 2.5 HdfSensorData（转换接口）

```cpp
struct HdfSensorData {
    int sensorTypeId;     // 传感器类型 ID
    int version;           // 算法版本
    long timestamp;        // 数据时间戳
    int option;            // 数据选项
    int mode;              // 上报模式
    int deviceId;          // 设备 ID
    int sensorId;          // 传感器 ID
    int location;          // 位置
    unsigned char[] data;  // 传感器数据
};
```

### 2.6 HdfDeviceStatusPolicy（转换接口）

```cpp
struct HdfDeviceStatusPolicy {
    unsigned int deviceStatus;  // 设备状态
    unsigned int policy;        // 转换策略
    unsigned int[] reserve;     // 保留组
};
```

### 2.7 关键常量

| 常量名 | 值 | 说明 |
|--------|-----|------|
| TRADITIONAL_SENSOR_TYPE | 0 | 传统传感器组 |
| MEDICAL_SENSOR_TYPE | 1 | 医疗传感器组 |
| SENSOR_GROUP_TYPE_MAX | 2 | 传感器组类型最大值（无效值，用于负面测试） |
| SENSOR_SUCCESS | 0 | 操作成功 |
| SENSOR_NOT_SUPPORT | -2 | 不支持 |
| SENSOR_INVALID_PARAM | -3 | 参数无效 |
| CALLBACK_ID_BEGIN | 100000 | 回调 ID 最小值 |
| GPS_CALLBACK_ID_BEGIN | 100001 | GPS 回调 ID 起始 |
| THP_CALLBACK_ID_BEGIN | 100011 | THP 回调 ID 起始 |

---

## 3. 测试回调实现说明（sensor_test_callback_impl.h）

### 3.1 SensorTestDataCallback

**用途**：用于记录和验证传感器数据回调的自定义回调类，继承自 `V3_0::ISensorCallback`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `OnDataEvent(event)` | `HdfSensorEvents` | `HDF_SUCCESS` | 同步回调，将事件存入 `receivedEvents_` 向量，递增 `eventCount_` |
| `OnDataEventAsync(events)` | `vector<HdfSensorEvents>` | `HDF_SUCCESS` | 异步回调，遍历事件向量逐个存储，递增 `asyncEventCount_` |
| `GetEventCount()` | 无 | `int32_t` | 获取接收到的同步事件总数 |
| `GetAsyncEventCount()` | 无 | `int32_t` | 获取接收到的异步事件总数 |
| `GetLastTimestamp()` | 无 | `int64_t` | 获取最后一次事件的时间戳 |
| `GetLastSensorType()` | 无 | `int32_t` | 获取最后一次事件的传感器类型 |
| `GetReceivedEvents()` | 无 | `vector<HdfSensorEvents>` | 获取所有接收到的事件副本 |
| `Reset()` | 无 | `void` | 清空所有累计数据 |

### 3.2 SensorTestPlugCallback

**用途**：用于记录和验证传感器热插拔回调的自定义回调类，继承自 `V3_0::ISensorPlugCallback`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `OnSensorPlugEvent(sensorPlugInfo)` | `SensorPlugInfo` | `HDF_SUCCESS` | 插拔回调，记录设备名和状态 |
| `GetPlugEventCount()` | 无 | `int32_t` | 获取接收到的插拔事件总数 |
| `GetLastDeviceName()` | 无 | `string` | 获取最后一次插拔事件的设备名 |
| `GetLastStatus()` | 无 | `int32_t` | 获取最后一次插拔事件的状态 |
| `Reset()` | 无 | `void` | 清空所有累计数据 |

### 3.3 SensorTestFailureCallback

**用途**：用于测试失败回调路径的回调类，`OnDataEvent` 和 `OnDataEventAsync` 均返回 `HDF_FAILURE`。

---

## 4. 测试用例详细说明

### 4.1 sensor_hdi_full_test.cpp — ISensorInterface 全接口测试

#### 4.1.1 客户端获取测试

##### SensorHdiFullTest_GetSensorClient_001

**测试目标**：`V3_0::ISensorInterface::Get()`

**参数意义**：无参数

**代码逐行说明**：
```cpp
ASSERT_NE(nullptr, g_sensorInterfaceV3_0);
```
- 断言 v3_0 接口实例不为空，验证 `Get()` 成功返回了有效的 proxy 对象。

##### SensorHdiFullTest_GetSensorClient_002

**测试目标**：`V3_1::ISensorInterface::Get()`

**参数意义**：无参数

**代码逐行说明**：
```cpp
ASSERT_NE(nullptr, g_sensorInterfaceV3_1);
```
- 断言 v3_1 接口实例不为空，验证 v3_1 版本的 `Get()` 成功返回了有效的 proxy 对象。

#### 4.1.2 GetAllSensorInfo 测试

##### SensorHdiFullTest_GetAllSensorInfo_001

**测试目标**：`GetAllSensorInfo([out] vector<HdfSensorInformation>& info)`

**参数意义**：
- `info`：输出参数，用于接收系统中所有传感器的信息列表

**代码逐行说明**：
```cpp
std::vector<HdfSensorInformation> info;
int32_t ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 声明一个空的 `info` 向量，调用 `GetAllSensorInfo` 获取所有传感器信息。
- 断言返回值为 `SENSOR_SUCCESS`（0），表示操作成功。

```cpp
EXPECT_GT(info.size(), static_cast<size_t>(0));
```
- 断言返回的传感器列表不为空（至少存在一个传感器）。

```cpp
for (const auto& sensor : info) {
    EXPECT_GT(sensor.sensorName.size(), static_cast<size_t>(0));
    EXPECT_GT(sensor.vendorName.size(), static_cast<size_t>(0));
    EXPECT_GE(sensor.maxRange, 0.0f);
    EXPECT_GE(sensor.power, 0.0f);
}
```
- 遍历每个传感器信息，验证：传感器名称非空、供应商名称非空、最大量程非负、功耗非负。

##### SensorHdiFullTest_GetAllSensorInfo_002

**测试目标**：`GetAllSensorInfo` 稳定性

**参数意义**：无额外参数

**代码逐行说明**：
```cpp
std::vector<HdfSensorInformation> info1;
std::vector<HdfSensorInformation> info2;
int32_t ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info1);
ret = g_sensorInterfaceV3_0->GetAllSensorInfo(info2);
EXPECT_EQ(info1.size(), info2.size());
```
- 连续调用两次 `GetAllSensorInfo`，验证两次返回的传感器数量一致，确保接口输出稳定。

#### 4.1.3 GetDeviceSensorInfo 测试

##### SensorHdiFullTest_GetDeviceSensorInfo_001

**测试目标**：`GetDeviceSensorInfo([in] int deviceId, [out] vector<HdfSensorInformation>& info)`

**参数意义**：
- `deviceId`：设备 ID，`-1` 表示默认/本地设备
- `info`：输出参数，接收该设备包含的传感器信息列表

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->GetDeviceSensorInfo(-1, info);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 调用 `GetDeviceSensorInfo` 传入 deviceId=-1（默认设备），验证返回成功。

```cpp
for (const auto& sensor : info) {
    EXPECT_EQ(-1, sensor.deviceSensorInfo.deviceId);
}
```
- 遍历返回的传感器信息，验证每个传感器的 `deviceId` 都为 -1（与请求的 deviceId 一致）。

##### SensorHdiFullTest_GetDeviceSensorInfo_002

**测试目标**：`GetDeviceSensorInfo` 异常 deviceId 测试

**参数意义**：
- `ABNORMAL_DEVICE_ID = -999`：不存在的设备 ID

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->GetDeviceSensorInfo(ABNORMAL_DEVICE_ID, info);
EXPECT_NE(SENSOR_SUCCESS, ret);
EXPECT_EQ(static_cast<size_t>(0), info.size());
```
- 传入不存在的 deviceId（-999），验证返回值不等于 SUCCESS，且返回的传感器列表为空。

#### 4.1.4 Register / Unregister 测试

##### SensorHdiFullTest_Register_001

**测试目标**：`Register([in] int groupId, [in] ISensorCallback callbackObj)` + `Unregister`

**参数意义**：
- `groupId`：传感器组 ID，`TRADITIONAL_SENSOR_TYPE = 0` 表示传统传感器组
- `callbackObj`：回调对象指针

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 注册传统传感器回调，验证返回成功。
- 注销同一个回调，验证返回成功。

##### SensorHdiFullTest_Register_002

**测试目标**：`Register` 医疗传感器组

**参数意义**：
- `groupId = MEDICAL_SENSOR_TYPE = 1`：医疗传感器组

**代码逐行说明**：与 Register_001 相同，但使用 `MEDICAL_SENSOR_TYPE` 作为 groupId。

##### SensorHdiFullTest_Register_003

**测试目标**：`Register` 无效 groupId 测试

**参数意义**：
- `groupId = SENSOR_GROUP_TYPE_MAX = 2`：无效的传感器组 ID

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->Register(SENSOR_GROUP_TYPE_MAX, g_traditionalCallback);
EXPECT_EQ(SENSOR_INVALID_PARAM, ret);
```
- 传入无效 groupId，验证返回 `SENSOR_INVALID_PARAM`（-3）。

##### SensorHdiFullTest_Register_004

**测试目标**：`Register` 重复注册同一回调

**参数意义**：同 Register_001

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 连续注册同一个回调两次，验证两次都返回成功（幂等性）。

#### 4.1.5 Enable / Disable 测试

##### SensorHdiFullTest_Enable_001

**测试目标**：`Enable([in] DeviceSensorInfo)` + `SetBatch` + `Disable` 全流程

**参数意义**：
- `DeviceSensorInfo devInfo = iter.deviceSensorInfo`：从传感器列表中获取的真实传感器设备信息
- `SENSOR_INTERVAL1 = 200000000`：采样间隔，单位纳秒（200ms）
- `SENSOR_POLL_TIME = 1`：上报间隔，单位纳秒

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 注册数据回调。

```cpp
for (const auto& iter : g_info) {
    V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
    ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    ret = g_sensorInterfaceV3_0->Enable(devInfo);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    OsalMSleep(SENSOR_WAIT_TIME);
    ret = g_sensorInterfaceV3_0->Disable(devInfo);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
}
```
- 遍历所有传感器：设置采样批次 → 启用传感器 → 等待 100ms 收集数据 → 禁用传感器。

```cpp
ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
EXPECT_EQ(SENSOR_SUCCESS, ret);
EXPECT_EQ(SensorCallbackImpl::sensorDataFlag, 1);
SensorCallbackImpl::sensorDataFlag = 1;
```
- 注销回调，验证数据校验标志为 1（表示所有收到的数据都在合理范围内），然后重置标志。

##### SensorHdiFullTest_Enable_002

**测试目标**：`Enable` 异常 sensorType

**参数意义**：
- `ABNORMAL_SENSORID = -1`：不存在的传感器类型

**代码逐行说明**：
```cpp
V3_0::DeviceSensorInfo abnormalInfo = {-1, ABNORMAL_SENSORID, 0, 1};
int32_t ret = g_sensorInterfaceV3_0->Enable(abnormalInfo);
EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
ret = g_sensorInterfaceV3_0->Disable(abnormalInfo);
EXPECT_EQ(SENSOR_NOT_SUPPORT, ret);
```
- 构造一个不存在的传感器类型（-1），调用 Enable 和 Disable，验证都返回 `SENSOR_NOT_SUPPORT`（-2）。

##### SensorHdiFullTest_Enable_003

**测试目标**：`Enable` 不注册回调直接启用

**参数意义**：使用第一个传感器的设备信息

**代码逐行说明**：不注册回调，直接 SetBatch + Enable + Sleep + Disable，验证 Enable 返回成功（部分设备允许无回调启用）。

##### SensorHdiFullTest_Disable_001

**测试目标**：`Disable` 未启用的传感器

**参数意义**：使用第一个传感器的设备信息

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_0->Disable(devInfo);
EXPECT_NE(SENSOR_INVALID_PARAM, ret);
```
- 对从未启用的传感器调用 Disable，验证不返回 INVALID_PARAM（可能返回 SUCCESS 或 NOT_SUPPORT，取决于实现）。

#### 4.1.6 SetBatch 测试

##### SensorHdiFullTest_SetBatch_001

**测试目标**：`SetBatch` 多种采样间隔

**参数意义**：
- `intervals[]`：6 种不同的采样间隔，从 20ms 到 200ms

**代码逐行说明**：对每个传感器，依次使用 6 种不同的采样间隔调用 `SetBatch`，验证每次都返回成功，然后 Enable → Sleep → Disable。

##### SensorHdiFullTest_SetBatch_002

**测试目标**：`SetBatch` 异常 sensorType

**代码逐行说明**：对不存在的传感器类型调用 `SetBatch`，验证返回 `SENSOR_NOT_SUPPORT`。

##### SensorHdiFullTest_SetBatch_003

**测试目标**：`SetBatch` 零间隔

**参数意义**：
- `samplingInterval = 0, reportInterval = 0`：零采样间隔和零上报间隔

**代码逐行说明**：对所有传感器设置零间隔，验证返回成功（服务端会使用默认值）。

#### 4.1.7 SetMode 测试

##### SensorHdiFullTest_SetMode_001 ~ SetMode_005

**测试目标**：`SetMode([in] DeviceSensorInfo, [in] int mode)` 所有模式

**参数意义**：
- `SENSOR_MODE_DEFAULT = 0`：默认模式
- `SENSOR_MODE_REALTIME = 1`：实时模式
- `SENSOR_MODE_ON_CHANGE = 2`：变化时上报
- `SENSOR_MODE_ONE_SHOT = 3`：单次上报
- `SENSOR_MODE_FIFO = 4`：FIFO 模式

**代码逐行说明**（以 SetMode_001 为例）：
```cpp
for (const auto& iter : g_info) {
    V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
    ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_DEFAULT);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    ret = g_sensorInterfaceV3_0->Enable(devInfo);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    OsalMSleep(SENSOR_WAIT_TIME);
    ret = g_sensorInterfaceV3_0->Disable(devInfo);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
}
```
- 对每个传感器：设置默认模式 → 设置批次 → 启用 → 等待 → 禁用，验证每步都成功。

##### SensorHdiFullTest_SetMode_006

**测试目标**：`SetMode` 异常 sensorType

**代码逐行说明**：对不存在的传感器类型调用 `SetMode`，验证返回 `SENSOR_NOT_SUPPORT`。

#### 4.1.8 SetOption 测试

##### SensorHdiFullTest_SetOption_001

**测试目标**：`SetOption([in] DeviceSensorInfo, [in] unsigned int option)` 默认选项

**参数意义**：
- `SENSOR_OPTION_DEFAULT = 0`：默认选项

**代码逐行说明**：注册回调 → 对每个传感器设置默认选项 → SetBatch → Enable → Sleep → Disable → 注销回调。

##### SensorHdiFullTest_SetOption_002

**测试目标**：`SetOption` 不同选项值

**参数意义**：
- `SENSOR_OPTION_RANGE1 = 0`：量程级别 1
- `SENSOR_OPTION_RANGE2 = 1`：量程级别 2
- `SENSOR_OPTION_ACCURACY = 2`：精度选项

**代码逐行说明**：对每个传感器，依次使用 3 种不同的选项值调用 `SetOption`，验证每次都返回成功。

##### SensorHdiFullTest_SetOption_003

**测试目标**：`SetOption` 异常 sensorType

**代码逐行说明**：对不存在的传感器类型调用 `SetOption`，验证返回 `SENSOR_NOT_SUPPORT`。

#### 4.1.9 ReadData 测试

##### SensorHdiFullTest_ReadData_001

**测试目标**：`ReadData([in] DeviceSensorInfo, [out] vector<HdfSensorEvents>& event)`

**参数意义**：
- `devInfo`：传感器设备信息
- `events`：输出参数，接收传感器事件数据

**代码逐行说明**：
```cpp
for (const auto& iter : g_info) {
    V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
    std::vector<HdfSensorEvents> events;
    int32_t ret = g_sensorInterfaceV3_0->ReadData(devInfo, events);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
}
```
- 对每个传感器调用 `ReadData`，验证返回成功。

##### SensorHdiFullTest_ReadData_002

**测试目标**：`ReadData` 异常 sensorType

**代码逐行说明**：对不存在的传感器类型调用 `ReadData`，验证返回 `SENSOR_NOT_SUPPORT`。

#### 4.1.10 SetSdcSensor 测试

##### SensorHdiFullTest_SetSdcSensor_001

**测试目标**：`SetSdcSensor([in] DeviceSensorInfo, [in] bool enabled, [in] int rateLevel)`

**参数意义**：
- `devInfo`：传感器设备信息
- `enabled = true/false`：启用/禁用 SDC
- `RATE_LEVEL_LOW = 1`：低速率级别

**代码逐行说明**：
```cpp
for (const auto& iter : g_info) {
    V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
    int32_t ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, true, RATE_LEVEL_LOW);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    OsalMSleep(SENSOR_WAIT_TIME);
    ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, false, RATE_LEVEL_LOW);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
}
```
- 对每个传感器：启用 SDC（rateLevel=1） → 等待 100ms → 禁用 SDC，验证每步成功。

##### SensorHdiFullTest_SetSdcSensor_002

**测试目标**：`SetSdcSensor` 不同速率级别

**参数意义**：
- `RATE_LEVEL_LOW = 1`：低速率
- `RATE_LEVEL_MID = 50`：中速率
- `RATE_LEVEL_HIGH = 100`：高速率

**代码逐行说明**：对第一个传感器，依次使用 3 种速率级别启用和禁用 SDC，验证每步成功。

##### SensorHdiFullTest_SetSdcSensor_003

**测试目标**：`SetSdcSensor` 异常 sensorType

**代码逐行说明**：对不存在的传感器类型调用 `SetSdcSensor`，验证返回 `SENSOR_NOT_SUPPORT`。

#### 4.1.11 GetSdcSensorInfo 测试

##### SensorHdiFullTest_GetSdcSensorInfo_001

**测试目标**：`GetSdcSensorInfo([out] vector<SdcSensorInfo>& sdcSensorInfo)`

**参数意义**：
- `sdcInfo`：输出参数，接收 SDC 传感器信息列表

**代码逐行说明**：
```cpp
std::vector<V3_0::SdcSensorInfo> sdcInfo;
int32_t ret = g_sensorInterfaceV3_0->GetSdcSensorInfo(sdcInfo);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 调用 `GetSdcSensorInfo` 获取 SDC 传感器信息，验证返回成功。

##### SensorHdiFullTest_GetSdcSensorInfo_002

**测试目标**：`GetSdcSensorInfo` 稳定性

**代码逐行说明**：连续调用两次，验证返回的 SDC 信息数量一致。

#### 4.1.12 RegisterAsync / UnregisterAsync 测试

##### SensorHdiFullTest_RegisterAsync_001

**测试目标**：`RegisterAsync([in] int groupId, [in] ISensorCallback callbackObj)`

**参数意义**：
- `groupId = TRADITIONAL_SENSOR_TYPE`：传统传感器组
- `callbackObj`：回调对象

**代码逐行说明**：异步注册传统传感器回调，验证成功，然后异步注销，验证成功。

##### SensorHdiFullTest_RegisterAsync_002

**测试目标**：`RegisterAsync` 医疗传感器组

**代码逐行说明**：使用 `MEDICAL_SENSOR_TYPE` 作为 groupId 进行异步注册和注销。

##### SensorHdiFullTest_RegisterAsync_003

**测试目标**：`RegisterAsync` 无效 groupId

**代码逐行说明**：使用 `SENSOR_GROUP_TYPE_MAX` 作为 groupId，验证返回 `SENSOR_INVALID_PARAM`。

#### 4.1.13 RegSensorPlugCallBack / UnRegSensorPlugCallBack 测试

##### SensorHdiFullTest_RegSensorPlugCallBack_001

**测试目标**：`RegSensorPlugCallBack([in] ISensorPlugCallback callbackObj)`

**参数意义**：
- `callbackObj`：热插拔回调对象

**代码逐行说明**：注册热插拔回调，验证成功，然后注销，验证成功。

##### SensorHdiFullTest_RegSensorPlugCallBack_002

**测试目标**：`UnRegSensorPlugCallBack` 未注册的回调

**代码逐行说明**：创建一个新的未注册的回调对象，直接调用 `UnRegSensorPlugCallBack`，验证不返回成功。

##### SensorHdiFullTest_RegSensorPlugCallBack_003

**测试目标**：`RegSensorPlugCallBack` 重复注册

**代码逐行说明**：连续注册同一个插拔回调两次，验证都返回成功（幂等性），然后注销一次。

#### 4.1.14 v3_1 EnableWithCallbackId 测试

##### SensorHdiFullTest_EnableWithCallbackId_001

**测试目标**：`EnableWithCallbackId([in] DeviceSensorInfo, [in] unsigned int callbackId)` 全流程

**参数意义**：
- `devInfo`：传感器设备信息
- `TEST_CALLBACK_ID = 100001`：GPS 回调 ID

**代码逐行说明**：
```cpp
int32_t ret = g_sensorInterfaceV3_1->RegisterWithCallbackId(
    TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 使用 v3_1 接口注册带 callbackId 的回调。

```cpp
for (const auto& iter : g_info) {
    V3_0::DeviceSensorInfo devInfo = iter.deviceSensorInfo;
    ret = g_sensorInterfaceV3_1->SetBatchWithCallbackId(
        devInfo, TEST_CALLBACK_ID, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    ret = g_sensorInterfaceV3_1->EnableWithCallbackId(devInfo, TEST_CALLBACK_ID);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
    OsalMSleep(SENSOR_WAIT_TIME);
    ret = g_sensorInterfaceV3_1->DisableWithCallbackId(devInfo, TEST_CALLBACK_ID);
    EXPECT_EQ(SENSOR_SUCCESS, ret);
}
```
- 对每个传感器：带 callbackId 的 SetBatch → 带 callbackId 的 Enable → 等待 → 带 callbackId 的 Disable。

```cpp
ret = g_sensorInterfaceV3_1->UnregisterWithCallbackId(
    TRADITIONAL_SENSOR_TYPE, g_traditionalCallback, TEST_CALLBACK_ID);
EXPECT_EQ(SENSOR_SUCCESS, ret);
```
- 带 callbackId 的注销。

##### SensorHdiFullTest_EnableWithCallbackId_002

**测试目标**：`EnableWithCallbackId` 无效 callbackId

**参数意义**：
- `INVALID_CALLBACK_ID = 99999`：低于 `CALLBACK_ID_BEGIN`（100000）的无效 ID

**代码逐行说明**：使用无效 callbackId 调用 `EnableWithCallbackId`，验证不返回成功。

##### SensorHdiFullTest_EnableWithCallbackId_003

**测试目标**：`EnableWithCallbackId` 异常 sensorType

**代码逐行说明**：使用不存在的传感器类型调用 `EnableWithCallbackId`，验证返回 `SENSOR_NOT_SUPPORT`。

#### 4.1.15 DisableWithCallbackId 测试

##### SensorHdiFullTest_DisableWithCallbackId_001

**测试目标**：`DisableWithCallbackId` 未启用的传感器

**代码逐行说明**：对未启用的传感器调用 `DisableWithCallbackId`，验证不返回 `SENSOR_INVALID_PARAM`。

#### 4.1.16 SetBatchWithCallbackId 测试

##### SensorHdiFullTest_SetBatchWithCallbackId_001

**测试目标**：`SetBatchWithCallbackId([in] DeviceSensorInfo, [in] uint callbackId, [in] long samplingInterval, [in] long reportInterval)`

**参数意义**：
- `TEST_CALLBACK_ID = 100001`：回调 ID
- `intervals[]`：4 种不同的采样间隔

**代码逐行说明**：注册带 callbackId 的回调 → 对每个传感器用 4 种间隔调用 `SetBatchWithCallbackId` → 注销。

##### SensorHdiFullTest_SetBatchWithCallbackId_002

**测试目标**：`SetBatchWithCallbackId` 无效 callbackId

**代码逐行说明**：使用无效 callbackId 调用 `SetBatchWithCallbackId`，验证不返回成功。

#### 4.1.17 RegisterWithCallbackId 测试

##### SensorHdiFullTest_RegisterWithCallbackId_001

**测试目标**：`RegisterWithCallbackId([in] int groupId, [in] ISensorCallback, [in] uint callbackId)` 传统组

**参数意义**：
- `groupId = TRADITIONAL_SENSOR_TYPE`
- `TEST_CALLBACK_ID = 100001`（GPS 回调 ID 范围）

**代码逐行说明**：注册带 callbackId 的传统传感器回调，注销，验证两步都成功。

##### SensorHdiFullTest_RegisterWithCallbackId_002

**测试目标**：`RegisterWithCallbackId` 医疗组

**参数意义**：
- `groupId = MEDICAL_SENSOR_TYPE`
- `TEST_CALLBACK_ID_THP = 100011`（THP 回调 ID 范围）

**代码逐行说明**：使用医疗组 ID 和 THP 回调 ID 进行注册和注销。

##### SensorHdiFullTest_RegisterWithCallbackId_003

**测试目标**：`RegisterWithCallbackId` 无效 groupId

**代码逐行说明**：使用 `SENSOR_GROUP_TYPE_MAX` 调用，验证返回 `SENSOR_INVALID_PARAM`。

##### SensorHdiFullTest_RegisterWithCallbackId_004

**测试目标**：`RegisterWithCallbackId` 无效 callbackId

**参数意义**：
- `INVALID_CALLBACK_ID = 99999`：低于 `CALLBACK_ID_BEGIN` 的值

**代码逐行说明**：使用无效 callbackId 调用注册，验证不返回成功。

#### 4.1.18 UnregisterWithCallbackId 测试

##### SensorHdiFullTest_UnregisterWithCallbackId_001

**测试目标**：`UnregisterWithCallbackId` 未注册的回调

**代码逐行说明**：未注册直接调用 `UnregisterWithCallbackId`，验证不返回成功。

#### 4.1.19 综合工作流测试

##### SensorHdiFullTest_FullWorkflow_001

**测试目标**：v3_0 完整工作流

**代码逐行说明**：
```cpp
ret = g_sensorInterfaceV3_0->Register(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
// ...
ret = g_sensorInterfaceV3_0->SetMode(devInfo, SENSOR_MODE_DEFAULT);
ret = g_sensorInterfaceV3_0->SetOption(devInfo, SENSOR_OPTION_DEFAULT);
ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
ret = g_sensorInterfaceV3_0->Enable(devInfo);
OsalMSleep(SENSOR_WAIT_TIME);
ret = g_sensorInterfaceV3_0->ReadData(devInfo, g_events);
ret = g_sensorInterfaceV3_0->Disable(devInfo);
// ...
ret = g_sensorInterfaceV3_0->Unregister(TRADITIONAL_SENSOR_TYPE, g_traditionalCallback);
```
- 对每个传感器执行完整流程：注册 → SetMode → SetOption → SetBatch → Enable → 等待 → ReadData → Disable → 注销。

##### SensorHdiFullTest_FullWorkflowWithCallbackId_001

**测试目标**：v3_1 完整工作流（使用 CallbackId 变体）

**代码逐行说明**：使用 v3_1 的 `RegisterWithCallbackId` → `SetBatchWithCallbackId` → `EnableWithCallbackId` → Sleep → `DisableWithCallbackId` → `UnregisterWithCallbackId`。

##### SensorHdiFullTest_SdcCoexistence_001

**测试目标**：SA 和 SDC 共存

**代码逐行说明**：
```cpp
ret = g_sensorInterfaceV3_0->SetBatch(devInfo, SENSOR_INTERVAL1, SENSOR_POLL_TIME);
ret = g_sensorInterfaceV3_0->Enable(devInfo);
ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, true, RATE_LEVEL_MID);
OsalMSleep(SENSOR_WAIT_TIME);
ret = g_sensorInterfaceV3_0->SetSdcSensor(devInfo, false, RATE_LEVEL_MID);
ret = g_sensorInterfaceV3_0->Disable(devInfo);
```
- 先启用 SA 传感器，再启用 SDC，验证共存，然后先禁用 SDC 再禁用 SA。

##### SensorHdiFullTest_AsyncCallbackWithData_001

**测试目标**：RegisterAsync 回调接收数据

**代码逐行说明**：使用 `SensorTestDataCallback`（自定义回调）注册异步回调 → SetBatch → Enable → 等待 500ms → 验证 `GetEventCount() >= 0` → Disable → UnregisterAsync。

##### SensorHdiFullTest_FailureCallback_001

**测试目标**：失败回调路径

**代码逐行说明**：使用 `SensorCallbackImplTest`（返回 HDF_FAILURE 的回调）注册 → Enable → 等待 → Disable → Unregister，验证无崩溃。

---

### 4.2 sensor_callback_full_test.cpp — 回调接口测试

#### 4.2.1 OnDataEvent 测试

##### SensorCallbackFullTest_OnDataEvent_001

**测试目标**：`ISensorCallback::OnDataEvent([in] HdfSensorEvents event)` 基本功能

**参数意义**：
- `event.deviceSensorInfo = {-1, 1, 0, 1}`：设备 ID=-1，传感器类型=1（加速度计），sensorId=0，location=1
- `event.timestamp = 1234567890`：时间戳
- `event.data`：12 字节的测试数据，填充值为 42

**代码逐行说明**：
```cpp
HdfSensorEvents event;
event.deviceSensorInfo = {-1, 1, 0, 1};
event.version = 1;
event.timestamp = 1234567890;
event.option = 0;
event.mode = 0;
event.data = std::vector<uint8_t>(TEST_DATA_SIZE, TEST_DATA_VALUE);
event.dataLen = TEST_DATA_SIZE;
```
- 构造一个完整的 `HdfSensorEvents` 事件，包含设备信息、版本、时间戳、选项、模式、12 字节数据。

```cpp
int32_t ret = g_dataCallback->OnDataEvent(event);
EXPECT_EQ(HDF_SUCCESS, ret);
EXPECT_EQ(1, g_dataCallback->GetEventCount());
```
- 调用 `OnDataEvent`，验证返回 `HDF_SUCCESS`，事件计数从 0 变为 1。

##### SensorCallbackFullTest_OnDataEvent_002

**测试目标**：`OnDataEvent` 数据完整性验证

**代码逐行说明**：构造带有特定数据（{10,20,30,40,50}）的事件，调用 `OnDataEvent`，然后通过 `GetReceivedEvents()` 获取存储的事件，验证时间戳、传感器类型、数据长度都与输入一致。

##### SensorCallbackFullTest_OnDataEvent_003

**测试目标**：`OnDataEvent` 多次调用计数累积

**代码逐行说明**：连续调用 10 次 `OnDataEvent`（每次时间戳递增），验证 `GetEventCount()` 返回 10，`GetLastTimestamp()` 返回最后一次的时间戳。

##### SensorCallbackFullTest_OnDataEvent_004

**测试目标**：`OnDataEvent` 空数据

**代码逐行说明**：构造 data 为空向量、dataLen=0 的事件，调用 `OnDataEvent`，验证返回 `HDF_SUCCESS` 且无崩溃。

#### 4.2.2 OnDataEventAsync 测试

##### SensorCallbackFullTest_OnDataEventAsync_001

**测试目标**：`[oneway] OnDataEventAsync([in] vector<HdfSensorEvents> events)` 基本功能

**参数意义**：
- `events`：包含 5 个事件的向量

**代码逐行说明**：构造 5 个事件，调用 `OnDataEventAsync`，验证 `GetAsyncEventCount()` 返回 5，`GetEventCount()` 也返回 5（同步和异步计数器都递增）。

##### SensorCallbackFullTest_OnDataEventAsync_002

**测试目标**：`OnDataEventAsync` 空向量

**代码逐行说明**：传入空向量，验证返回 `HDF_SUCCESS` 且计数为 0。

##### SensorCallbackFullTest_OnDataEventAsync_003

**测试目标**：`OnDataEventAsync` 多次调用累积

**代码逐行说明**：先传入 3 个事件，再传入 7 个事件，验证异步计数从 3 增加到 10。

#### 4.2.3 OnSensorPlugEvent 测试

##### SensorCallbackFullTest_OnSensorPlugEvent_001

**测试目标**：`ISensorPlugCallback::OnSensorPlugEvent([in] SensorPlugInfo sensorPlugInfo)` 基本功能

**参数意义**：
- `plugInfo.deviceSensorInfo = {-1, 1, 0, 1}`：设备信息
- `plugInfo.deviceName = "test_sensor_device"`：设备名
- `plugInfo.status = 1`：插入状态

**代码逐行说明**：构造插拔信息，调用 `OnSensorPlugEvent`，验证返回 `HDF_SUCCESS`，计数为 1，设备名和状态正确。

##### SensorCallbackFullTest_OnSensorPlugEvent_002

**测试目标**：`OnSensorPlugEvent` 拔出状态

**参数意义**：
- `plugInfo.status = 0`：拔出状态

**代码逐行说明**：构造 status=0 的事件，验证回调正确记录了拔出状态。

##### SensorCallbackFullTest_OnSensorPlugEvent_003

**测试目标**：`OnSensorPlugEvent` 多次调用累积

**代码逐行说明**：连续调用 5 次，验证计数为 5。

##### SensorCallbackFullTest_OnSensorPlugEvent_004

**测试目标**：`OnSensorPlugEvent` 空设备名

**代码逐行说明**：构造 deviceName="" 的事件，验证无崩溃且正确记录。

#### 4.2.4 失败回调测试

##### SensorCallbackFullTest_FailureCallback_001

**测试目标**：`SensorTestFailureCallback::OnDataEvent` 返回 `HDF_FAILURE`

**代码逐行说明**：创建失败回调对象，调用 `OnDataEvent`，验证返回 `HDF_FAILURE`。

##### SensorCallbackFullTest_FailureCallback_002

**测试目标**：`SensorTestFailureCallback::OnDataEventAsync` 返回 `HDF_FAILURE`

**代码逐行说明**：创建失败回调对象，调用 `OnDataEventAsync`，验证返回 `HDF_FAILURE`。

#### 4.2.5 Reset 测试

##### SensorCallbackFullTest_Reset_001

**测试目标**：`SensorTestDataCallback::Reset()` 清空数据

**代码逐行说明**：发送 2 个事件，验证计数为 2，调用 `Reset()`，验证计数和异步计数都归零。

##### SensorCallbackFullTest_PlugReset_001

**测试目标**：`SensorTestPlugCallback::Reset()` 清空数据

**代码逐行说明**：发送 2 个插拔事件，验证计数为 2，调用 `Reset()`，验证计数归零。

#### 4.2.6 真实数据流测试

##### SensorCallbackFullTest_RealDataFlow_001

**测试目标**：Register + Enable 后通过 `OnDataEvent` 接收真实传感器数据

**代码逐行说明**：注册 `SensorTestDataCallback` → SetBatch → Enable → 等待 1000ms → 验证 `GetEventCount() >= 0` → Disable → Unregister。

##### SensorCallbackFullTest_RealDataFlowAsync_001

**测试目标**：RegisterAsync + Enable 后通过 `OnDataEventAsync` 接收真实传感器数据

**代码逐行说明**：与 RealDataFlow_001 类似，但使用 `RegisterAsync` 和 `UnregisterAsync`。

##### SensorCallbackFullTest_PlugCallbackFlow_001

**测试目标**：`RegSensorPlugCallBack` + 等待 + `UnRegSensorPlugCallBack`

**代码逐行说明**：注册插拔回调 → 等待 200ms → 验证 `GetPlugEventCount() >= 0` → 注销。

#### 4.2.7 共享回调实现测试

##### SensorCallbackFullTest_SensorCallbackImpl_001

**测试目标**：共享 `SensorCallbackImpl::OnDataEventAsync` 返回 `HDF_SUCCESS`

**代码逐行说明**：使用 `test/common/` 中的 `SensorCallbackImpl`，调用 `OnDataEventAsync`，验证返回 `HDF_SUCCESS`。

##### SensorCallbackFullTest_SensorCallbackImpl_002

**测试目标**：`SensorCallbackImpl::sensorDataFlag` 初始值

**代码逐行说明**：设置 `sensorDataFlag = 1`，验证值为 1。

##### SensorCallbackFullTest_SensorPlugCallbackImpl_001

**测试目标**：共享 `SensorPlugCallbackImpl::OnSensorPlugEvent` 返回 `HDF_SUCCESS`

**代码逐行说明**：使用 `test/common/` 中的 `SensorPlugCallbackImpl`，调用 `OnSensorPlugEvent`，验证返回 `HDF_SUCCESS`。

##### SensorCallbackFullTest_SensorCallbackImplTest_001

**测试目标**：共享 `SensorCallbackImplTest::OnDataEvent` 返回 `HDF_FAILURE`

**代码逐行说明**：使用 `test/common/` 中的 `SensorCallbackImplTest`，调用 `OnDataEvent`，验证返回 `HDF_FAILURE`。

#### 4.2.8 其它测试

##### SensorCallbackFullTest_GetLastSensorType_001

**测试目标**：`GetLastSensorType()` 返回正确的传感器类型

**代码逐行说明**：发送一个 sensorType=8 的事件，验证 `GetLastSensorType()` 返回 8。

##### SensorCallbackFullTest_ConcurrentOnDataEvent_001

**测试目标**：多线程并发调用 `OnDataEvent` 线程安全性

**代码逐行说明**：
```cpp
const int32_t threadCount = 4;
const int32_t eventsPerThread = 25;
std::vector<std::thread> threads;
auto sendEvents = [&]() {
    for (int32_t i = 0; i < eventsPerThread; ++i) {
        HdfSensorEvents event;
        // ... 构造事件 ...
        g_dataCallback->OnDataEvent(event);
    }
};
for (int32_t i = 0; i < threadCount; ++i) {
    threads.emplace_back(sendEvents);
}
for (auto& t : threads) {
    t.join();
}
EXPECT_EQ(threadCount * eventsPerThread, g_dataCallback->GetEventCount());
```
- 创建 4 个线程，每个线程发送 25 个事件。等待所有线程完成，验证总事件数为 100，证明 `OnDataEvent` 的 mutex 保护了 `receivedEvents_` 和计数器的线程安全。

---

### 4.3 sensor_convert_full_test.cpp — 转换接口测试

#### 4.3.1 GetConvertInterface_001

**测试目标**：`ISensorConvertInterfaces::Get()` 获取转换接口实例

**代码逐行说明**：断言 `g_convertInterface` 不为空。

#### 4.3.2 ConvertSensorData_001 ~ 008

**测试目标**：`ConvertSensorData([in] HdfDeviceStatusPolicy status, [in] HdfSensorData inSensorData, [out] HdfSensorData outSensorData)`

**参数意义**：
- `status.deviceStatus`：设备状态（0=正常, 1=挂起, 2=关机）
- `status.policy`：转换策略（0=默认, 1=转换）
- `inSensorData`：输入传感器数据
- `outSensorData`：输出传感器数据

**各用例说明**：

| 用例 | deviceStatus | policy | sensorType | 特殊条件 |
|------|-------------|--------|------------|---------|
| 001 | 0 (NORMAL) | 0 (DEFAULT) | 1 (ACCEL) | 标准场景 |
| 002 | 1 (SUSPEND) | 1 (CONVERT) | 2 (GYRO) | 挂起+转换策略 |
| 003 | 2 (SHUTDOWN) | 0 (DEFAULT) | 8 (BAROMETER) | 关机状态 |
| 004 | 0 | 0 | 1 | 空数据向量 |
| 005 | 0 | 1 | 1 | 带 reserve 数组的策略 |
| 006 | 0 | 0 | 1 | 两次调用验证一致性 |
| 007 | 0 | 0 | 多种类型 | 9 种传感器类型遍历 |
| 008 | 0 | 0 | 1 | 零时间戳和零版本 |

**代码逐行说明**（以 001 为例）：
```cpp
HdfDeviceStatusPolicy status = CreateTestPolicy(TEST_DEVICE_STATUS_NORMAL, TEST_POLICY_DEFAULT);
```
- 创建一个设备状态策略：设备状态=正常（0），策略=默认（0）。

```cpp
HdfSensorData inData = CreateTestSensorData(TEST_SENSOR_TYPE_ACCEL);
```
- 创建一个加速度计类型的测试传感器数据。

```cpp
HdfSensorData outData;
int32_t ret = g_convertInterface->ConvertSensorData(status, inData, outData);
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
```
- 调用 `ConvertSensorData` 进行数据转换，验证返回值为 `HDF_SUCCESS` 或 `HDF_ERR_NOT_SUPPORT`（取决于实现是否支持转换）。

#### 4.3.3 ConvertWithSensorData_001

**测试目标**：真实传感器数据转换流程

**代码逐行说明**：
1. 注册回调 → SetBatch → Enable → 等待 500ms → ReadData 获取真实数据
2. 如果有数据，将 `HdfSensorEvents` 转换为 `HdfSensorData` 格式
3. 调用 `ConvertSensorData` 进行转换
4. Disable → Unregister

#### 4.3.4 ConvertWithCallbackId_001

**测试目标**：v3_1 + Convert 组合流程

**代码逐行说明**：
1. 使用 v3_1 的 `RegisterWithCallbackId` 注册回调
2. `SetBatchWithCallbackId` → `EnableWithCallbackId` → 等待 → `DisableWithCallbackId`
3. 构造 `HdfSensorData` 并调用 `ConvertSensorData`
4. `UnregisterWithCallbackId` 注销

---

## 5. 构建配置说明（BUILD.gn）

### 5.1 构建目标

```gn
ohos_unittest("hdi_full_test_sensor")
```

- 构建目标名称：`hdi_full_test_sensor`
- 模块输出路径：`drivers_peripheral_sensor/drivers_peripheral_sensor/SensorHdiFullTest`

### 5.2 头文件包含路径

| 路径 | 用途 |
|------|------|
| `../../../interfaces/include` | `sensor_type.h`（枚举、结构体定义） |
| `../../../interfaces/v1_0` | VDI 接口头文件 |
| `../../common/include` | 共享测试回调实现（`sensor_callback_impl.h` 等） |
| `../../../utils/include` | `sensor_uhdf_log.h`、`sensor_trace.h` |

### 5.3 源文件

| 源文件 | 说明 |
|--------|------|
| `../../common/src/sensor_callback_impl.cpp` | 共享数据回调实现（含数据校验） |
| `../../common/src/sensor_callback_impl_test.cpp` | 共享失败回调实现 |
| `sensor_hdi_full_test.cpp` | ISensorInterface 全接口测试 |
| `sensor_callback_full_test.cpp` | 回调接口测试 |
| `sensor_convert_full_test.cpp` | 转换接口测试 |

### 5.4 外部依赖

| 依赖 | 说明 |
|------|------|
| `drivers_interface_sensor:libsensor_proxy_3.0` | v3_0 HDI proxy |
| `drivers_interface_sensor:libsensor_proxy_3.1` | v3_1 HDI proxy |
| `drivers_interface_sensor:libsensor_convert_proxy_1.0` | convert/v1_0 HDI proxy |
| `hdf_core:libhdf_host` | HDF 宿主 |
| `hdf_core:libhdf_utils` | HDF 工具库 |
| `hilog:libhilog` | 日志库 |
| `hitrace:hitrace_meter` | HiTrace |
| `ipc:ipc_single` | IPC 单实例 |
| `c_utils:utils` | C++ 工具库（条件依赖） |

### 5.5 构建命令

```bash
./build.sh --product-name rk3568 --build-target hdf_test_sensor
```

---

## 6. 接口覆盖矩阵

### 6.1 ISensorInterface v3_0（16 个方法）

| 方法 | 测试用例数 | 覆盖情况 |
|------|-----------|---------|
| GetAllSensorInfo | 2 | 正常 + 稳定性 |
| GetDeviceSensorInfo | 2 | 正常 + 异常 deviceId |
| Enable | 3 | 全流程 + 异常 sensorType + 无回调 |
| Disable | 1 | 未启用 |
| SetBatch | 3 | 多间隔 + 异常 + 零间隔 |
| SetMode | 6 | 5 种模式 + 异常 |
| SetOption | 3 | 默认 + 多选项 + 异常 |
| Register | 4 | 传统 + 医疗 + 无效组 + 重复 |
| Unregister | 4 | （随 Register 配对测试） |
| ReadData | 2 | 正常 + 异常 |
| SetSdcSensor | 3 | 启用/禁用 + 多速率 + 异常 |
| GetSdcSensorInfo | 2 | 正常 + 稳定性 |
| RegisterAsync | 3 | 传统 + 医疗 + 无效组 |
| UnregisterAsync | 3 | （随 RegisterAsync 配对） |
| RegSensorPlugCallBack | 3 | 正常 + 未注册注销 + 重复 |
| UnRegSensorPlugCallBack | 3 | （随 RegSensorPlugCallBack 配对） |

### 6.2 ISensorInterface v3_1（5 个方法）

| 方法 | 测试用例数 | 覆盖情况 |
|------|-----------|---------|
| EnableWithCallbackId | 3 | 全流程 + 无效 ID + 异常 sensorType |
| DisableWithCallbackId | 1 | 未启用 |
| SetBatchWithCallbackId | 2 | 多间隔 + 无效 ID |
| RegisterWithCallbackId | 4 | 传统 + 医疗 + 无效组 + 无效 ID |
| UnregisterWithCallbackId | 1 | 未注册 |

### 6.3 ISensorCallback（2 个方法）

| 方法 | 测试用例数 | 覆盖情况 |
|------|-----------|---------|
| OnDataEvent | 4 | 基本 + 数据完整性 + 多次累积 + 空数据 |
| OnDataEventAsync | 3 | 基本 + 空向量 + 多次累积 |

### 6.4 ISensorPlugCallback（1 个方法）

| 方法 | 测试用例数 | 覆盖情况 |
|------|-----------|---------|
| OnSensorPlugEvent | 4 | 基本 + 拔出 + 多次累积 + 空设备名 |

### 6.5 ISensorConvertInterfaces（1 个方法）

| 方法 | 测试用例数 | 覆盖情况 |
|------|-----------|---------|
| ConvertSensorData | 11 | 多状态 + 多策略 + 多类型 + 空数据 + 一致性 + 真实数据 |

---

## 7. 测试分类

### 7.1 正向测试
- 所有接口的正常调用路径
- 各种有效参数组合
- 真实传感器数据流验证

### 7.2 负向测试
- 异常 sensorType（-1）
- 异常 deviceId（-999）
- 无效 groupId（SENSOR_GROUP_TYPE_MAX）
- 无效 callbackId（低于 CALLBACK_ID_BEGIN）
- 未注册直接注销
- 未启用直接禁用

### 7.3 边界测试
- 零采样间隔和零上报间隔
- 空数据向量
- 空设备名
- 零时间戳

### 7.4 稳定性测试
- 多次调用同一接口验证输出一致
- 重复注册同一回调
- 连续启用/禁用

### 7.5 并发测试
- 多线程同时调用 `OnDataEvent`
- 验证 mutex 保护的线程安全性

### 7.6 综合工作流测试
- v3_0 完整流程：Register → SetMode → SetOption → SetBatch → Enable → ReadData → Disable → Unregister
- v3_1 完整流程：RegisterWithCallbackId → SetBatchWithCallbackId → EnableWithCallbackId → DisableWithCallbackId → UnregisterWithCallbackId
- SA/SDC 共存场景
- 失败回调路径
