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

#include "mapper_service.h"

#include <dlfcn.h>
#include <hdf_base.h>
#include <hdf_log.h>
#include "display_log.h"

namespace OHOS {
namespace HDI {
namespace Display {
namespace Buffer {
namespace V1_0 {
extern "C" IMapper* MapperImplGetInstance(void)
{
    return new (std::nothrow) MapperService();
}

MapperService::MapperService()
    : libHandle_(nullptr),
    vdiImpl_(nullptr),
    createVdi_(nullptr),
    destroyVdi_(nullptr)
{
    int32_t ret = LoadVdi();
    if (ret == HDF_SUCCESS) {
        vdiImpl_ = createVdi_();
        CHECK_NULLPOINTER_RETURN(vdiImpl_);
    } else {
        HDF_LOGE("%{public}s: Load buffer VDI failed, lib: %{public}s", __func__, DISPLAY_BUFFER_VDI_LIBRARY);
    }
}

MapperService::~MapperService()
{
    if (destroyVdi_ != nullptr && vdiImpl_ != nullptr) {
        destroyVdi_(vdiImpl_);
    }
    if (libHandle_ != nullptr) {
        dlclose(libHandle_);
    }
}

int32_t MapperService::LoadVdi()
{
    const char* errStr = dlerror();
    if (errStr != nullptr) {
        HDF_LOGI("%{public}s: mapper loadvid, clear earlier dlerror: %{public}s", __func__, errStr);
    }
    libHandle_ = dlopen(DISPLAY_BUFFER_VDI_LIBRARY, RTLD_LAZY);
    CHECK_NULLPOINTER_RETURN_VALUE(libHandle_, HDF_FAILURE);

    createVdi_ = reinterpret_cast<CreateDisplayBufferVdiFunc>(dlsym(libHandle_, "CreateDisplayBufferVdi"));
    if (createVdi_ == nullptr) {
        errStr = dlerror();
        if (errStr != nullptr) {
            HDF_LOGE("%{public}s: mapper CreateDisplayBufferVdi dlsym error: %{public}s", __func__, errStr);
        }
        dlclose(libHandle_);
        return HDF_FAILURE;
    }

    destroyVdi_ = reinterpret_cast<DestroyDisplayBufferVdiFunc>(dlsym(libHandle_, "DestroyDisplayBufferVdi"));
    if (destroyVdi_ == nullptr) {
        errStr = dlerror();
        if (errStr != nullptr) {
            HDF_LOGE("%{public}s: mapper DestroyDisplayBufferVdi dlsym error: %{public}s", __func__, errStr);
        }
        dlclose(libHandle_);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t MapperService::FreeMem(const sptr<NativeBuffer>& handle)
{
    CHECK_NULLPOINTER_RETURN_VALUE(handle, HDF_FAILURE);
    CHECK_NULLPOINTER_RETURN_VALUE(vdiImpl_, HDF_FAILURE);
    vdiImpl_->FreeMem(*handle->Move());
    return HDF_SUCCESS;
}

int32_t MapperService::Mmap(const sptr<NativeBuffer>& handle)
{
    CHECK_NULLPOINTER_RETURN_VALUE(handle, HDF_FAILURE);
    CHECK_NULLPOINTER_RETURN_VALUE(vdiImpl_, HDF_FAILURE);
    void* retPtr = vdiImpl_->Mmap(*handle->GetBufferHandle());
    CHECK_NULLPOINTER_RETURN_VALUE(retPtr, HDF_FAILURE);
    return HDF_SUCCESS;
}

int32_t MapperService::Unmap(const sptr<NativeBuffer>& handle)
{
    CHECK_NULLPOINTER_RETURN_VALUE(handle, HDF_FAILURE);
    CHECK_NULLPOINTER_RETURN_VALUE(vdiImpl_, HDF_FAILURE);
    int32_t ret = vdiImpl_->Unmap(*handle->GetBufferHandle());
    DISPLAY_CHK_RETURN(ret != HDF_SUCCESS, HDF_FAILURE, DISPLAY_LOGE(" fail"));
    return ret;
}

int32_t MapperService::FlushCache(const sptr<NativeBuffer>& handle)
{
    CHECK_NULLPOINTER_RETURN_VALUE(handle, HDF_FAILURE);
    CHECK_NULLPOINTER_RETURN_VALUE(vdiImpl_, HDF_FAILURE);
    int32_t ret = vdiImpl_->FlushCache(*handle->GetBufferHandle());
    DISPLAY_CHK_RETURN(ret != HDF_SUCCESS, HDF_FAILURE, DISPLAY_LOGE(" fail"));
    return ret;
}

int32_t MapperService::InvalidateCache(const sptr<NativeBuffer>& handle)
{
    CHECK_NULLPOINTER_RETURN_VALUE(handle, HDF_FAILURE);
    CHECK_NULLPOINTER_RETURN_VALUE(vdiImpl_, HDF_FAILURE);
    int32_t ret = vdiImpl_->InvalidateCache(*handle->GetBufferHandle());
    DISPLAY_CHK_RETURN(ret != HDF_SUCCESS, HDF_FAILURE, DISPLAY_LOGE(" fail"));
    return ret;
}
} // namespace V1_0
} // namespace Buffer
} // namespace Display
} // namespace HDI
} // namespace OHOS