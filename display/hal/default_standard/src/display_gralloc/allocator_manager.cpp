/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "allocator_manager.h"
#include <mutex>
#include "display_common.h"
#include "framebuffer_allocator.h"
#include "dmabufferheap_allocator.h"
#include "drm_allocator.h"

namespace OHOS {
namespace HDI {
namespace DISPLAY {
// Fix FCT bug3: AllocatorManager is a process-wide singleton with no lock;
// GrallocInitialize/Uninitialize and the per-call AllocMem/FreeMem paths can
// race on init_/allocator_/frameBufferAllocator_. Serialize all access.
static std::mutex g_allocatorMutex;

int32_t AllocatorManager::Init()
{
    DISPLAY_LOGD("AllocatorManager::Init");
    std::lock_guard<std::mutex> lock(g_allocatorMutex);
    if (init_) {
        DISPLAY_LOGW("allocator has initialized");
        return DISPLAY_SUCCESS;
    }
    // first use drm allocator
    std::shared_ptr<Allocator> drmAllocator = std::make_shared<DrmAllocator>();
    int ret = drmAllocator->Init();
    if (ret == DISPLAY_SUCCESS) {
        frameBufferAllocator_ = drmAllocator;
        allocator_ = drmAllocator;
    } else {
        std::shared_ptr<Allocator> fbAllocator = std::make_shared<FramebufferAllocator>();
        ret = fbAllocator->Init();
        DISPLAY_CHK_RETURN((ret != DISPLAY_SUCCESS), DISPLAY_FAILURE,
            DISPLAY_LOGE("Failed init framebuffer allocator"));

        std::shared_ptr<Allocator> dmaBufferallocator = std::make_shared<DmaBufferHeapAllocator>();
        ret = dmaBufferallocator->Init();
        DISPLAY_CHK_RETURN((ret != DISPLAY_SUCCESS), DISPLAY_FAILURE,
            DISPLAY_LOGE("Failed init Dmabuffer allocator"));

        frameBufferAllocator_ = fbAllocator;
        allocator_ = dmaBufferallocator;
    }

    init_ = true;
    DISPLAY_LOGD("init success");
    return DISPLAY_SUCCESS;
}

int32_t AllocatorManager::DeInit()
{
    DISPLAY_LOGD();
    std::lock_guard<std::mutex> lock(g_allocatorMutex);
    init_ = false;
    frameBufferAllocator_.reset();
    allocator_.reset();
    return DISPLAY_SUCCESS;
}

Allocator* AllocatorManager::GetAllocator(uint64_t usage)
{
    DISPLAY_LOGD();
    // Fix FCT bug3: read the shared_ptr members under the lock (copy the
    // shared_ptr first, then call get()) so the read does not race with a
    // concurrent Init/DeInit resetting the members.
    std::shared_ptr<Allocator> allocator;
    {
        std::lock_guard<std::mutex> lock(g_allocatorMutex);
        allocator = ((usage & HBM_USE_MEM_FB) != 0) ? frameBufferAllocator_ : allocator_;
    }
    return allocator.get();
}
}  // namespace DISPLAY
}  // namespace HDI
}  // namespace OHOS