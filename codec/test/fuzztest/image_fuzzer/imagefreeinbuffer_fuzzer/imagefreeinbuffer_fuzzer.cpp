/*
 * Copyright (c) 2023 Shenzhen Kaihong DID Co., Ltd.
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

#include "imagefreeinbuffer_fuzzer.h"
#include <hdf_log.h>
#include <image_auto_initer.h>
#include <vector>
#include "image_common.h"
#include "v1_0/icodec_image_jpeg.h"
using namespace OHOS::HDI::Codec::Image::V1_0;
using namespace OHOS;
using namespace std;
namespace OHOS {
namespace Codec {
namespace Image {
bool FreeInBuffer(const uint8_t *data, size_t size)
{
    if (data == nullptr) {
        return false;
    }

    sptr<ICodecImageJpeg> image = ICodecImageJpeg::Get(false);
    if (image == nullptr) {
        HDF_LOGE("%{public}s: get ICodecImageJpeg failed\n", __func__);
        return false;
    }
    ImageAutoIniter autoIniter(image);
    CodecImageBuffer inBuffer;
    FillDataImageBuffer(inBuffer);
    auto err = image->FreeInBuffer(inBuffer);
    if (err != HDF_SUCCESS) {
        HDF_LOGE("%{public}s FreeInBuffer return %{public}d", __func__, err);
    }
    return true;
}
}  // namespace Image
}  // namespace Codec
}  // namespace OHOS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::Codec::Image::FreeInBuffer(data, size);
    return 0;
}
