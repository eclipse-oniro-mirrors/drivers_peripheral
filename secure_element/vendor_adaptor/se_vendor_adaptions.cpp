/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include "se_vendor_adaptions.h"
#include <hdf_base.h>
#include <hdf_log.h>
#include <vector>

#ifdef SE_VENDOR_ADAPTION_USE_CA
#include "secure_element_ca_proxy.h"
#endif

#define HDF_LOG_TAG hdf_se

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif

#define LOG_DOMAIN 0xD000305

namespace OHOS {
namespace HDI {
namespace SecureElement {
static sptr<ISecureElementCallback> g_callbackV1_0 = nullptr;
#ifdef SE_VENDOR_ADAPTION_USE_CA
static const int RES_BUFFER_MAX_LENGTH = 512;
static const uint16_t SW1_OFFSET = 2;
static const uint16_t SW2_OFFSET = 1;
static const uint16_t MAX_CHANNEL_NUM = 4;
uint16_t g_openedChannelCount = 0;
bool g_openedChannels[MAX_CHANNEL_NUM] = {false, false, false, false};
#endif

SeVendorAdaptions::SeVendorAdaptions() {}

SeVendorAdaptions::~SeVendorAdaptions() {}

int32_t SeVendorAdaptions::init(const sptr<ISecureElementCallback>& clientCallback, SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    if (clientCallback == nullptr) {
        HDF_LOGE("init failed, clientCallback is null");
        status = SecureElementStatus::SE_NULL_POINTER_ERROR;
        return HDF_ERR_INVALID_PARAM;
    }
#ifdef SE_VENDOR_ADAPTION_USE_CA
    g_openedChannelCount = 0;
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaInit();
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        HDF_LOGE("VendorSecureElementCaInit failed ret %{public}u", ret);
        status = SecureElementStatus::SE_GENERAL_ERROR;
        return HDF_ERR_INVALID_PARAM;
    }
#endif
    g_callbackV1_0 = clientCallback;
    g_callbackV1_0->OnSeStateChanged(true);
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::getAtr(std::vector<uint8_t>& response)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
#ifdef SE_VENDOR_ADAPTION_USE_CA
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaGetAtr(res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        HDF_LOGE("getAtr failed ret %{public}u", ret);
    }
#endif
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::isSecureElementPresent(bool& present)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    if (g_callbackV1_0 == nullptr) {
        present = false;
    } else {
        present = true;
    }
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::openLogicalChannel(const std::vector<uint8_t>& aid, uint8_t p2,
    std::vector<uint8_t>& response, uint8_t& channelNumber, SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    if (aid.empty()) {
        HDF_LOGE("aid is null");
        status = SecureElementStatus::SE_ILLEGAL_PARAMETER_ERROR;
        return HDF_ERR_INVALID_PARAM;
    }
#ifdef SE_VENDOR_ADAPTION_USE_CA
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaOpenLogicalChannel(
        (uint8_t *)&aid[0], aid.size(), p2, res, &resLen, (uint32_t *)&channelNumber);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("openLogicalChannel failed ret %{public}u", ret);
        if (g_openedChannelCount == 0) {
            HDF_LOGI("openLogicalChannel: g_openedChannelCount = %{public}d, Uninit", g_openedChannelCount);
            SecureElementCaProxy::GetInstance().VendorSecureElementCaUninit();
        }
        return HDF_SUCCESS;
    }
    if (ret == SECURE_ELEMENT_CA_RET_OK && resLen >= SW1_OFFSET
        && channelNumber < MAX_CHANNEL_NUM - 1 && !g_openedChannels[channelNumber]) {
        if ((response[resLen - SW1_OFFSET] == 0x90 && response[resLen - SW2_OFFSET] == 0x00)
            || response[resLen - SW2_OFFSET] == 0x62 || response[resLen - SW2_OFFSET] == 0x63) {
            g_openedChannels[channelNumber] = true;
            g_openedChannelCount++;
        }
    }
#endif
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::openBasicChannel(const std::vector<uint8_t>& aid, uint8_t p2, std::vector<uint8_t>& response,
    SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    if (aid.empty()) {
        HDF_LOGE("aid is null");
        status = SecureElementStatus::SE_ILLEGAL_PARAMETER_ERROR;
        return HDF_ERR_INVALID_PARAM;
    }
#ifdef SE_VENDOR_ADAPTION_USE_CA
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaOpenBasicChannel(
        (uint8_t *)&aid[0], aid.size(), res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("openBasicChannel failed ret %{public}u", ret);
        if (g_openedChannelCount == 0) {
            HDF_LOGI("openBasicChannel failed: g_openedChannelCount = %{public}d, Uninit", g_openedChannelCount);
            SecureElementCaProxy::GetInstance().VendorSecureElementCaUninit();
        }
        return HDF_SUCCESS;
    }
    if (ret == SECURE_ELEMENT_CA_RET_OK && resLen >= SW1_OFFSET && !g_openedChannels[0]) {
        if (response[resLen - SW1_OFFSET] == 0x90 && response[resLen - SW2_OFFSET] == 0x00) {
            g_openedChannels[0] = true;
            g_openedChannelCount++;
        }
    }
#endif
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::closeChannel(uint8_t channelNumber, SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
#ifdef SE_VENDOR_ADAPTION_USE_CA
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaCloseChannel(channelNumber);
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("closeChannel failed ret %{public}u", ret);
        return HDF_SUCCESS;
    }
    HDF_LOGI("closeChannel: channelNumber = %{public}d", channelNumber);
    if (channelNumber < MAX_CHANNEL_NUM - 1 && g_openedChannels[channelNumber]) {
        g_openedChannels[channelNumber] = false;
        g_openedChannelCount--;
    }
    if (g_openedChannelCount == 0) {
        HDF_LOGI("closeChannel: g_openedChannelCount = %{public}d, Uninit", g_openedChannelCount);
        SecureElementCaProxy::GetInstance().VendorSecureElementCaUninit();
    }
#endif
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::transmit(const std::vector<uint8_t>& command, std::vector<uint8_t>& response,
    SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
#ifdef SE_VENDOR_ADAPTION_USE_CA
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = SecureElementCaProxy::GetInstance().VendorSecureElementCaTransmit(
        (uint8_t *)&command[0], command.size(), res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("transmit failed ret %{public}u", ret);
    }
#endif
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::reset(SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    HDF_LOGE("reset is not support");
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}
} // SecureElement
} // HDI
} // OHOS