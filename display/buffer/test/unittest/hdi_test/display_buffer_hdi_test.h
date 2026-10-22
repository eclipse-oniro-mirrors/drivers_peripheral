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

#ifndef DISPLAY_BUFFER_HDI_TEST_H
#define DISPLAY_BUFFER_HDI_TEST_H

#include <securec.h>

#include "gtest/gtest.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "buffer_handle.h"
#include "v1_0/display_buffer_type.h"
#include "v1_0/iallocator.h"
#include "v1_0/imapper.h"
#include "v1_1/imetadata.h"
#include "v1_2/display_buffer_type.h"
#include "v1_2/imapper.h"
#include "v1_3/imapper.h"
#include "v1_4/iallocator.h"
#include "v1_4/include/idisplay_buffer.h"

namespace OHOS {
namespace HDI {
namespace Display {
namespace TEST {

using namespace testing::ext;

using OHOS::HDI::Display::Buffer::V1_0::AllocInfo;
using OHOS::HDI::Display::Buffer::V1_0::VerifyAllocInfo;
using OHOS::HDI::Display::Buffer::V1_0::IAllocator;
using OHOS::HDI::Display::Buffer::V1_0::IMapper;
using OHOS::HDI::Display::Buffer::V1_1::IMetadata;
using OHOS::HDI::Display::Buffer::V1_2::ImageLayout;
using OHOS::HDI::Display::Buffer::V1_2::ImagePlane;
using OHOS::HDI::Display::Buffer::V1_3::IDisplayBuffer;

const uint32_t ALLOC_WIDTH_1080 = 1080;
const uint32_t ALLOC_WIDTH_1920 = 1920;
const uint32_t ALLOC_WIDTH_1280 = 1280;
const uint32_t ALLOC_WIDTH_720 = 720;
const uint32_t ALLOC_WIDTH_2560 = 2560;
const uint32_t ALLOC_WIDTH_640 = 640;
const uint32_t ALLOC_WIDTH_480 = 480;
const uint32_t ALLOC_HEIGHT_1080 = 1080;
const uint32_t ALLOC_HEIGHT_1920 = 1920;
const uint32_t ALLOC_HEIGHT_720 = 720;
const uint32_t ALLOC_HEIGHT_1280 = 1280;
const uint32_t ALLOC_HEIGHT_480 = 480;
const int32_t TEST_REPEAT_COUNT = 10;
const int32_t TEST_REPEAT_LARGE = 40;
const uint32_t METADATA_KEY_DEFAULT = 1;
const uint32_t METADATA_KEY_COLOR = 2;
const uint32_t METADATA_KEY_CROP = 3;
const uint32_t METADATA_KEY_INVALID = 9999;
const uint32_t METADATA_VALUE_SIZE_SMALL = 4;
const uint32_t METADATA_VALUE_SIZE_LARGE = 256;

const AllocInfo DEFAULT_ALLOC_INFO = {
    .width = ALLOC_WIDTH_1080,
    .height = ALLOC_HEIGHT_1920,
    .usage = 0x1000UL | 0x2UL | 0x1UL,
    .format = PIXEL_FMT_RGBX_8888,
    .expectedSize = 0
};

class DisplayBufferHdiTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    IDisplayBuffer* displayBuffer_ { nullptr };
    sptr<IAllocator> allocator_ { nullptr };
    sptr<IMapper> mapper_ { nullptr };
    sptr<IMetadata> metadata_ { nullptr };

    BufferHandle* AllocBuffer(const AllocInfo& info);
    void FreeBuffer(BufferHandle* handle);
    int32_t FullLifecycleTest(const AllocInfo& info);
    int32_t MetadataFullTest(BufferHandle& handle);
    int32_t ImageLayoutTest(BufferHandle& handle);
    int32_t CloneBufferTest(BufferHandle& handle);
};

class AllocatorInterfaceTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    sptr<IAllocator> allocator_ { nullptr };
    sptr<OHOS::HDI::Display::Buffer::V1_4::IAllocator> allocatorV1_4_ { nullptr };

    sptr<NativeBuffer> DoAlloc(const AllocInfo& info);
};

class MapperInterfaceTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    sptr<IMapper> mapper_ { nullptr };
    sptr<OHOS::HDI::Display::Buffer::V1_3::IMapper> mapperV1_3_ { nullptr };

    sptr<NativeBuffer> AllocViaMapper(const AllocInfo& info);
};

class MetadataInterfaceTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    sptr<IMetadata> metadata_ { nullptr };
    sptr<IAllocator> allocator_ { nullptr };

    sptr<NativeBuffer> AllocBufferForMetadata(const AllocInfo& info);
};

} // namespace TEST
} // namespace Display
} // namespace HDI
} // namespace OHOS

#endif // DISPLAY_BUFFER_HDI_TEST_H
