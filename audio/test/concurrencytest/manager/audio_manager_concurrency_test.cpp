/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <atomic>
#include <climits>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "hdf_dlist.h"
#include "osal_mem.h"
#include "v6_1/audio_types.h"
#include "v6_1/iaudio_adapter.h"
#include "v6_1/iaudio_manager.h"

using namespace std;
using namespace testing::ext;

#define AUDIO_ADAPTER_NUM_MAX   5
#define AUDIO_THREAD_NUM        4
#define AUDIO_LOAD_REPEAT_TIMES 8
#define AUDIO_INVALID_NAME      "invalid_adapter_name_ext_test"

namespace {
static void ReleaseAdapterDescs(struct AudioAdapterDescriptor **descs, uint32_t descsLen)
{
    if ((descsLen > 0) && (descs != nullptr) && ((*descs) != nullptr)) {
        for (uint32_t i = 0; i < descsLen; i++) {
            AudioAdapterDescriptorFree(&(*descs)[i], false);
        }
        OsalMemFree(*descs);
        *descs = nullptr;
    }
}

static int32_t PrepareAdapterManager(struct IAudioManager **manager, struct AudioAdapterDescriptor **descs,
    uint32_t *adapterNum)
{
    if ((manager == nullptr) || (descs == nullptr) || (adapterNum == nullptr)) {
        return HDF_ERR_INVALID_PARAM;
    }
    *manager = IAudioManagerGet(false);
    if (*manager == nullptr) {
        return HDF_FAILURE;
    }
    *descs = (struct AudioAdapterDescriptor *)OsalMemCalloc(
        sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
    if (*descs == nullptr) {
        IAudioManagerRelease(*manager, false);
        *manager = nullptr;
        return HDF_ERR_MALLOC_FAIL;
    }
    uint32_t size = AUDIO_ADAPTER_NUM_MAX;
    int32_t ret = (*manager)->GetAllAdapters(*manager, *descs, &size);
    if (ret != HDF_SUCCESS || size == 0) {
        ReleaseAdapterDescs(descs, AUDIO_ADAPTER_NUM_MAX);
        IAudioManagerRelease(*manager, false);
        *manager = nullptr;
        return (ret != HDF_SUCCESS) ? ret : HDF_FAILURE;
    }
    *adapterNum = size;
    return HDF_SUCCESS;
}

static void LoadAdapterRepeatWorker(struct IAudioManager *manager, struct AudioAdapterDescriptor *descs,
    std::atomic<int32_t> &loadCnt, std::atomic<int32_t> &unloadCnt)
{
    for (uint32_t j = 0; j < AUDIO_LOAD_REPEAT_TIMES; j++) {
        struct IAudioAdapter *adapter = nullptr;
        if (manager->LoadAdapter(manager, &descs[0], &adapter) != HDF_SUCCESS) {
            continue;
        }
        loadCnt++;
        if (manager->UnloadAdapter(manager, descs[0].adapterName) == HDF_SUCCESS) {
            unloadCnt++;
        }
    }
}

static void QueryAdaptersWorker(struct IAudioManager *manager, std::atomic<int32_t> &queryCnt)
{
    // each thread uses its own descriptor array
    uint32_t size = AUDIO_ADAPTER_NUM_MAX;
    struct AudioAdapterDescriptor *queryDescs = (struct AudioAdapterDescriptor *)OsalMemCalloc(
        sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
    if (queryDescs == nullptr) {
        return;
    }
    if (manager->GetAllAdapters(manager, queryDescs, &size) == HDF_SUCCESS) {
        queryCnt++;
    }
    ReleaseAdapterDescs(&queryDescs, AUDIO_ADAPTER_NUM_MAX);
}

static void LoadAdapterOnceWorker(struct IAudioManager *manager, struct AudioAdapterDescriptor *descs,
    std::atomic<int32_t> &loadCnt)
{
    struct IAudioAdapter *adapter = nullptr;
    if (manager->LoadAdapter(manager, &descs[0], &adapter) != HDF_SUCCESS) {
        return;
    }
    loadCnt++;
    manager->UnloadAdapter(manager, descs[0].adapterName);
}

class HdfAudioConcManagerTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
};

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcGetAllAdaptersMutiThread001, TestSize.Level1)
{
    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt]() {
            struct IAudioManager *manager = IAudioManagerGet(false);
            if (manager == nullptr) {
                return;
            }
            uint32_t size = AUDIO_ADAPTER_NUM_MAX;
            struct AudioAdapterDescriptor *descs = (struct AudioAdapterDescriptor *)OsalMemCalloc(
                sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
            if (descs == nullptr) {
                IAudioManagerRelease(manager, false);
                return;
            }
            if (manager->GetAllAdapters(manager, descs, &size) == HDF_SUCCESS) {
                successCnt++;
            }
            ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
            IAudioManagerRelease(manager, false);
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcLoadAdapterMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = IAudioManagerGet(false);
    ASSERT_NE(manager, nullptr);

    uint32_t size = AUDIO_ADAPTER_NUM_MAX;
    struct AudioAdapterDescriptor *descs = (struct AudioAdapterDescriptor *)OsalMemCalloc(
        sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
    ASSERT_NE(descs, nullptr);
    ASSERT_EQ(HDF_SUCCESS, manager->GetAllAdapters(manager, descs, &size));
    ASSERT_GT(size, 0);

    std::atomic<int32_t> loadCnt(0);
    std::atomic<int32_t> unloadCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&loadCnt, &unloadCnt, manager, descs]() {
            struct IAudioAdapter *adapter = nullptr;
            if (manager->LoadAdapter(manager, &descs[0], &adapter) != HDF_SUCCESS) {
                return;
            }
            loadCnt++;
            if (manager->UnloadAdapter(manager, descs[0].adapterName) == HDF_SUCCESS) {
                unloadCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
    IAudioManagerRelease(manager, false);

    EXPECT_EQ(AUDIO_THREAD_NUM, loadCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM, unloadCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcGetVersionMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = IAudioManagerGet(false);
    ASSERT_NE(manager, nullptr);

    std::atomic<int32_t> successCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&successCnt, manager]() {
            uint32_t majorVer = 0;
            uint32_t minorVer = 0;
            if (manager->GetVersion(manager, &majorVer, &minorVer) == HDF_SUCCESS) {
                successCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    IAudioManagerRelease(manager, false);

    EXPECT_EQ(AUDIO_THREAD_NUM, successCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcLoadAdapterRepeatMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = nullptr;
    struct AudioAdapterDescriptor *descs = nullptr;
    uint32_t adapterNum = 0;
    ASSERT_EQ(HDF_SUCCESS, PrepareAdapterManager(&manager, &descs, &adapterNum));

    std::atomic<int32_t> loadCnt(0);
    std::atomic<int32_t> unloadCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&loadCnt, &unloadCnt, manager, descs]() {
            LoadAdapterRepeatWorker(manager, descs, loadCnt, unloadCnt);
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
    IAudioManagerRelease(manager, false);

    EXPECT_EQ(AUDIO_THREAD_NUM * AUDIO_LOAD_REPEAT_TIMES, loadCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM * AUDIO_LOAD_REPEAT_TIMES, unloadCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcGetAllAdaptersLoadMixedMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = nullptr;
    struct AudioAdapterDescriptor *descs = nullptr;
    uint32_t adapterNum = 0;
    ASSERT_EQ(HDF_SUCCESS, PrepareAdapterManager(&manager, &descs, &adapterNum));

    std::atomic<int32_t> queryCnt(0);
    std::atomic<int32_t> loadCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([i, &queryCnt, &loadCnt, manager, descs]() {
            if (i % 2 == 0) {
                // query group: each thread queries with its own descriptor array
                QueryAdaptersWorker(manager, queryCnt);
            } else {
                // load group: load and unload the same adapter concurrently
                LoadAdapterOnceWorker(manager, descs, loadCnt);
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
    IAudioManagerRelease(manager, false);

    EXPECT_EQ(AUDIO_THREAD_NUM / 2, queryCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM / 2, loadCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcLoadAdapterDiffDescMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = nullptr;
    struct AudioAdapterDescriptor *descs = nullptr;
    uint32_t adapterNum = 0;
    ASSERT_EQ(HDF_SUCCESS, PrepareAdapterManager(&manager, &descs, &adapterNum));

    std::atomic<int32_t> loadCnt(0);
    std::atomic<int32_t> unloadCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([i, adapterNum, &loadCnt, &unloadCnt, manager, descs]() {
            struct AudioAdapterDescriptor *desc = &descs[i % adapterNum];
            struct IAudioAdapter *adapter = nullptr;
            if (manager->LoadAdapter(manager, desc, &adapter) != HDF_SUCCESS) {
                return;
            }
            loadCnt++;
            if (manager->UnloadAdapter(manager, desc->adapterName) == HDF_SUCCESS) {
                unloadCnt++;
            }
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
    IAudioManagerRelease(manager, false);

    EXPECT_EQ(AUDIO_THREAD_NUM, loadCnt.load());
    EXPECT_EQ(AUDIO_THREAD_NUM, unloadCnt.load());
}

HWTEST_F(HdfAudioConcManagerTest, HdfAudioManagerConcUnloadInvalidAdapterMutiThread001, TestSize.Level1)
{
    struct IAudioManager *manager = IAudioManagerGet(false);
    ASSERT_NE(manager, nullptr);

    std::atomic<int32_t> finishedCnt(0);
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < AUDIO_THREAD_NUM; i++) {
        threads.push_back(std::thread([&finishedCnt, manager]() {
            // unload an adapter which was never loaded, service must stay alive
            (void)manager->UnloadAdapter(manager, AUDIO_INVALID_NAME);
            finishedCnt++;
        }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(AUDIO_THREAD_NUM, finishedCnt.load());

    // the manager service must still work after invalid concurrent unload
    uint32_t size = AUDIO_ADAPTER_NUM_MAX;
    struct AudioAdapterDescriptor *descs = (struct AudioAdapterDescriptor *)OsalMemCalloc(
        sizeof(struct AudioAdapterDescriptor) * (AUDIO_ADAPTER_NUM_MAX));
    ASSERT_NE(descs, nullptr);
    EXPECT_EQ(HDF_SUCCESS, manager->GetAllAdapters(manager, descs, &size));
    ReleaseAdapterDescs(&descs, AUDIO_ADAPTER_NUM_MAX);
    IAudioManagerRelease(manager, false);
}

} // namespace
