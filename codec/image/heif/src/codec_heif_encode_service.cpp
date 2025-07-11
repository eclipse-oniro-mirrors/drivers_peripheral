/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "codec_heif_encode_service.h"
#include <algorithm>
#include "codec_log_wrapper.h"
#include "hdf_base.h"
#include "hdf_remote_service.h"
#include <dlfcn.h>
#include <unistd.h>

namespace OHOS {
namespace HDI {
namespace Codec {
namespace Image {
namespace V2_1 {
using GetCodecHeifHwi = OHOS::VDI::HEIF::ICodecHeifHwi*(*)();

CodecHeifEncodeService::CodecHeifEncodeService()
{
    isIPCMode_ = (HdfRemoteGetCallingPid() == getpid() ? false : true);
}

CodecHeifEncodeService::~CodecHeifEncodeService()
{
    heifHwi_ = nullptr;
    libHeif_ = nullptr;
}

bool CodecHeifEncodeService::LoadVendorLib()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (heifHwi_) {
        return true;
    }
    if (libHeif_ == nullptr) {
        void *handle = dlopen(CODEC_HEIF_VDI_LIB_NAME, RTLD_LAZY);
        if (handle == nullptr) {
            CODEC_LOGE("failed to load vendor lib");
            return false;
        }
        libHeif_ = std::shared_ptr<void>(handle, dlclose);
    }
    auto func = reinterpret_cast<GetCodecHeifHwi>(dlsym(libHeif_.get(), "GetCodecHeifHwi"));
    if (func == nullptr) {
        CODEC_LOGE("failed to load symbol from vendor lib");
        return false;
    }
    heifHwi_ = func();
    if (heifHwi_ == nullptr) {
        CODEC_LOGE("failed to create heif hardware encoder");
        return false;
    }
    return true;
}

int32_t CodecHeifEncodeService::DoHeifEncode(const std::vector<ImageItem>& inputImgs,
                                             const std::vector<MetaItem>& inputMetas,
                                             const std::vector<ItemRef>& refs,
                                             const SharedBuffer& output, uint32_t& filledLen)
{
    if (!isIPCMode_) {
        return HDF_FAILURE;
    }

    std::vector<OHOS::VDI::HEIF::ImageItem> inputImgsInternal(inputImgs.size());
    std::transform(inputImgs.cbegin(), inputImgs.cend(), inputImgsInternal.begin(),
        OHOS::VDI::HEIF::ConvertImageItem);

    std::vector<OHOS::VDI::HEIF::MetaItem> inputMetasInternal(inputMetas.size());
    std::transform(inputMetas.cbegin(), inputMetas.cend(), inputMetasInternal.begin(),
        OHOS::VDI::HEIF::ConvertMetaItem);

    OHOS::VDI::HEIF::SharedBuffer outputToReturn = OHOS::VDI::HEIF::ConvertSharedBuffer(output);

    if (!LoadVendorLib()) {
        return HDF_FAILURE;
    }

    int32_t ret = (heifHwi_->DoHeifEncode)(inputImgsInternal, inputMetasInternal, refs, outputToReturn);
    filledLen = outputToReturn.filledLen;
    return ret;
}
} // V2_1
} // Image
} // Codec
} // HDI
} // OHOS