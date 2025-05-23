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

#include "effect_host_common.h"

#include "hdf_base.h"
#include "v1_0/effect_types.h"
#include "v1_0/effect_types_vdi.h"
#include "v1_0/ieffect_control_vdi.h"
#include "audio_uhdf_log.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "effect_core.h"
#include "audio_dfx.h"
#include <inttypes.h>

#define HDF_LOG_TAG HDF_AUDIO_EFFECT

int32_t EffectControlEffectProcess(struct IEffectControl *self, const struct AudioEffectBuffer *input,
    struct AudioEffectBuffer *output)
{
    CHECK_TRUE_RETURN_RET_LOG(self == NULL || input == NULL || output == NULL, HDF_ERR_INVALID_PARAM,
        "%{public}s: invailid input params", __func__);

    struct ControllerManager *manager = (struct ControllerManager *)self;
    CHECK_TRUE_RETURN_RET_LOG(manager->ctrlOps == NULL || manager->ctrlOps->EffectProcess == NULL,
        HDF_FAILURE, "%{public}s: controller has no options", __func__);
    if (strcmp(manager->libName, "libmock_effect_lib") != 0) {
        output->frameCount = input->frameCount;
        output->datatag = input->datatag;
        output->rawDataLen = input->rawDataLen;
        output->rawData = (int8_t *)OsalMemCalloc(sizeof(int8_t) * output->rawDataLen);
        CHECK_TRUE_RETURN_RET_LOG(output->rawData == NULL, HDF_FAILURE,
            "%{public}s: OsalMemCalloc fail", __func__);
    }
    struct AudioEffectBufferVdi *inputVdi = (struct AudioEffectBufferVdi *)input;
    struct AudioEffectBufferVdi *outputVdi = (struct AudioEffectBufferVdi *)output;

    OsalTimespec start;
    OsalTimespec end;
    OsalTimespec diff;
    int res = OsalGetTime(&start);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalGetTime start failed.");

    HdfAudioStartTrace("Hdi:Audio:EffectProcess", 0);
    int32_t ret = manager->ctrlOps->EffectProcess(manager->ctrlOps, inputVdi, outputVdi);
    HdfAudioFinishTrace();

    res = OsalGetTime(&end);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalGetTime end failed.");

    res = OsalDiffTime(&start, &end, &diff);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalDiffTime failed.");

    HDF_LOGI("EffectProcess cost time %{public}" PRIu64 " us",
        (HDF_KILO_UNIT * HDF_KILO_UNIT) * diff.sec + diff.usec);

    CHECK_TRUE_RETURN_RET_LOG(ret != HDF_SUCCESS, ret,
        "AudioEffectProcess failed, ret=%{public}d", ret);

    output = (struct AudioEffectBuffer *)outputVdi;
    return ret;
}

int32_t EffectControlSendCommand(struct IEffectControl *self, enum EffectCommandTableIndex cmdId, const int8_t *cmdData,
    uint32_t cmdDataLen, int8_t *replyData, uint32_t *replyDataLen)
{
    if (self == NULL || cmdData == NULL || replyData == NULL || replyDataLen == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct ControllerManager *manager = (struct ControllerManager *)self;
    if (manager->ctrlOps == NULL || manager->ctrlOps->SendCommand == NULL) {
        HDF_LOGE("%{public}s: controller has no options", __func__);
        return HDF_FAILURE;
    }
    enum EffectCommandTableIndexVdi cmdIdVdi = (enum EffectCommandTableIndexVdi)cmdId;
    int32_t ret = manager->ctrlOps->SendCommand(manager->ctrlOps, cmdIdVdi, (void *)cmdData, cmdDataLen,
                                         (void *)replyData, replyDataLen);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("SendCommand failed, ret=%{public}d", ret);
        return ret;
    }
    return ret;
}

int32_t EffectGetOwnDescriptor(struct IEffectControl *self, struct EffectControllerDescriptor *desc)
{
    CHECK_TRUE_RETURN_RET_LOG(self == NULL || desc == NULL, HDF_ERR_INVALID_PARAM,
        "%{public}s: invailid input params", __func__);

    struct ControllerManager *manager = (struct ControllerManager *)self;
    CHECK_TRUE_RETURN_RET_LOG(manager->ctrlOps == NULL || manager->ctrlOps->GetEffectDescriptor == NULL,
        HDF_FAILURE, "%{public}s: controller has no options", __func__);

    struct EffectControllerDescriptorVdi *descVdi = (struct EffectControllerDescriptorVdi *)desc;
    CHECK_TRUE_RETURN_RET_LOG(ConstructDescriptor(descVdi) != HDF_SUCCESS, HDF_FAILURE,
        "%{public}s: ConstructDescriptor fail!", __func__);

    OsalTimespec start;
    OsalTimespec end;
    OsalTimespec diff;
    int res = OsalGetTime(&start);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalGetTime start failed.");

    HdfAudioStartTrace("Hdi:Audio:GetEffectDescriptor", 0);
    int32_t ret = manager->ctrlOps->GetEffectDescriptor(manager->ctrlOps, descVdi);
    HdfAudioFinishTrace();

    res = OsalGetTime(&end);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalGetTime end failed.");

    res = OsalDiffTime(&start, &end, &diff);
    CHECK_TRUE_RETURN_RET_LOG(HDF_SUCCESS != res, HDF_FAILURE, "OsalDiffTime failed.");

    HDF_LOGI("GetEffectDescriptor cost time %{public}" PRIu64 " us",
        (HDF_KILO_UNIT * HDF_KILO_UNIT) * diff.sec + diff.usec);

    CHECK_TRUE_RETURN_RET_LOG(ret != HDF_SUCCESS, ret,
        "EffectGetOwnDescriptor failed, ret=%{public}d", ret);

    desc = (struct EffectControllerDescriptor *)descVdi;
    return ret;
}

int32_t EffectControlEffectReverse(struct IEffectControl *self, const struct AudioEffectBuffer *input,
    struct AudioEffectBuffer *output)
{
    if (self == NULL || input == NULL || output == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct ControllerManager *manager = (struct ControllerManager *)self;
    if (manager->ctrlOps == NULL || manager->ctrlOps->EffectReverse == NULL) {
        HDF_LOGE("%{public}s: controller has no options", __func__);
        return HDF_FAILURE;
    }
    struct AudioEffectBufferVdi *inputVdi = (struct AudioEffectBufferVdi *)input;
    struct AudioEffectBufferVdi *outputVdi = (struct AudioEffectBufferVdi *)output;
    int32_t ret = manager->ctrlOps->EffectReverse(manager->ctrlOps, inputVdi, outputVdi);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("EffectReverse failed, ret=%{public}d", ret);
        return ret;
    }
    output = (struct AudioEffectBuffer *)outputVdi;
    return ret;
}
