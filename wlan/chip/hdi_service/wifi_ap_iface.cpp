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

#include "wifi_ap_iface.h"
#include <hdf_log.h>

namespace OHOS {
namespace HDI {
namespace Wlan {
namespace Chip {
namespace V2_0 {
WifiApIface::WifiApIface(
    const std::string& ifname, const std::vector<std::string>& instances,
    const std::weak_ptr<WifiVendorHal> vendorHal,
    const std::weak_ptr<IfaceUtil> ifaceUtil)
    : ifname_(ifname),
      instances_(instances),
      vendorHal_(vendorHal),
      ifaceUtil_(ifaceUtil),
      isValid_(true) {}

void WifiApIface::Invalidate()
{
    vendorHal_.reset();
    isValid_ = false;
}

bool WifiApIface::IsValid()
{
    return isValid_;
}

std::string WifiApIface::GetName()
{
    return ifname_;
}

void WifiApIface::RemoveInstance(std::string instance)
{
    instances_.erase(std::remove(instances_.begin(), instances_.end(), instance), instances_.end());
}

int32_t WifiApIface::GetIfaceType(IfaceType& type)
{
    type = IfaceType::AP;
    return HDF_SUCCESS;
}

int32_t WifiApIface::GetIfaceName(std::string& name)
{
    name = ifname_;
    return HDF_SUCCESS;
}

int32_t WifiApIface::GetSupportFreqs(int band, std::vector<uint32_t>& frequencies)
{
    WifiError status;
    std::vector<uint32_t> validFrequencies;
    std::tie(status, validFrequencies) = vendorHal_.lock()->GetValidFrequenciesForBand(
        instances_.size() > 0 ? instances_[0] : ifname_, band);
    frequencies = validFrequencies;
    if (status == HAL_SUCCESS) {
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t WifiApIface::GetIfaceCap(uint32_t& capabilities)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::SetMacAddress(const std::string& mac)
{
    bool status = ifaceUtil_.lock()->SetMacAddress(ifname_, mac);
    if (!status) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t WifiApIface::SetCountryCode(const std::string& code)
{
    WifiError status = vendorHal_.lock()->SetCountryCode(ifname_, code);
    if (status != HAL_SUCCESS) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t WifiApIface::GetPowerMode(int32_t& powerMode)
{
    WifiError status;
    int mode;

    std::tie(status, mode) = vendorHal_.lock()->GetPowerMode(ifname_);
    if (status == HAL_SUCCESS) {
        powerMode = mode;
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t WifiApIface::SetPowerMode(int32_t powerMode)
{
    WifiError status = vendorHal_.lock()->SetPowerMode(ifname_, powerMode);
    if (status == HAL_SUCCESS) {
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t WifiApIface::RegisterChipIfaceCallBack(const sptr<IChipIfaceCallback>& chipIfaceCallback)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::UnRegisterChipIfaceCallBack(const sptr<IChipIfaceCallback>& chipIfaceCallback)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::StartScan(const ScanParams& scanParam)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::GetScanInfos(std::vector<ScanResultsInfo>& scanResultsInfo)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::StartPnoScan(const PnoScanParams& pnoParams)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::StopPnoScan()
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::GetSignalPollInfo(SignalPollResult& signalPollResult)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::EnablePowerMode(int32_t mode)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::SetDpiMarkRule(int32_t uid, int32_t protocol, int32_t enable)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::SetTxPower(int32_t power)
{
{
    WifiError status = vendorHal_.lock()->SetTxPower(ifname_, power);
    return status;
}
}

int32_t WifiApIface::SetIfaceState(bool state)
{
    if (ifaceUtil_.lock()->SetUpState(ifname_, state)) {
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t WifiApIface::SendCmdToDriver(const std::string& ifName, int32_t cmdId,
    const std::vector<int8_t>& paramBuf, std::vector<int8_t>& result)
{
    WifiError status = vendorHal_.lock()->SendCmdToDriver(ifName, cmdId, paramBuf, result);
    if (status == HAL_SUCCESS) {
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t WifiApIface::SendActionFrame(const std::string& ifName, uint32_t freq, const std::vector<uint8_t>& frameData)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::RegisterActionFrameReceiver(const std::string& ifName, const std::vector<uint8_t>& match)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::GetCoexictenceChannelList(const std::string& ifName, std::vector<uint8_t>& paramBuf)
{
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WifiApIface::SetProjectionScreenParam(const std::string& ifName, const ProjectionScreenCmdParam& param)
{
    return HDF_ERR_NOT_SUPPORT;
}
}
}
}
}
}