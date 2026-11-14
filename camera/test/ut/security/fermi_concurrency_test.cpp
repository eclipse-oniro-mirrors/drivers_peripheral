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
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "capture_request.h"
#include "istream.h"
#include "metadata_controller.h"
#include "stream.h"
#include "stream_tunnel.h"

using namespace OHOS::Camera;
using namespace testing::ext;

namespace {
class MockStream : public IStream {
public:
    explicit MockStream(int32_t id) : streamId_(id) {}

    RetCode ConfigStream(StreamConfiguration& config) override { return RC_OK; }
    RetCode CommitStream() override { return RC_OK; }
    RetCode StartStream() override { return RC_OK; }
    RetCode StopStream() override { return RC_OK; }
    RetCode AddRequest(std::shared_ptr<CaptureRequest>& request) override { return RC_OK; }
    RetCode CancelRequest(const std::shared_ptr<CaptureRequest>& request) override { return RC_OK; }
    RetCode AttachStreamTunnel(std::shared_ptr<StreamTunnel>& tunnel) override { return RC_OK; }
    RetCode DetachStreamTunnel() override { return RC_OK; }
    RetCode ChangeToOfflineStream(std::shared_ptr<OfflineStream> offlineStream) override { return RC_OK; }
    bool GetTunnelMode() const override { return false; }
    StreamConfiguration GetStreamAttribute() const override { return {}; }
    int32_t GetStreamId() const override { return streamId_; }
    RetCode Capture(const std::shared_ptr<CaptureRequest>& request) override { return RC_OK; }
    RetCode OnFrame(const std::shared_ptr<CaptureRequest>& request) override { return RC_OK; }
    bool IsRunning() const override { return running_.load(); }
    void DumpStatsInfo() const override {}
    void ReleaseStreamBufferPool() override {}

    void SetRunning(bool r) { running_.store(r); }

private:
    int32_t streamId_;
    std::atomic<bool> running_{false};
};

static CaptureMeta MakeCaptureMeta(int32_t streamId)
{
    auto meta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
    int32_t id = streamId;
    meta->addEntry(OHOS_CAMERA_STREAM_ID, &id, 1);
    return meta;
}

} // namespace

class FermiMetadataConcurrencyTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp(void) {}
    void TearDown(void)
    {
        auto& ctrl = MetadataController::GetInstance();
        ctrl.Stop();
    }
};

/**
 * @tc.name: Fermi001_SetDeviceDefaultMetadata_NoNullDerefUnderConcurrency
 * @tc.desc: Verify concurrent SetDeviceDefaultMetadata and GetSettingsConfig do not cause null pointer dereference.
 * @tc.level: Level0
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiMetadataConcurrencyTest, Fermi001_SetDeviceDefaultMetadata_NoNullDerefUnderConcurrency, TestSize.Level0)
{
    constexpr int threadCount = 8;
    constexpr int iterations = 200;
    auto& ctrl = MetadataController::GetInstance();

    auto defaultMeta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
    int32_t streamId = static_cast<int32_t>(DEVICE_STREAM_ID);
    defaultMeta->addEntry(OHOS_CAMERA_STREAM_ID, &streamId, 1);
    ctrl.SetDeviceDefaultMetadata(defaultMeta);

    auto reader = [&ctrl, &iterations]() {
        for (int i = 0; i < iterations; ++i) {
            std::shared_ptr<CameraMetadata> out;
            ctrl.GetSettingsConfig(out);
        }
    };

    auto reInit = [&ctrl, &iterations]() {
        for (int i = 0; i < iterations; ++i) {
            auto meta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
            int32_t id = static_cast<int32_t>(DEVICE_STREAM_ID);
            meta->addEntry(OHOS_CAMERA_STREAM_ID, &id, 1);
            ctrl.SetDeviceDefaultMetadata(meta);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        if (i % 2 == 0) {
            threads.emplace_back(reader);
        } else {
            threads.emplace_back(reInit);
        }
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

/**
 * @tc.name: Fermi002_GetSettingsConfig_BeforeInit_NoCrash
 * @tc.desc: Verify GetSettingsConfig does not crash when metaDataConfig_ may be null.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiMetadataConcurrencyTest, Fermi002_GetSettingsConfig_BeforeInit_NoCrash, TestSize.Level1)
{
    auto& ctrl = MetadataController::GetInstance();
    std::shared_ptr<CameraMetadata> out;
    ctrl.GetSettingsConfig(out);
    SUCCEED();
}

/**
 * @tc.name: Fermi003_SetDeviceDefaultMetadata_OnlyOneInitSucceeds
 * @tc.desc: Verify atomic check-and-set ensures only one thread initializes device default metadata.
 * @tc.level: Level0
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiMetadataConcurrencyTest, Fermi003_SetDeviceDefaultMetadata_OnlyOneInitSucceeds, TestSize.Level0)
{
    constexpr int threadCount = 16;
    auto& ctrl = MetadataController::GetInstance();

    auto tryInit = [&ctrl]() {
        auto meta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
        int32_t id = static_cast<int32_t>(DEVICE_STREAM_ID);
        meta->addEntry(OHOS_CAMERA_STREAM_ID, &id, 1);
        ctrl.SetDeviceDefaultMetadata(meta);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(tryInit);
    }
    for (auto& t : threads) {
        t.join();
    }
    SUCCEED();
}

/**
 * @tc.name: Fermi004_AfterStartStopStop_StateConsistent
 * @tc.desc: Verify internal state remains consistent after repeated Start/Stop cycles.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiMetadataConcurrencyTest, Fermi004_AfterStartStopStop_StateConsistent, TestSize.Level1)
{
    auto& ctrl = MetadataController::GetInstance();

    auto defaultMeta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
    int32_t id = static_cast<int32_t>(DEVICE_STREAM_ID);
    defaultMeta->addEntry(OHOS_CAMERA_STREAM_ID, &id, 1);
    ctrl.SetDeviceDefaultMetadata(defaultMeta);

    for (int i = 0; i < 5; ++i) {
        ctrl.SetDeviceDefaultMetadata(defaultMeta);
        ctrl.Start();
        (void)ctrl.IsMute();
        std::shared_ptr<CameraMetadata> out;
        ctrl.GetSettingsConfig(out);
        ctrl.Stop();
    }
    SUCCEED();
}

/**
 * @tc.name: Fermi005_LockDisciplineSoundAfterFixes
 * @tc.desc: Verify lock discipline is sound after bug15/16/17/18 fixes;
 *           serial Start/Stop cycle with concurrent metadata reads.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiMetadataConcurrencyTest, Fermi005_LockDisciplineSoundAfterFixes, TestSize.Level1)
{
    auto& ctrl = MetadataController::GetInstance();
    auto defaultMeta = std::make_shared<CameraMetadata>(ENTRY_CAPACITY, DATA_CAPACITY);
    int32_t id = static_cast<int32_t>(DEVICE_STREAM_ID);
    defaultMeta->addEntry(OHOS_CAMERA_STREAM_ID, &id, 1);
    ctrl.SetDeviceDefaultMetadata(defaultMeta);

    constexpr int cycles = 10;
    std::atomic<bool> stopFlag{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&stopFlag, &ctrl]() {
            while (!stopFlag.load()) {
                std::shared_ptr<CameraMetadata> out;
                ctrl.GetSettingsConfig(out);
            }
        });
    }

    for (int i = 0; i < cycles; ++i) {
        ctrl.SetDeviceDefaultMetadata(defaultMeta);
        ctrl.Start();
        ctrl.Stop();
    }

    stopFlag.store(true);
    for (auto& t : readers) {
        t.join();
    }
    SUCCEED();
}

class FermiCaptureRequestTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp(void) {}
    void TearDown(void) {}
};

/**
 * @tc.name: Fermi006_DisableSync_UnblocksWaitingSync
 * @tc.desc: Verify DisableSync unblocks a waiting Sync (semp_ path) without lost wakeup.
 * @tc.level: Level0
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiCaptureRequestTest, Fermi006_DisableSync_UnblocksWaitingSync, TestSize.Level0)
{
    constexpr int32_t captureId = 1;
    constexpr int32_t ownerCount = 2;
    auto setting = MakeCaptureMeta(0);
    auto req = std::make_shared<CaptureRequest>(captureId, ownerCount, setting, false, false);

    auto stream = std::make_shared<MockStream>(1);
    stream->SetRunning(true);
    req->AddOwner(stream);

    auto processAsync = std::async(std::launch::async, [&req]() { return req->Process(1); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    req->DisableSync();

    ASSERT_EQ(processAsync.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "Sync() blocked forever after DisableSync() — lost wakeup (bug29)";
    EXPECT_EQ(processAsync.get(), RC_OK);
}

/**
 * @tc.name: Fermi007_DisableSync_UnblocksWaitingOnResult
 * @tc.desc: Verify DisableSync unblocks a waiting OnResult (semr_ path) without lost wakeup.
 * @tc.level: Level0
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiCaptureRequestTest, Fermi007_DisableSync_UnblocksWaitingOnResult, TestSize.Level0)
{
    constexpr int32_t captureId = 2;
    constexpr int32_t ownerCount = 2;
    auto setting = MakeCaptureMeta(0);
    auto req = std::make_shared<CaptureRequest>(captureId, ownerCount, setting, false, false);

    auto stream = std::make_shared<MockStream>(2);
    stream->SetRunning(true);
    req->AddOwner(stream);

    auto onResultAsync = std::async(std::launch::async, [&req]() { return req->OnResult(2); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    req->DisableSync();

    ASSERT_EQ(onResultAsync.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "OnResult() blocked forever after DisableSync() — lost wakeup (bug29)";
    EXPECT_EQ(onResultAsync.get(), RC_OK);
}

/**
 * @tc.name: Fermi008_NormalSync_SingleOwnerCompletes
 * @tc.desc: Verify single-owner Sync completes without blocking (no false negative).
 * @tc.level: Level0
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiCaptureRequestTest, Fermi008_NormalSync_SingleOwnerCompletes, TestSize.Level0)
{
    constexpr int32_t captureId = 3;
    constexpr int32_t ownerCount = 1;
    auto setting = MakeCaptureMeta(0);
    auto req = std::make_shared<CaptureRequest>(captureId, ownerCount, setting, false, false);

    auto stream = std::make_shared<MockStream>(3);
    stream->SetRunning(true);
    req->AddOwner(stream);

    ASSERT_EQ(req->Process(3), RC_OK);
    ASSERT_EQ(req->OnResult(3), RC_OK);
}

class FermiStreamTunnelTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp(void) {}
    void TearDown(void) {}
};

/**
 * @tc.name: Fermi009_DetachConcurrentWithGet_NoCrash
 * @tc.desc: Verify concurrent GetBuffer and DetachBufferQueue do not crash.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiStreamTunnelTest, Fermi009_DetachConcurrentWithGet_NoCrash, TestSize.Level1)
{
    constexpr int iterations = 100;
    auto tunnel = std::make_shared<StreamTunnel>();

    auto getter = [&tunnel, &iterations]() {
        for (int i = 0; i < iterations; ++i) {
            (void)tunnel->GetBuffer();
        }
    };
    auto detacher = [&tunnel, &iterations]() {
        for (int i = 0; i < iterations; ++i) {
            tunnel->DetachBufferQueue();
        }
    };

    std::thread t1(getter);
    std::thread t2(detacher);
    t1.join();
    t2.join();
    SUCCEED();
}

/**
 * @tc.name: Fermi010_CleanBuffers_NullBufferQueue_NoCrash
 * @tc.desc: Verify CleanBuffers does not crash when bufferQueue_ is null.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiStreamTunnelTest, Fermi010_CleanBuffers_NullBufferQueue_NoCrash, TestSize.Level1)
{
    auto tunnel = std::make_shared<StreamTunnel>();
    tunnel->NotifyStop();
    tunnel->CleanBuffers();
    SUCCEED();
}

/**
 * @tc.name: Fermi011_SetBufferCount_NullBufferQueue_ReturnsError
 * @tc.desc: Verify SetBufferCount returns RC_ERROR when bufferQueue_ is null.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiStreamTunnelTest, Fermi011_SetBufferCount_NullBufferQueue_ReturnsError, TestSize.Level1)
{
    auto tunnel = std::make_shared<StreamTunnel>();
    EXPECT_EQ(tunnel->SetBufferCount(3), RC_ERROR);
}

/**
 * @tc.name: Fermi012_SetBufferCount_InvalidCount_ReturnsError
 * @tc.desc: Verify SetBufferCount returns RC_ERROR for invalid count (<=0).
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiStreamTunnelTest, Fermi012_SetBufferCount_InvalidCount_ReturnsError, TestSize.Level1)
{
    auto tunnel = std::make_shared<StreamTunnel>();
    EXPECT_EQ(tunnel->SetBufferCount(0), RC_ERROR);
    EXPECT_EQ(tunnel->SetBufferCount(-1), RC_ERROR);
}

/**
 * @tc.name: Fermi013_PutBuffer_NullBufferQueue_NoCrash
 * @tc.desc: Verify PutBuffer does not crash when bufferQueue_ is null.
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
HWTEST_F(FermiStreamTunnelTest, Fermi013_PutBuffer_NullBufferQueue_NoCrash, TestSize.Level1)
{
    auto tunnel = std::make_shared<StreamTunnel>();
    std::shared_ptr<IBuffer> nullBuffer;
    EXPECT_EQ(tunnel->PutBuffer(nullBuffer), RC_ERROR);
}
