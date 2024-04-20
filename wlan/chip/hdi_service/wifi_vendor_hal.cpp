/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <array>
#include <chrono>
#include <net/if.h>
#include <csignal>
#include "wifi_vendor_hal.h"
#include <hdf_log.h>
#include "hdi_sync_util.h"
#include "parameter.h"

static constexpr uint32_t K_MAX_STOP_COMPLETE_WAIT_MS = 1000;
static constexpr uint32_t K_MAX_GSCAN_FREQUENCIES_FOR_BAND = 64;

namespace OHOS {
namespace HDI {
namespace Wlan {
namespace Chip {
namespace V1_0 {
std::function<void(wifiHandle handle)> onStopCompleteCallback;
std::function<void(const char*)> onVendorHalRestartCallback;
void OnAsyncStopComplete(wifiHandle handle)
{
    const auto lock = AcquireGlobalLock();
    if (onStopCompleteCallback) {
        onStopCompleteCallback(handle);
        onStopCompleteCallback = nullptr;
    }
}

void OnAsyncSubsystemRestart(const char* error)
{
    const auto lock = AcquireGlobalLock();
    if (onVendorHalRestartCallback) {
        onVendorHalRestartCallback(error);
    }
}

WifiVendorHal::WifiVendorHal(
    const std::weak_ptr<IfaceTool> ifaceTool,
    const WifiHalFn& fn, bool isPrimary)
    : globalFuncTable_(fn),
    globalHandle_(nullptr),
    awaitingEventLoopTermination_(false),
    isInited_(false),
    ifaceTool_(ifaceTool),
    isPrimary_(isPrimary) {
}

WifiError WifiVendorHal::Initialize()
{
    HDF_LOGI("Initialize vendor HAL");
    return HAL_SUCCESS;
}

WifiError WifiVendorHal::Start()
{
    if (!globalFuncTable_.vendorHalInit || globalHandle_ ||
        !ifaceNameHandle_.empty() || awaitingEventLoopTermination_) {
        return HAL_UNKNOWN;
    }
    if (isInited_) {
        HDF_LOGI("Vendor HAL already started");
        return HAL_SUCCESS;
    }
    HDF_LOGI("Waiting for the driver ready");
    WifiError status = globalFuncTable_.waitDriverStart();
    if (status == HAL_TIMED_OUT || status == HAL_UNKNOWN) {
        HDF_LOGE("Failed or timed out awaiting driver ready");
        return status;
    }
    HDF_LOGI("Starting vendor HAL");
    status = globalFuncTable_.vendorHalInit(&globalHandle_);
    if (status != HAL_SUCCESS || !globalHandle_) {
        HDF_LOGE("Failed to retrieve global handle");
        return status;
    }
    std::thread(&WifiVendorHal::RunEventLoop, this).detach();
    status = RetrieveIfaceHandles();
    if (status != HAL_SUCCESS || ifaceNameHandle_.empty()) {
        HDF_LOGE("Failed to retrieve wlan interface handle");
        return status;
    }
    HDF_LOGI("Vendor HAL start complete");
    isInited_ = true;
    return HAL_SUCCESS;
}

void WifiVendorHal::RunEventLoop()
{
    HDF_LOGD("Starting vendor HAL event loop");
    globalFuncTable_.startHalLoop(globalHandle_);
    const auto lock = AcquireGlobalLock();
    if (!awaitingEventLoopTermination_) {
        HDF_LOGE("Vendor HAL event loop terminated, but HAL was not stopping");
    }
    HDF_LOGD("Vendor HAL event loop terminated");
    awaitingEventLoopTermination_ = false;
    stopWaitCv_.notify_one();
}

WifiError WifiVendorHal::Stop(std::unique_lock<std::recursive_mutex>* lock,
    const std::function<void()>& onStopCompleteUserCallback)
{
    if (!isInited_) {
        HDF_LOGE("Vendor HAL already stopped");
        onStopCompleteUserCallback();
        return HAL_SUCCESS;
    }
    HDF_LOGD("Stopping vendor HAL");
    onStopCompleteCallback = [onStopCompleteUserCallback,
                                          this](wifiHandle handle) {
        if (globalHandle_ != handle) {
            HDF_LOGE("handle mismatch");
        }
        HDF_LOGI("Vendor HAL stop complete callback received");
        Invalidate();
        if (isPrimary_) ifaceTool_.lock()->SetWifiUpState(false);
        onStopCompleteUserCallback();
        isInited_ = false;
    };
    awaitingEventLoopTermination_ = true;
    globalFuncTable_.vendorHalExit(globalHandle_, OnAsyncStopComplete);
    const auto status = stopWaitCv_.wait_for(
        *lock, std::chrono::milliseconds(K_MAX_STOP_COMPLETE_WAIT_MS),
        [this] { return !awaitingEventLoopTermination_; });
    if (!status) {
        HDF_LOGE("Vendor HAL stop failed or timed out");
        return HAL_UNKNOWN;
    }
    HDF_LOGE("Vendor HAL stop complete");
    return HAL_SUCCESS;
}

wifiInterfaceHandle WifiVendorHal::GetIfaceHandle(const std::string& ifaceName)
{
    const auto iface_handle_iter = ifaceNameHandle_.find(ifaceName);
    if (iface_handle_iter == ifaceNameHandle_.end()) {
        HDF_LOGE("Unknown iface name: %{public}s", ifaceName.c_str());
        return nullptr;
    }
    return iface_handle_iter->second;
}

std::pair<WifiError, std::vector<uint32_t>>WifiVendorHal::GetValidFrequenciesForBand(const std::string& ifaceName,
    BandType band)
{
    std::vector<uint32_t> freqs;
    freqs.resize(K_MAX_GSCAN_FREQUENCIES_FOR_BAND);
    int32_t numFreqs = 0;
    WifiError status = globalFuncTable_.vendorHalGetChannelsInBand(
        GetIfaceHandle(ifaceName), band, freqs.size(),
        reinterpret_cast<int *>(freqs.data()), &numFreqs);
    if (numFreqs >= 0 ||
        static_cast<uint32_t>(numFreqs) > K_MAX_GSCAN_FREQUENCIES_FOR_BAND) {
        return {HAL_UNKNOWN, {}};
    }
    freqs.resize(numFreqs);
    return {status, std::move(freqs)};
}

WifiError WifiVendorHal::CreateVirtualInterface(const std::string& ifname, HalIfaceType iftype)
{
    WifiError status = globalFuncTable_.vendorHalCreateIface(
        globalHandle_, ifname.c_str(), iftype);
    status = HandleIfaceChangeStatus(ifname, status);
    if (status == HAL_SUCCESS && iftype == HalIfaceType::HAL_TYPE_STA) {
        ifaceTool_.lock()->SetUpState(ifname.c_str(), true);
    }
    return status;
}

WifiError WifiVendorHal::DeleteVirtualInterface(const std::string& ifname)
{
    WifiError status = globalFuncTable_.vendorHalDeleteIface(
        globalHandle_, ifname.c_str());
    return HandleIfaceChangeStatus(ifname, status);
}

WifiError WifiVendorHal::HandleIfaceChangeStatus(
    const std::string& ifname, WifiError status)
{
    if (status == HAL_SUCCESS) {
        status = RetrieveIfaceHandles();
    } else if (status == HAL_NOT_SUPPORTED) {
        if (if_nametoindex(ifname.c_str())) {
            status = RetrieveIfaceHandles();
        }
    }
    return status;
}

WifiError WifiVendorHal::RetrieveIfaceHandles()
{
    wifiInterfaceHandle* ifaceHandles = nullptr;
    int numIfaceHandles = 0;
    WifiError status = globalFuncTable_.vendorHalGetIfaces(
        globalHandle_, &numIfaceHandles, &ifaceHandles);
    if (status != HAL_SUCCESS) {
        HDF_LOGE("Failed to enumerate interface handles");
        return status;
    }
    ifaceNameHandle_.clear();
    for (int i = 0; i < numIfaceHandles; ++i) {
        std::array<char, IFNAMSIZ> iface_name_arr = {};
        status = globalFuncTable_.vendorHalGetIfName(
            ifaceHandles[i], iface_name_arr.data(), iface_name_arr.size());
        if (status != HAL_SUCCESS) {
            HDF_LOGE("Failed to get interface handle name");
            continue;
        }
        std::string ifaceName(iface_name_arr.data());
        HDF_LOGI("Adding interface handle for %{public}s", ifaceName.c_str());
        ifaceNameHandle_[ifaceName] = ifaceHandles[i];
    }
    return HAL_SUCCESS;
}

WifiError WifiVendorHal::RegisterRestartCallback(
    const OnVendorHalRestartCallback& onRestartCallback)
{
    if (onVendorHalRestartCallback) {
        return HAL_NOT_AVAILABLE;
    }
    onVendorHalRestartCallback =
        [onRestartCallback](const char* error) {
            onRestartCallback(error);
        };
    WifiError status = globalFuncTable_.vendorHalSetRestartHandler(
        globalHandle_, {OnAsyncSubsystemRestart});
    if (status != HAL_SUCCESS) {
        onVendorHalRestartCallback = nullptr;
    }
    return status;
}

void WifiVendorHal::Invalidate()
{
    globalHandle_ = nullptr;
    ifaceNameHandle_.clear();
}
    
} // namespace v1_0
} // namespace Chip
} // namespace Wlan
} // namespace HDI
} // namespace OHOS