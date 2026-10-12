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
using namespace OHOS::HDI::Display::Buffer::V1_4;

namespace OHOS {
namespace HDI {
namespace Display {
namespace TEST {

void DisplayBufferHdiTest::SetUpTestCase()
{
    HDF_LOGI("DisplayBufferHdiTest SetUpTestCase enter");
}

void DisplayBufferHdiTest::TearDownTestCase()
{
    HDF_LOGI("DisplayBufferHdiTest TearDownTestCase enter");
}

void DisplayBufferHdiTest::SetUp()
{
    displayBuffer_ = IDisplayBuffer::Get();
    ASSERT_NE(displayBuffer_, nullptr);
}

void DisplayBufferHdiTest::TearDown()
{
    displayBuffer_ = nullptr;
}

BufferHandle* DisplayBufferHdiTest::AllocBuffer(const AllocInfo& info)
{
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    if (ret != HDF_SUCCESS || handle == nullptr) {
        HDF_LOGE("AllocBuffer failed, ret = %{public}d", ret);
        return nullptr;
    }
    return handle;
}

void DisplayBufferHdiTest::FreeBuffer(BufferHandle* handle)
{
    if (handle != nullptr) {
        displayBuffer_->FreeMem(*handle);
    }
}

int32_t DisplayBufferHdiTest::FullLifecycleTest(const AllocInfo& info)
{
    BufferHandle* buffer = AllocBuffer(info);
    if (buffer == nullptr) {
        return HDF_FAILURE;
    }

    void* vAddr = displayBuffer_->Mmap(*buffer);
    if (vAddr == nullptr) {
        HDF_LOGE("Mmap failed in FullLifecycleTest");
        FreeBuffer(buffer);
        return HDF_FAILURE;
    }

    int32_t ret = displayBuffer_->InvalidateCache(*buffer);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("InvalidateCache failed, ret = %{public}d", ret);
        displayBuffer_->Unmap(*buffer);
        FreeBuffer(buffer);
        return ret;
    }

    if (memset_s(vAddr, buffer->size, 0x55, buffer->size) != EOK) {
        HDF_LOGE("memset_s failed in FullLifecycleTest");
        displayBuffer_->Unmap(*buffer);
        FreeBuffer(buffer);
        return HDF_FAILURE;
    }

    ret = displayBuffer_->FlushCache(*buffer);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("FlushCache failed, ret = %{public}d", ret);
        displayBuffer_->Unmap(*buffer);
        FreeBuffer(buffer);
        return ret;
    }

    ret = displayBuffer_->InvalidateCache(*buffer);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("InvalidateCache after flush failed, ret = %{public}d", ret);
    }

    displayBuffer_->Unmap(*buffer);
    FreeBuffer(buffer);
    return HDF_SUCCESS;
}

int32_t DisplayBufferHdiTest::MetadataFullTest(BufferHandle& handle)
{
    int32_t ret = displayBuffer_->RegisterBuffer(handle);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("RegisterBuffer failed, ret = %{public}d", ret);
        return ret;
    }

    std::vector<uint8_t> setValue = {0x01, 0x02, 0x03, 0x04};
    ret = displayBuffer_->SetMetadata(handle, METADATA_KEY_DEFAULT, setValue);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("SetMetadata failed, ret = %{public}d", ret);
        return ret;
    }

    std::vector<uint8_t> getValue;
    ret = displayBuffer_->GetMetadata(handle, METADATA_KEY_DEFAULT, getValue);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("GetMetadata failed, ret = %{public}d", ret);
        return ret;
    }
    if (ret == HDF_SUCCESS) {
        if (getValue != setValue) {
            HDF_LOGE("GetMetadata value mismatch");
            return HDF_FAILURE;
        }
    }

    std::vector<uint32_t> keys;
    ret = displayBuffer_->ListMetadataKeys(handle, keys);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("ListMetadataKeys failed, ret = %{public}d", ret);
        return ret;
    }
    if (ret == HDF_SUCCESS) {
        bool foundKey = false;
        for (uint32_t k : keys) {
            if (k == METADATA_KEY_DEFAULT) {
                foundKey = true;
                break;
            }
        }
        if (!foundKey && !keys.empty()) {
            HDF_LOGE("ListMetadataKeys did not contain expected key");
            return HDF_FAILURE;
        }
    }

    ret = displayBuffer_->EraseMetadataKey(handle, METADATA_KEY_DEFAULT);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("EraseMetadataKey failed, ret = %{public}d", ret);
        return ret;
    }

    if (ret == HDF_SUCCESS) {
        getValue.clear();
        ret = displayBuffer_->GetMetadata(handle, METADATA_KEY_DEFAULT, getValue);
        if (ret == HDF_SUCCESS && !getValue.empty()) {
            HDF_LOGE("GetMetadata should fail or return empty after erase");
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

int32_t DisplayBufferHdiTest::ImageLayoutTest(BufferHandle& handle)
{
    ImageLayout layout;
    int32_t ret = displayBuffer_->GetImageLayout(handle, layout);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("GetImageLayout failed, ret = %{public}d", ret);
        return ret;
    }
    if (ret == HDF_SUCCESS) {
        HDF_LOGI("GetImageLayout success, pixelFormat = %{public}u, planes count = %{public}zu",
            layout.pixelFormat, layout.planes.size());
        for (size_t i = 0; i < layout.planes.size(); i++) {
            HDF_LOGI("Plane[%{public}zu]: offset=%{public}u, hStride=%{public}u, vStride=%{public}u",
                i, layout.planes[i].offset, layout.planes[i].hStride, layout.planes[i].vStride);
        }
    }
    return HDF_SUCCESS;
}

int32_t DisplayBufferHdiTest::CloneBufferTest(BufferHandle& handle)
{
    BufferHandle* clonedHandle = nullptr;
    int32_t ret = displayBuffer_->CloneDmaBufferHandle(handle, clonedHandle);
    if (ret != HDF_SUCCESS && ret != HDF_ERR_NOT_SUPPORT) {
        HDF_LOGE("CloneDmaBufferHandle failed, ret = %{public}d", ret);
        return ret;
    }
    if (ret == HDF_SUCCESS && clonedHandle != nullptr) {
        if (clonedHandle->width != handle.width || clonedHandle->height != handle.height) {
            HDF_LOGE("Cloned handle dimensions mismatch");
            FreeBuffer(clonedHandle);
            return HDF_FAILURE;
        }
        if (clonedHandle->format != handle.format) {
            HDF_LOGE("Cloned handle format mismatch");
            FreeBuffer(clonedHandle);
            return HDF_FAILURE;
        }
        FreeBuffer(clonedHandle);
    }
    return HDF_SUCCESS;
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    if (handle != nullptr) {
        EXPECT_GE(handle->size, 0);
        EXPECT_EQ(handle->width, static_cast<int>(ALLOC_WIDTH_1080));
        EXPECT_EQ(handle->height, static_cast<int>(ALLOC_HEIGHT_1920));
        FreeBuffer(handle);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_BGRA_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem004, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem005, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCRCB_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem006, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem007, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCRCB_420_P,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem008, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem009, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem010, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem011, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_CPU_HW_BOTH | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem012, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_CPU_HW_BOTH | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_1010102,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem013, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_2560,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMem014, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_640,
        .height = ALLOC_HEIGHT_480,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMemRepeat, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {
        BufferHandle* handle = nullptr;
        int32_t ret = displayBuffer_->AllocMem(info, handle);
        EXPECT_EQ(ret, HDF_SUCCESS);
        EXPECT_NE(handle, nullptr);
        FreeBuffer(handle);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_FreeMem001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    FreeBuffer(handle);
    SUCCEED();
}

HWTEST_F(DisplayBufferHdiTest, test_Mmap001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    EXPECT_NE(vAddr, nullptr);
    if (vAddr != nullptr) {
        displayBuffer_->Unmap(*handle);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_Mmap002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    EXPECT_NE(vAddr, nullptr);
    if (vAddr != nullptr) {
        if (memset_s(vAddr, handle->size, 0xAA, handle->size) != EOK) {
            HDF_LOGE("memset_s failed in test_Mmap002");
        }
        displayBuffer_->Unmap(*handle);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_Unmap001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    ASSERT_NE(vAddr, nullptr);
    int32_t ret = displayBuffer_->Unmap(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_FlushCache001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    ASSERT_NE(vAddr, nullptr);
    int32_t ret = displayBuffer_->FlushCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    displayBuffer_->Unmap(*handle);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_FlushCache002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    ASSERT_NE(vAddr, nullptr);
    if (memset_s(vAddr, handle->size, 0x33, handle->size) != EOK) {
        HDF_LOGE("memset_s failed in test_FlushCache002");
    }
    int32_t ret = displayBuffer_->FlushCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    displayBuffer_->Unmap(*handle);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_InvalidateCache001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    void* vAddr = displayBuffer_->Mmap(*handle);
    ASSERT_NE(vAddr, nullptr);
    int32_t ret = displayBuffer_->InvalidateCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    displayBuffer_->Unmap(*handle);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_InvalidateCache002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_BGRA_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = displayBuffer_->InvalidateCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_FullLifecycle001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    int32_t ret = FullLifecycleTest(info);
    EXPECT_EQ(ret, HDF_SUCCESS);
}

HWTEST_F(DisplayBufferHdiTest, test_FullLifecycle002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    int32_t ret = FullLifecycleTest(info);
    EXPECT_EQ(ret, HDF_SUCCESS);
}

HWTEST_F(DisplayBufferHdiTest, test_FullLifecycle003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    int32_t ret = FullLifecycleTest(info);
    EXPECT_EQ(ret, HDF_SUCCESS);
}

HWTEST_F(DisplayBufferHdiTest, test_FullLifecycle004, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_CPU_HW_BOTH | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    int32_t ret = FullLifecycleTest(info);
    EXPECT_EQ(ret, HDF_SUCCESS);
}

HWTEST_F(DisplayBufferHdiTest, test_FullLifecycleRepeat, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {
        int32_t ret = FullLifecycleTest(info);
        EXPECT_EQ(ret, HDF_SUCCESS);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataRegisterBuffer001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = displayBuffer_->RegisterBuffer(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataSetAndGet001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    uint32_t key = METADATA_KEY_DEFAULT;
    std::vector<uint8_t> setValue = {0x10, 0x20, 0x30, 0x40};
    int32_t ret = displayBuffer_->SetMetadata(*handle, key, setValue);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);

    std::vector<uint8_t> getValue;
    ret = displayBuffer_->GetMetadata(*handle, key, getValue);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS) {
        EXPECT_EQ(getValue, setValue);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataSetAndGet002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    uint32_t key = METADATA_KEY_COLOR;
    std::vector<uint8_t> setValue(METADATA_VALUE_SIZE_LARGE, 0x77);
    int32_t ret = displayBuffer_->SetMetadata(*handle, key, setValue);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);

    std::vector<uint8_t> getValue;
    ret = displayBuffer_->GetMetadata(*handle, key, getValue);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS) {
        EXPECT_EQ(getValue, setValue);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataSetAndGet003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    std::vector<uint8_t> emptyValue;
    int32_t ret = displayBuffer_->SetMetadata(*handle, METADATA_KEY_CROP, emptyValue);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataListKeys001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    std::vector<uint8_t> setValue = {0x01, 0x02, 0x03};
    displayBuffer_->SetMetadata(*handle, METADATA_KEY_DEFAULT, setValue);
    displayBuffer_->SetMetadata(*handle, METADATA_KEY_COLOR, setValue);
    displayBuffer_->SetMetadata(*handle, METADATA_KEY_CROP, setValue);

    std::vector<uint32_t> keys;
    int32_t ret = displayBuffer_->ListMetadataKeys(*handle, keys);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS) {
        EXPECT_GE(keys.size(), 0);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataListKeys002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    std::vector<uint32_t> keys;
    int32_t ret = displayBuffer_->ListMetadataKeys(*handle, keys);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    if (ret == HDF_SUCCESS) {
        EXPECT_EQ(keys.size(), 0);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataEraseKey001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    std::vector<uint8_t> setValue = {0xAB, 0xCD, 0xEF};
    displayBuffer_->SetMetadata(*handle, METADATA_KEY_DEFAULT, setValue);

    int32_t ret = displayBuffer_->EraseMetadataKey(*handle, METADATA_KEY_DEFAULT);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);

    if (ret == HDF_SUCCESS) {
        std::vector<uint8_t> getValue;
        ret = displayBuffer_->GetMetadata(*handle, METADATA_KEY_DEFAULT, getValue);
        EXPECT_NE(ret, HDF_SUCCESS);
    }
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataEraseKey002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    displayBuffer_->RegisterBuffer(*handle);

    int32_t ret = displayBuffer_->EraseMetadataKey(*handle, METADATA_KEY_INVALID);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataFullWorkflow001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = MetadataFullTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_MetadataFullWorkflow002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = MetadataFullTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_GetImageLayout001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = ImageLayoutTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_GetImageLayout002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = ImageLayoutTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_GetImageLayout003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = ImageLayoutTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_ReAllocMem001, TestSize.Level1)
{
    AllocInfo allocInfo = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* inBuffer = AllocBuffer(allocInfo);
    ASSERT_NE(inBuffer, nullptr);

    AllocInfo reallocInfo = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | OHOS::HDI::Display::Composer::V1_0::HBM_USE_VIDEO_DECODER |
                 OHOS::HDI::Display::Composer::V1_0::HBM_USE_HW_COMPOSER,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* outBuffer = nullptr;
    int32_t ret = displayBuffer_->ReAllocMem(reallocInfo, *inBuffer, outBuffer);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(outBuffer, nullptr);

    FreeBuffer(inBuffer);
    if (outBuffer != nullptr) {
        FreeBuffer(outBuffer);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_ReAllocMem002, TestSize.Level1)
{
    AllocInfo allocInfo = {
        .width = ALLOC_WIDTH_720,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* inBuffer = AllocBuffer(allocInfo);
    ASSERT_NE(inBuffer, nullptr);

    AllocInfo reallocInfo = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | OHOS::HDI::Display::Composer::V1_0::HBM_USE_VIDEO_DECODER |
                 OHOS::HDI::Display::Composer::V1_0::HBM_USE_HW_COMPOSER,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* outBuffer = nullptr;
    int32_t ret = displayBuffer_->ReAllocMem(reallocInfo, *inBuffer, outBuffer);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(outBuffer, nullptr);

    FreeBuffer(inBuffer);
    if (outBuffer != nullptr) {
        FreeBuffer(outBuffer);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_ReAllocMem003, TestSize.Level1)
{
    AllocInfo allocInfo = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* inBuffer = AllocBuffer(allocInfo);
    ASSERT_NE(inBuffer, nullptr);

    AllocInfo reallocInfo = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | OHOS::HDI::Display::Composer::V1_0::HBM_USE_VIDEO_DECODER |
                 OHOS::HDI::Display::Composer::V1_0::HBM_USE_HW_COMPOSER,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* outBuffer = nullptr;
    int32_t ret = displayBuffer_->ReAllocMem(reallocInfo, *inBuffer, outBuffer);
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(outBuffer, nullptr);

    FreeBuffer(inBuffer);
    if (outBuffer != nullptr) {
        FreeBuffer(outBuffer);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_ReAllocMemInvalidInfo, TestSize.Level2)
{
    AllocInfo allocInfo = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_P,
        .expectedSize = 0
    };
    BufferHandle* inBuffer = AllocBuffer(allocInfo);
    ASSERT_NE(inBuffer, nullptr);

    AllocInfo invalidInfo = {};
    BufferHandle* outBuffer = nullptr;
    int32_t ret = displayBuffer_->ReAllocMem(invalidInfo, *inBuffer, outBuffer);
    EXPECT_NE(ret, HDF_SUCCESS);

    FreeBuffer(inBuffer);
    if (outBuffer != nullptr) {
        FreeBuffer(outBuffer);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_CloneDmaBufferHandle001, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = CloneBufferTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_CloneDmaBufferHandle002, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = CloneBufferTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_CloneDmaBufferHandle003, TestSize.Level1)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_YCBCR_420_SP,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);
    int32_t ret = CloneBufferTest(*handle);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
    FreeBuffer(handle);
}

HWTEST_F(DisplayBufferHdiTest, test_IsSupportedAlloc001, TestSize.Level2)
{
    std::vector<VerifyAllocInfo> infos;
    VerifyAllocInfo info1 = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888
    };
    infos.push_back(info1);

    std::vector<bool> supporteds;
    int32_t ret = displayBuffer_->IsSupportedAlloc(infos, supporteds);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
}

HWTEST_F(DisplayBufferHdiTest, test_IsSupportedAlloc002, TestSize.Level2)
{
    std::vector<VerifyAllocInfo> infos;
    VerifyAllocInfo info1 = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888
    };
    VerifyAllocInfo info2 = {
        .width = ALLOC_WIDTH_1920,
        .height = ALLOC_HEIGHT_1080,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBA_8888
    };
    VerifyAllocInfo info3 = {
        .width = ALLOC_WIDTH_1280,
        .height = ALLOC_HEIGHT_720,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_BGRA_8888
    };
    infos.push_back(info1);
    infos.push_back(info2);
    infos.push_back(info3);

    std::vector<bool> supporteds;
    int32_t ret = displayBuffer_->IsSupportedAlloc(infos, supporteds);
    EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
}

HWTEST_F(DisplayBufferHdiTest, test_MultiBufferAlloc001, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_640,
        .height = ALLOC_HEIGHT_480,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    std::vector<BufferHandle*> handles;
    for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {
        BufferHandle* handle = AllocBuffer(info);
        EXPECT_NE(handle, nullptr);
        if (handle != nullptr) {
            handles.push_back(handle);
        }
    }
    for (auto handle : handles) {
        FreeBuffer(handle);
    }
}

HWTEST_F(DisplayBufferHdiTest, test_AllocAndMmapMultiFormat, TestSize.Level2)
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
        BufferHandle* handle = AllocBuffer(info);
        EXPECT_NE(handle, nullptr);
        if (handle != nullptr) {
            void* vAddr = displayBuffer_->Mmap(*handle);
            EXPECT_NE(vAddr, nullptr);
            if (vAddr != nullptr) {
                displayBuffer_->Unmap(*handle);
            }
            FreeBuffer(handle);
        }
    }
}

HWTEST_F(DisplayBufferHdiTest, test_AllocMmapWriteFlushUnmapFree, TestSize.Level2)
{
    AllocInfo info = {
        .width = ALLOC_WIDTH_1080,
        .height = ALLOC_HEIGHT_1920,
        .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
        .format = PIXEL_FMT_RGBX_8888,
        .expectedSize = 0
    };
    BufferHandle* handle = AllocBuffer(info);
    ASSERT_NE(handle, nullptr);

    void* vAddr = displayBuffer_->Mmap(*handle);
    ASSERT_NE(vAddr, nullptr);

    int32_t ret = displayBuffer_->InvalidateCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);

    for (uint32_t i = 0; i < handle->size; i++) {
        static_cast<uint8_t*>(vAddr)[i] = static_cast<uint8_t>(i & 0xFF);
    }

    ret = displayBuffer_->FlushCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);

    ret = displayBuffer_->InvalidateCache(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);

    bool dataValid = true;
    for (uint32_t i = 0; i < handle->size; i++) {
        if (static_cast<uint8_t*>(vAddr)[i] != static_cast<uint8_t>(i & 0xFF)) {
            dataValid = false;
            break;
        }
    }
    EXPECT_TRUE(dataValid);

    ret = displayBuffer_->Unmap(*handle);
    EXPECT_EQ(ret, HDF_SUCCESS);

    FreeBuffer(handle);
}

} // namespace TEST
} // namespace Display
} // namespace HDI
} // namespace OHOS
