/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef OHOS_HDI_SENSOR_V3_0_SENSOR_TEST_CALLBACK_IMPL_H
#define OHOS_HDI_SENSOR_V3_0_SENSOR_TEST_CALLBACK_IMPL_H

#include <atomic>
#include <mutex>
#include <vector>
#include <hdf_base.h>
#include "v3_0/isensor_callback.h"
#include "v3_0/isensor_plug_callback.h"

namespace OHOS {
namespace HDI {
namespace Sensor {
namespace V3_0 {

class SensorTestDataCallback : public V3_0::ISensorCallback {
public:
    SensorTestDataCallback() = default;
    virtual ~SensorTestDataCallback() = default;

    int32_t OnDataEvent(const HdfSensorEvents& event) override
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        receivedEvents_.push_back(event);
        eventCount_.fetch_add(1, std::memory_order_relaxed);
        lastTimestamp_ = event.timestamp;
        lastSensorType_ = event.deviceSensorInfo.sensorType;
        return HDF_SUCCESS;
    }

    int32_t OnDataEventAsync(const std::vector<HdfSensorEvents>& events) override
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        for (const auto& event : events) {
            receivedEvents_.push_back(event);
            eventCount_.fetch_add(1, std::memory_order_relaxed);
        }
        asyncEventCount_.fetch_add(static_cast<int32_t>(events.size()), std::memory_order_relaxed);
        return HDF_SUCCESS;
    }

    int32_t GetEventCount() const
    {
        return eventCount_.load(std::memory_order_relaxed);
    }

    int32_t GetAsyncEventCount() const
    {
        return asyncEventCount_.load(std::memory_order_relaxed);
    }

    int64_t GetLastTimestamp() const
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return lastTimestamp_;
    }

    int32_t GetLastSensorType() const
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return lastSensorType_;
    }

    std::vector<HdfSensorEvents> GetReceivedEvents() const
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return receivedEvents_;
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        receivedEvents_.clear();
        eventCount_.store(0, std::memory_order_relaxed);
        asyncEventCount_.store(0, std::memory_order_relaxed);
        lastTimestamp_ = 0;
        lastSensorType_ = 0;
    }

private:
    mutable std::mutex dataMutex_;
    std::vector<HdfSensorEvents> receivedEvents_;
    std::atomic<int32_t> eventCount_{0};
    std::atomic<int32_t> asyncEventCount_{0};
    int64_t lastTimestamp_ = 0;
    int32_t lastSensorType_ = 0;
};

class SensorTestPlugCallback : public V3_0::ISensorPlugCallback {
public:
    SensorTestPlugCallback() = default;
    virtual ~SensorTestPlugCallback() = default;

    int32_t OnSensorPlugEvent(const SensorPlugInfo& sensorPlugInfo) override
    {
        std::lock_guard<std::mutex> lock(plugMutex_);
        lastPlugInfo_ = sensorPlugInfo;
        plugEventCount_.fetch_add(1, std::memory_order_relaxed);
        lastDeviceName_ = sensorPlugInfo.deviceName;
        lastStatus_ = sensorPlugInfo.status;
        return HDF_SUCCESS;
    }

    int32_t GetPlugEventCount() const
    {
        return plugEventCount_.load(std::memory_order_relaxed);
    }

    std::string GetLastDeviceName() const
    {
        std::lock_guard<std::mutex> lock(plugMutex_);
        return lastDeviceName_;
    }

    int32_t GetLastStatus() const
    {
        std::lock_guard<std::mutex> lock(plugMutex_);
        return lastStatus_;
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(plugMutex_);
        plugEventCount_.store(0, std::memory_order_relaxed);
        lastDeviceName_.clear();
        lastStatus_ = 0;
    }

private:
    mutable std::mutex plugMutex_;
    std::atomic<int32_t> plugEventCount_{0};
    std::string lastDeviceName_;
    int32_t lastStatus_ = 0;
    SensorPlugInfo lastPlugInfo_;
};

class SensorTestFailureCallback : public V3_0::ISensorCallback {
public:
    SensorTestFailureCallback() = default;
    virtual ~SensorTestFailureCallback() = default;

    int32_t OnDataEvent(const HdfSensorEvents& event) override
    {
        (void)event;
        return HDF_FAILURE;
    }

    int32_t OnDataEventAsync(const std::vector<HdfSensorEvents>& events) override
    {
        (void)events;
        return HDF_FAILURE;
    }
};

} // V3_0
} // Sensor
} // HDI
} // OHOS

#endif // OHOS_HDI_SENSOR_V3_0_SENSOR_TEST_CALLBACK_IMPL_H
