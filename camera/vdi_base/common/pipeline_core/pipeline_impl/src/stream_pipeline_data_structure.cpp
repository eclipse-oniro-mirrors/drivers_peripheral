/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STREAM_PIPELINE_DATA_STRUCTURE_H
#define STREAM_PIPELINE_DATA_STRUCTURE_H

#include <dlfcn.h>
#include "stream_pipeline_data_structure.h"
#include "hdf_base.h"
#include "camera.h"
#include "params.h"
#include "config.h"
namespace OHOS::Camera {
#ifdef __ARCH64__
#define PIPELINE_CONFIG_LIB_PATH HDF_LIBRARY_DIR"64/libcamera_pipeline_config.z.so"
#else
#define PIPELINE_CONFIG_LIB_PATH HDF_LIBRARY_DIR"/libcamera_pipeline_config.z.so"
#endif

using HdfGetModuleConfigRootFunc = const struct HdfConfigRoot* (*)(void);
using HdfGetPipelineSpecsModuleConfigRootFunc = const struct HdfConfigPipelineSpecsRoot* (*)(void);

extern "C" void* GetHandle()
{
    static void* handle = ::dlopen(PIPELINE_CONFIG_LIB_PATH, RTLD_NOW);
    if (!handle) {
        char* errStr = dlerror();
        CAMERA_LOGE("dlopen failed, errStr: %{public}s", errStr);
        return nullptr;
    }
    return handle;
}

extern "C" const struct HdfConfigRoot* HdfGetModuleConfigRoot(void)
{
    static const struct HdfConfigRoot* pHdfConfigRoot = nullptr;
    static struct HdfConfigRoot defaultConfigRoot = {
        .module = nullptr,
        .streamInfo = nullptr,
        .streamInfoSize = 0,
        .sceneInfo = nullptr,
        .sceneInfoSize = 0,
    };
    if (pHdfConfigRoot == nullptr) {
        void* handle = GetHandle();
        if (!handle) {
            return &defaultConfigRoot;
        }
        dlerror();
        HdfGetModuleConfigRootFunc HdfGetModuleConfigRootF =
            reinterpret_cast<HdfGetModuleConfigRootFunc>(dlsym(handle, "HdfGetModuleConfigRoot"));
        char* errStr = dlerror();
        if (errStr) {
            CAMERA_LOGE("dlsym failed, errStr: %{public}s", errStr);
            return &defaultConfigRoot;
        }
        pHdfConfigRoot = HdfGetModuleConfigRootF();
        if (pHdfConfigRoot == nullptr) {
            return &defaultConfigRoot;
        }
    }

    return pHdfConfigRoot;
}

extern "C" const struct HdfConfigPipelineSpecsRoot* HdfGetPipelineSpecsModuleConfigRoot(void)
{
    static const struct HdfConfigPipelineSpecsRoot* pHdfConfigPipelineSpecsRoot = nullptr;
    static struct HdfConfigPipelineSpecsRoot defaultPipelineSpecsRoot = {
        .module = nullptr,
        .pipelineSpec = nullptr,
        .pipelineSpecSize = 0,
    };
    if (pHdfConfigPipelineSpecsRoot == nullptr) {
        void* handle = GetHandle();
        if (!handle) {
            return &defaultPipelineSpecsRoot;
        }
        HdfGetPipelineSpecsModuleConfigRootFunc HdfGetPipelineSpecsModuleConfigRootF =
            reinterpret_cast<HdfGetPipelineSpecsModuleConfigRootFunc>(
                dlsym(handle, "HdfGetPipelineSpecsModuleConfigRoot"));
        char* errStr = dlerror();
        if (errStr) {
            return &defaultPipelineSpecsRoot;
        }
        pHdfConfigPipelineSpecsRoot = HdfGetPipelineSpecsModuleConfigRootF();
        if (pHdfConfigPipelineSpecsRoot == nullptr) {
            return &defaultPipelineSpecsRoot;
        }
    }

    return pHdfConfigPipelineSpecsRoot;
}
}
#endif
