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
#include <securec.h>
#include <string.h>
#include "effect_core.h"
#include "hdf_log.h"
#include "osal_mem.h"
#include "parse_effect_config.h"

#define AUDIO_EFFECT_CONFIG  HDF_CHIP_PROD_CONFIG_DIR"/audio_effect.json"
#define HDF_LOG_TAG HDF_AUDIO_EFFECT
struct ConfigDescriptor *g_cfgDescs = NULL;

static int32_t EffectModelIsSupplyEffectLibs(struct IEffectModel *self, bool *supply)
{
    if (self == NULL || supply == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    *supply = IsEffectLibExist();
    return HDF_SUCCESS;
}

static int32_t EffectModelGetAllEffectDescriptors(struct IEffectModel *self,
                                                  struct EffectControllerDescriptor *descs, uint32_t *descsLen)
{
    HDF_LOGI("enter to %{public}s", __func__);
    int32_t ret;
    uint32_t i;
    uint32_t descNum = 0;
    struct EffectFactory *factLib = NULL;

    if (self == NULL || descs == NULL || descsLen == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (g_cfgDescs == NULL) {
        HDF_LOGE("%{public}s: point is null!", __func__);
        return HDF_FAILURE;
    }

    for (i = 0; i < g_cfgDescs->effectNum; i++) {
        factLib = GetEffectLibFromList(g_cfgDescs->effectCfgDescs[i].library);
        if (factLib == NULL) {
            HDF_LOGE("%{public}s: GetEffectLibFromList fail!", __func__);
            continue;
        }
        ret = factLib->GetDescriptor(factLib, g_cfgDescs->effectCfgDescs[i].effectId, &descs[descNum]);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: GetDescriptor fail!", __func__);
            continue;
        }
        descNum++;
    }
    *descsLen = descNum;

    HDF_LOGI("%{public}s success", __func__);
    return HDF_SUCCESS;
}

static int32_t EffectModelGetEffectDescriptor(struct IEffectModel *self, const char *uuid,
     struct EffectControllerDescriptor *desc)
{
    HDF_LOGI("enter to %{public}s", __func__);
    uint32_t i;
    struct EffectFactory *factLib = NULL;
    if (self == NULL || uuid == NULL || desc == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    for (i = 0; i < g_cfgDescs->effectNum; i++) {
        if (strcmp(uuid, g_cfgDescs->effectCfgDescs[i].effectId) != 0) {
            continue;
        }

        factLib = GetEffectLibFromList(g_cfgDescs->effectCfgDescs[i].library);
        if (factLib == NULL) {
            HDF_LOGE("%{public}s: GetEffectLibFromList fail!", __func__);
            return HDF_FAILURE;
        }

        if (factLib->GetDescriptor(factLib, uuid, desc) != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: GetDescriptor fail!", __func__);
            return HDF_FAILURE;
        }
        HDF_LOGI("%{public}s success", __func__);
        return HDF_SUCCESS;
    }

    HDF_LOGE("%{public}s fail!", __func__);
    return HDF_FAILURE;
}

static int32_t EffectModelCreateEffectController(struct IEffectModel *self, const struct EffectInfo *info,
     struct IEffectControl **contoller, struct ControllerId *contollerId)
{
    if (self == NULL || info == NULL || contoller == NULL || contollerId == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    struct EffectFactory *lib = NULL;
    struct ControllerManager *ctrlMgr = NULL;
    struct EffectControl *ctrlOps = NULL;

    lib = GetEffectLibFromList(info->libName);
    if (lib == NULL) {
        HDF_LOGE("%{public}s: not match any lib", __func__);
        return HDF_FAILURE;
    }

    if (lib->CreateController == NULL) {
        HDF_LOGE("%{public}s: lib has no create method", __func__);
        return HDF_FAILURE;
    }

    lib->CreateController(lib, info, &ctrlOps);
    if (ctrlOps == NULL) {
        HDF_LOGE("%{public}s: lib create controller failed.", __func__);
        return HDF_FAILURE;
    }

    /* ctrlMgr mark it and using it in release process */
    ctrlMgr = (struct ControllerManager *)OsalMemCalloc(sizeof(struct ControllerManager));
    if (ctrlMgr == NULL) {
        HDF_LOGE("%{public}s: malloc ControllerManager obj failed!", __func__);
        return HDF_FAILURE;
    }

    ctrlMgr->ctrlOps = ctrlOps;
    ctrlMgr->effectId = strdup(info->effectId);
    ctrlMgr->ctrlImpls.EffectProcess = EffectControlEffectProcess;
    ctrlMgr->ctrlImpls.SendCommand = EffectControlSendCommand;
    ctrlMgr->ctrlImpls.GetEffectDescriptor = EffectGetOwnDescriptor;
    *contoller = &ctrlMgr->ctrlImpls;
    if (RegisterControllerToList(ctrlMgr) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: register ctroller to list failed.", __func__);
        OsalMemFree(ctrlMgr);
        return HDF_FAILURE;
    }

    // free after send reply
    contollerId->libName = strdup(info->libName);
    contollerId->effectId = strdup(info->effectId);
    return HDF_SUCCESS;
}

int32_t EffectModelDestroyEffectController(struct IEffectModel *self, const struct ControllerId *contollerId)
{
    if (self == NULL || contollerId == NULL) {
        HDF_LOGE("%{public}s: invailid input params", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    struct EffectFactory *lib = NULL;
    struct ControllerManager *ctrlMgr = NULL;

    lib = GetEffectLibFromList(contollerId->libName);
    if (lib == NULL) {
        HDF_LOGE("%{public}s: not match any lib", __func__);
        return HDF_FAILURE;
    }

    ctrlMgr = GetControllerFromList(contollerId->effectId);
    if (ctrlMgr == NULL) {
        HDF_LOGE("%{public}s: controller manager not found", __func__);
        return HDF_FAILURE;
    }

    if (ctrlMgr->ctrlOps == NULL) {
        HDF_LOGE("%{public}s: controller has no options", __func__);
        OsalMemFree(ctrlMgr);
        ctrlMgr = NULL;
        return HDF_FAILURE;
    }

    if (ctrlMgr->effectId != NULL) {
        OsalMemFree(ctrlMgr->effectId);
        ctrlMgr->effectId = NULL;
    }

    /* call the lib destroy method，then free controller manager */
    lib->DestroyController(lib, ctrlMgr->ctrlOps);
    OsalMemFree(ctrlMgr);
    ctrlMgr = NULL;

    return HDF_SUCCESS;
}

static int32_t RegLibraryInstByName(char *libPath)
{
    struct EffectFactory *factLib = NULL;
    struct EffectFactory *(*GetFactoryLib)(void);
    void *libHandle = NULL;
    if (libPath == NULL) {
        HDF_LOGE("%{public}s: invalid input param", __func__);
        return HDF_FAILURE;
    }

    libHandle = dlopen(libPath, RTLD_LAZY);
    if (libHandle == NULL) {
        HDF_LOGE("%{public}s: open so failed, reason:%{public}s", __func__, dlerror());
        return HDF_FAILURE;
    }

    GetFactoryLib = dlsym(libHandle, "GetEffectoyFactoryLib");
    factLib = GetFactoryLib();
    if (factLib == NULL) {
        HDF_LOGE("%{public}s: get fact lib failed %{public}s", __func__, dlerror());
        dlclose(libHandle);
        return HDF_FAILURE;
    }

    if (RegisterEffectLibToList(libHandle, factLib) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: register lib to list failed", __func__);
        dlclose(libHandle);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t RegLibraryInst(struct LibraryConfigDescriptor **libCfgDescs, const uint32_t libNum)
{
    int32_t ret;
    uint32_t i;
    char path[PATH_MAX];
    char pathBuf[PATH_MAX];
    if (libCfgDescs == NULL || libNum == 0 || libNum > HDF_EFFECT_LIB_NUM_MAX) {
        HDF_LOGE("Invalid parameter!");
        return HDF_ERR_INVALID_PARAM;
    }

    for (i = 0; i < libNum; i++) {
#ifdef __aarch64__
ret = snprintf_s(path, PATH_MAX, PATH_MAX, "/vendor/lib64/%s.z.so", (*libCfgDescs)[i].libPath);
#else
ret = snprintf_s(path, PATH_MAX, PATH_MAX, "/vendor/lib/%s.z.so", (*libCfgDescs)[i].libPath);
#endif
        if (ret < 0) {
            HDF_LOGE("%{public}s: get libPath failed", __func__);
            continue;
        }

        if (realpath(path, pathBuf) == NULL) {
            HDF_LOGE("%{public}s: realpath is null! [%{public}d]", __func__, errno);
            continue;
        }

        if (RegLibraryInstByName(path) != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: regist library[%{private}s] failed", __func__, path);
        }
    }
    return HDF_SUCCESS;
}

void ModelInit()
{
    struct ConfigDescriptor *cfgDesc = NULL;
    if (AudioEffectGetConfigDescriptor(AUDIO_EFFECT_CONFIG, &cfgDesc) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: AudioEffectGetConfigDescriptor fail!", __func__);
        return;
    }

    if (cfgDesc == NULL || cfgDesc->effectCfgDescs == NULL || cfgDesc->libCfgDescs == NULL) {
        HDF_LOGE("cfgDesc is null!");
        return;
    }

    g_cfgDescs = cfgDesc;
    if (RegLibraryInst(&(cfgDesc->libCfgDescs), cfgDesc->libNum) != HDF_SUCCESS) {
        HDF_LOGE("%{public}s: RegLibraryInst failed", __func__);
        AudioEffectReleaseCfgDesc(cfgDesc);
        return;
    }

    HDF_LOGI("%{public}s end!", __func__);
}

struct IEffectModel *EffectModelImplGetInstance(void)
{
    struct EffectModelService *service = (struct EffectModelService *)OsalMemCalloc(sizeof(struct EffectModelService));
    if (service == NULL) {
        HDF_LOGE("%{public}s: malloc EffectModelService obj failed!", __func__);
        return NULL;
    }

    ModelInit();
    service->interface.IsSupplyEffectLibs = EffectModelIsSupplyEffectLibs;
    service->interface.GetAllEffectDescriptors = EffectModelGetAllEffectDescriptors;
    service->interface.CreateEffectController = EffectModelCreateEffectController;
    service->interface.DestroyEffectController = EffectModelDestroyEffectController;
    service->interface.GetEffectDescriptor = EffectModelGetEffectDescriptor;

    return &service->interface;
}

void EffectModelImplRelease(struct IEffectModel *instance)
{
    if (instance == NULL) {
        return;
    }

    AudioEffectReleaseCfgDesc(g_cfgDescs);
    ReleaseLibFromList();
    OsalMemFree(instance);
}