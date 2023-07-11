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
namespace OHOS::Camera {
#ifdef __ARCH64__
#define PIPELINE_CONFIG_LIB_PATH HDF_LIBRARY_DIR"64/libcamera_pipeline_config.z.so"
#else
#define PIPELINE_CONFIG_LIB_PATH HDF_LIBRARY_DIR"/libcamera_pipeline_config.z.so"
#endif

using HdfGetModuleConfigRootFunc = const struct HdfConfigRoot* (*)(void);
using HdfGetPipelineSpecsModuleConfigRootFunc = const struct HdfConfigPipelineSpecsRoot* (*)(void);

extern "C" const struct HdfConfigRoot* HdfGetModuleConfigRoot(void)
{
    void* handle = ::dlopen(PIPELINE_CONFIG_LIB_PATH, RTLD_NOW);
    char* errStr = dlerror();
    if (errStr) {
        return nullptr;
    }
    HdfGetModuleConfigRootFunc HdfGetModuleConfigRootF =
        reinterpret_cast<HdfGetModuleConfigRootFunc>(dlsym(handle, "HdfGetModuleConfigRoot"));
    errStr = dlerror();
    if (errStr) {
        return nullptr;
    }

    return HdfGetModuleConfigRootF();
}

extern "C" const struct HdfConfigPipelineSpecsRoot* HdfGetPipelineSpecsModuleConfigRoot(void)
{
    void* handle = ::dlopen(PIPELINE_CONFIG_LIB_PATH, RTLD_NOW);
    char* errStr = dlerror();
    if (errStr) {
        return nullptr;
    }
    HdfGetPipelineSpecsModuleConfigRootFunc HdfGetPipelineSpecsModuleConfigRootF =
        reinterpret_cast<HdfGetPipelineSpecsModuleConfigRootFunc>(dlsym(handle, "HdfGetPipelineSpecsModuleConfigRoot"));
    errStr = dlerror();
    if (errStr) {
        return nullptr;
    }

    return HdfGetPipelineSpecsModuleConfigRootF();
}
}
#endif
