/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_HDI_CODEC_V1_0_CODECJPEGCORE_H
#define OHOS_HDI_CODEC_V1_0_CODECJPEGCORE_H

#include "codec_jpeg_vdi.h"
#include "v1_0/icodec_image_jpeg.h"

namespace OHOS {
namespace HDI {
namespace Codec {
namespace Image {
class CodecJpegCore {
public:
    using GetCodecJpegHwi = ICodecJpegHwi*(*)();

    explicit CodecJpegCore();

    ~CodecJpegCore();

    int32_t Init();

    int32_t DeInit();

    int32_t AllocateInBuffer(BufferHandle **buffer, uint32_t size);

    int32_t FreeInBuffer(BufferHandle *buffer);

    int32_t DoDecode(BufferHandle *buffer, BufferHandle *outBuffer, const V1_0::CodecJpegDecInfo *decInfo,
                     const OHOS::sptr<V1_0::ICodecImageCallback> callbacks, int fenceFd);

    static int32_t OnEvent(int32_t error);

private:
    void AddVendorLib();

    static int32_t SyncWait(int fd);

private:
    void *libHandle_ = nullptr;
    GetCodecJpegHwi getCodecJpegHwi_ = nullptr;
    ICodecJpegHwi *JpegHwi_;
    struct CodecJpegCallbackHwi vdiCallback_;
    static int fence_;
    static OHOS::sptr<V1_0::ICodecImageCallback> callback_;
};
} // Image
} // Codec
} // HDI
} // OHOS

#endif // OHOS_HDI_CODEC_V1_0_CODECJPEGCORE_H