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

#include "display_buffer_hdi_test.h"

using namespace OHOS::HDI::Display::Composer::V1_1;
using namespace OHOS::HDI::Display::Composer::V1_2;
using namespace OHOS::HDI::Display::Buffer::V1_0;
using namespace OHOS::HDI::Display::Buffer::V1_4;

namespace OHOS {
namespace HDI {
namespace Display {
namespace TEST {

void AllocatorInterfaceTest::SetUpTestCase()
{
    HDF_LOGI("AllocatorInterfaceTest SetUpTestCase enter");
}

void AllocatorInterfaceTest::TearDownTestCase()
{
    HDF_LOGI("AllocatorInterfaceTest TearDownTestCase enter");
}

void AllocatorInterfaceTest::SetUp()
{
    allocator_ = IAllocator::Get(true);
    ASSERT_NE(allocator_, nullptr);
    allocatorV1_4_ = OHOS::HDI::Display::Buffer::V1_4::IAllocator::Get(true);
}

void AllocatorInterfaceTest::TearDown()
{
    allocator_ = nullptr;
    allocatorV1_4_ = nullptr;
}

sptr<NativeBuffer> AllocatorInterfaceTest::DoAlloc(const AllocInfo& info)
{
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    if (ret != HDF_SUCCESS || handle == nullptr) {
        HDF_LOGE("AllocatorInterface DoAlloc failed, ret = %{public}d", ret);
        return nullptr;
    }
    return handle;
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_BGRA_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem004, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem005, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem006, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem007, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_CPU_HW_BOTH | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem008, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_2560,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem009, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_640,
        .height = ALLOC_HEIGHT_480,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMem010, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_CPU_HW_BOTH | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_1010102,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemRepeat, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {
        sptr<NativeBuffer> handle = nullptr;
        int32_t ret = allocator_->AllocMem(info, handle);
        EXPECT_EQ(ret, HDF_SUCCESS);
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemVerifyHandle, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    ASSERT_EQ(ret, HDF_SUCCESS);
    ASSERT_NE(handle, nullptr);

    BufferHandle* buffer = handle->GetBufferHandle();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->width, static_cast<int>(ALLOC_WIDTH_1080));
    EXPECT_EQ(buffer->height, static_cast<int>(ALLOC_HEIGHT_1920));
    EXPECT_EQ(buffer->format, PIXEL_FMT_RGBX_8888);
    EXPECT_GT(buffer->size, 0);
    EXPECT_GE(buffer->fd, 0);
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemMultiFormat, TestSize.Level2)
{
    const uint32_t formats[] = {
        PIXEL_FMT_RGBX_8888,
        PIXEL_FMT_RGBA_8888,
        PIXEL_FMT_BGRA_8888,
        PIXEL_FMT_YCBCR_420_SP,
        PIXEL_FMT_YCRCB_420_SP,
        PIXEL_FMT_YCBCR_420_P,
        PIXEL_FMT_YCRCB_420_P,
    };
    const int32_t formatCount = sizeof(formats) / sizeof(formats[0]);
    for (int32_t i = 0; i < formatCount; i++) {
        AllocInfo info = {
            .width = ALLOC_WIDTH_1080,
            .height = ALLOC_HEIGHT_1920,
            .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
            .format = formats[i],
            .expectedSize = 0
        };
        sptr<NativeBuffer> handle = nullptr;
        int32_t ret = allocator_->AllocMem(info, handle);
        EXPECT_EQ(ret, HDF_SUCCESS);
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemMultiSize, TestSize.Level2)
{
    const uint32_t widths[] = {ALLOC_WIDTH_480, ALLOC_WIDTH_640, ALLOC_WIDTH_720, ALLOC_WIDTH_1080,
                               ALLOC_WIDTH_1280, ALLOC_WIDTH_1920, ALLOC_WIDTH_2560};
    const uint32_t heights[] = {ALLOC_HEIGHT_480, ALLOC_HEIGHT_720, ALLOC_HEIGHT_1080, ALLOC_HEIGHT_1920};
    const int32_t widthCount = sizeof(widths) / sizeof(widths[0]);
    const int32_t heightCount = sizeof(heights) / sizeof(heights[0]);
    for (int32_t wi = 0; wi < widthCount; wi++) {
        for (int32_t hi = 0; hi < heightCount; hi++) {
            AllocInfo info = {
                .width = widths[wi],
                .height = heights[hi],
                .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
                .format = PIXEL_FMT_RGBX_8888,
                .expectedSize = 0
            };
            sptr<NativeBuffer> handle = nullptr;
            int32_t ret = allocator_->AllocMem(info, handle);
            EXPECT_EQ(ret, HDF_SUCCESS);
            EXPECT_NE(handle, nullptr);
        }
    }
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorMultiBufferAlloc, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_640,
        .height = ALLOC_HEIGHT_480,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    std::vector<sptr<NativeBuffer>> handles;
    for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {
        sptr<NativeBuffer> handle = nullptr;
        int32_t ret = allocator_->AllocMem(info, handle);
        EXPECT_EQ(ret, HDF_SUCCESS);
        EXPECT_NE(handle, nullptr);
        if (handle != nullptr) {
            handles.push_back(handle);
        }
    }
}

HWTEST_F(AllocatorInterfaceTest, test_CloneDmaBufferHandle001, TestSize.Level1)
{
    if (allocatorV1_4_ == nullptr) {
        GTEST_SKIP() << "V1_4 IAllocator not available";
    }
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> inHandle = nullptr;
    int32_t ret = allocator_->AllocMem(info, inHandle);
    ASSERT_EQ(ret, HDF_SUCCESS);
    ASSERT_NE(inHandle, nullptr);

    sptr<NativeBuffer> outHandle = nullptr;
    ret = allocatorV1_4_->CloneDmaBufferHandle(inHandle, outHandle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS) {
        EXPECT_NE(outHandle, nullptr);
        BufferHandle* inBuffer = inHandle->GetBufferHandle();
        BufferHandle* outBuffer = (outHandle != nullptr) ? outHandle->GetBufferHandle() : nullptr;
        if (inBuffer != nullptr && outBuffer != nullptr) {
            EXPECT_EQ(outBuffer->width, inBuffer->width);
            EXPECT_EQ(outBuffer->height, inBuffer->height);
            EXPECT_EQ(outBuffer->format, inBuffer->format);
        }
    }
}

HWTEST_F(AllocatorInterfaceTest, test_CloneDmaBufferHandle002, TestSize.Level1)
{
    if (allocatorV1_4_ == nullptr) {
        GTEST_SKIP() << "V1_4 IAllocator not available";
    }
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> inHandle = nullptr;
    int32_t ret = allocator_->AllocMem(info, inHandle);
    ASSERT_EQ(ret, HDF_SUCCESS);
    ASSERT_NE(inHandle, nullptr);

    sptr<NativeBuffer> outHandle = nullptr;
    ret = allocatorV1_4_->CloneDmaBufferHandle(inHandle, outHandle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
}

HWTEST_F(AllocatorInterfaceTest, test_CloneDmaBufferHandle003, TestSize.Level1)
{
    if (allocatorV1_4_ == nullptr) {
        GTEST_SKIP() << "V1_4 IAllocator not available";
    }
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    sptr<NativeBuffer> inHandle = nullptr;
    int32_t ret = allocator_->AllocMem(info, inHandle);
    ASSERT_EQ(ret, HDF_SUCCESS);
    ASSERT_NE(inHandle, nullptr);

    sptr<NativeBuffer> outHandle = nullptr;
    ret = allocatorV1_4_->CloneDmaBufferHandle(inHandle, outHandle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS && outHandle != nullptr) {
        BufferHandle* inBuffer = inHandle->GetBufferHandle();
        BufferHandle* outBuffer = outHandle->GetBufferHandle();
        if (inBuffer != nullptr && outBuffer != nullptr) {
            EXPECT_EQ(outBuffer->size, inBuffer->size);
            EXPECT_GE(outBuffer->fd, 0);
        }
    }
}

HWTEST_F(AllocatorInterfaceTest, test_CloneDmaBufferHandleMultiFormat, TestSize.Level2)
{
    if (allocatorV1_4_ == nullptr) {
        GTEST_SKIP() << "V1_4 IAllocator not available";
    }
    const uint32_t formats[] = {
        PIXEL_FMT_RGBX_8888,
        PIXEL_FMT_RGBA_8888,
        PIXEL_FMT_BGRA_8888,
        PIXEL_FMT_YCBCR_420_SP,
        PIXEL_FMT_YCBCR_420_P,
    };
    const int32_t formatCount = sizeof(formats) / sizeof(formats[0]);
    for (int32_t i = 0; i < formatCount; i++) {
        AllocInfo info = {
            .width = ALLOC_WIDTH_1080,
            .height = ALLOC_HEIGHT_1920,
            .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
            .format = formats[i],
            .expectedSize = 0
        };
        sptr<NativeBuffer> inHandle = nullptr;
        int32_t ret = allocator_->AllocMem(info, inHandle);
        ASSERT_EQ(ret, HDF_SUCCESS);
        ASSERT_NE(inHandle, nullptr);

        sptr<NativeBuffer> outHandle = nullptr;
        ret = allocatorV1_4_->CloneDmaBufferHandle(inHandle, outHandle);
        EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    }
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemWithExpectedSize, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = ALLOC_WIDTH_1080 * ALLOC_HEIGHT_1920 * 4
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    if (handle != nullptr) {
        BufferHandle* buffer = handle->GetBufferHandle();
        if (buffer != nullptr) {
            EXPECT_GE(buffer->size, static_cast<int>(info.expectedSize));
        }
    }
}

HWTEST_F(AllocatorInterfaceTest, test_AllocatorAllocMemWithZeroExpectedSize, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    sptr<NativeBuffer> handle = nullptr;
    int32_t ret = allocator_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
}

} // namespace TEST
} // namespace Display
} // namespace HDI
} // namespace OHOS
