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

#include "secure_element_ca.h"

#define HDF_LOG_TAG hdf_se

namespace OHOS {
namespace HDI {
namespace SecureElement {
static sptr<ISecureElementCallback> g_callbackV1_0 = nullptr;
static const int RES_BUFFER_MAX_LENGTH = 512;

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
    
    int ret = HuaweiSecureElementCaInit();
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        HDF_LOGE("getAtr failed ret %u", ret);
        status = SecureElementStatus::SE_GENERAL_ERROR;
        return HDF_ERR_INVALID_PARAM;
    }
    g_callbackV1_0 = clientCallback;
    g_callbackV1_0->OnSeStateChanged(true);
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::getAtr(std::vector<uint8_t>& response)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = HuaweiSecureElementCaGetAtr(res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        HDF_LOGE("getAtr failed ret %u", ret);
    }
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
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = HuaweiSecureElementCaOpenLogicalChannel((uint8_t *)&aid[0], aid.size(), p2, res, &resLen,
        (uint32_t *)&channelNumber);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("openLogicalChannel failed ret %u", ret);
    }
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
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = HuaweiSecureElementCaOpenBasicChannel((uint8_t *)&aid[0], aid.size(), res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("openBasicChannel failed ret %u", ret);
    }
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::closeChannel(uint8_t channelNumber, SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    int ret = HuaweiSecureElementCaCloseChannel(channelNumber);
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("closeChannel failed ret %u", ret);
    }
    status = SecureElementStatus::SE_SUCCESS;
    return HDF_SUCCESS;
}

int32_t SeVendorAdaptions::transmit(const std::vector<uint8_t>& command, std::vector<uint8_t>& response,
    SecureElementStatus& status)
{
    HDF_LOGI("SeVendorAdaptions:%{public}s!", __func__);
    uint8_t res[RES_BUFFER_MAX_LENGTH] = {0};
    uint32_t resLen = RES_BUFFER_MAX_LENGTH;
    int ret = HuaweiSecureElementCaTransmit((uint8_t *)&command[0], command.size(), res, &resLen);
    for (uint32_t i = 0; i < resLen; i++) {
        response.push_back(res[i]);
    }
    if (ret != SECURE_ELEMENT_CA_RET_OK) {
        status = SecureElementStatus::SE_GENERAL_ERROR;
        HDF_LOGE("transmit failed ret %u", ret);
    }
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