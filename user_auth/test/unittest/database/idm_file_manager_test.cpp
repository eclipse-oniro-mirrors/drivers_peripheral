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

#include <gtest/gtest.h>

#include "buffer.h"
#include "idm_common.h"
#include "idm_file_manager.h"

extern "C" {
    extern ResultCode CapacityExpansion(Buffer *object, uint32_t targetCapacity);
    extern ResultCode StreamWrite(Buffer *parcel, void *from, uint32_t size);
    extern ResultCode StreamWriteEnrolledInfo(Buffer *parcel, LinkedList *enrolledList);
    extern ResultCode StreamWriteCredentialList(Buffer *parcel, LinkedList *credentialList);
    extern ResultCode StreamWriteUserInfo(Buffer *parcel, UserInfo *userInfo);
    extern ResultCode StreamRead(Buffer *parcel, uint32_t *index, void *to, uint32_t size);
    extern ResultCode StreamReadCredentialList(Buffer *parcel, uint32_t *index, LinkedList *credentialList);
    extern ResultCode StreamReadEnrolledList(Buffer *parcel, uint32_t *index, LinkedList *enrolledList);
}

namespace OHOS {
namespace UserIam {
namespace UserAuth {
using namespace testing;
using namespace testing::ext;

namespace {
    const uint32_t MAX_BUFFER_LEN = 512000;
} // namespace

class IdmFileMgrTest : public testing::Test {
public:
    static void SetUpTestCase() {};

    static void TearDownTestCase() {};

    void SetUp() {};

    void TearDown() {};
};

HWTEST_F(IdmFileMgrTest, TestCapacityExpansion_001, TestSize.Level0)
{
    EXPECT_EQ(CapacityExpansion(nullptr, 0), RESULT_BAD_PARAM);

    uint32_t bufferSize = 10;
    Buffer *object = CreateBufferBySize(bufferSize);
    object->maxSize = MAX_BUFFER_LEN;
    EXPECT_EQ(CapacityExpansion(object, 0), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestCapacityExpansion_002, TestSize.Level0)
{
    uint32_t bufferSize = 10;
    Buffer *object = CreateBufferBySize(bufferSize);
    EXPECT_EQ(CapacityExpansion(object, 0), RESULT_SUCCESS);
}

HWTEST_F(IdmFileMgrTest, TestCapacityExpansion_003, TestSize.Level0)
{
    uint32_t bufferSize = 10;
    Buffer *object = CreateBufferBySize(bufferSize);
    uint32_t targetCapacity = 5120000;
    EXPECT_EQ(CapacityExpansion(object, targetCapacity), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestStreamWrite_001, TestSize.Level0)
{
    EXPECT_EQ(StreamWrite(nullptr, nullptr, 0), RESULT_BAD_PARAM);

    uint32_t bufferSize = 10;
    Buffer *parcel = CreateBufferBySize(bufferSize);
    uint32_t from = 0;
    uint32_t objectSize = 5120000;
    EXPECT_EQ(StreamWrite(parcel, static_cast<void *>(&from), objectSize), RESULT_BAD_PARAM);

    objectSize = 15;
    uint8_t array[objectSize];
    EXPECT_EQ(StreamWrite(parcel, static_cast<void *>(&array), objectSize), RESULT_SUCCESS);
}

HWTEST_F(IdmFileMgrTest, TestStreamWrite_002, TestSize.Level0)
{
    uint32_t bufferSize = 10;
    Buffer *parcel = CreateBufferBySize(bufferSize);
    uint32_t objectSize = 5;

    uint8_t array[objectSize];
    EXPECT_EQ(StreamWrite(parcel, static_cast<void *>(&array), objectSize), RESULT_SUCCESS);
}

HWTEST_F(IdmFileMgrTest, TestStreamWriteEnrolledInfo, TestSize.Level0)
{
    EXPECT_EQ(StreamWriteEnrolledInfo(nullptr, nullptr), RESULT_BAD_PARAM);

    uint32_t bufferSize = 10;
    Buffer *parcel = CreateBufferBySize(bufferSize);
    LinkedList *enrolledList = CreateLinkedList(DestroyEnrolledNode);
    EXPECT_NE(enrolledList, nullptr);
    EXPECT_EQ(StreamWriteEnrolledInfo(parcel, enrolledList), RESULT_SUCCESS);

    enrolledList->insert(enrolledList, nullptr);
    EXPECT_EQ(StreamWriteEnrolledInfo(parcel, enrolledList), RESULT_GENERAL_ERROR);
}

HWTEST_F(IdmFileMgrTest, TestStreamWriteCredentialList, TestSize.Level0)
{
    EXPECT_EQ(StreamWriteCredentialList(nullptr, nullptr), RESULT_BAD_PARAM);

    uint32_t bufferSize = 10;
    Buffer *parcel = CreateBufferBySize(bufferSize);
    LinkedList *credentialList = CreateLinkedList(DestroyCredentialNode);
    EXPECT_NE(credentialList, nullptr);
    EXPECT_EQ(StreamWriteCredentialList(parcel, credentialList), RESULT_SUCCESS);

    credentialList->insert(credentialList, nullptr);
    EXPECT_EQ(StreamWriteCredentialList(parcel, credentialList), RESULT_GENERAL_ERROR);
}

HWTEST_F(IdmFileMgrTest, TestStreamWriteUserInfo, TestSize.Level0)
{
    EXPECT_EQ(StreamWriteUserInfo(nullptr, nullptr), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestUpdateFileInfo, TestSize.Level0)
{
    EXPECT_EQ(UpdateFileInfo(nullptr), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestStreamRead, TestSize.Level0)
{
    uint32_t bufferSize = 10;
    Buffer *parcel = CreateBufferBySize(bufferSize);
    uint32_t index = 20;
    uint32_t size = 1000;
    EXPECT_EQ(StreamRead(parcel, &index, nullptr, size), RESULT_BAD_PARAM);

    parcel->contentSize = 200;
    EXPECT_EQ(StreamRead(parcel, &index, nullptr, size), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestStreamReadCredentialList, TestSize.Level0)
{
    EXPECT_EQ(StreamReadCredentialList(nullptr, nullptr, nullptr), RESULT_BAD_PARAM);
}

HWTEST_F(IdmFileMgrTest, TestStreamReadEnrolledList, TestSize.Level0)
{
    EXPECT_EQ(StreamReadEnrolledList(nullptr, nullptr, nullptr), RESULT_BAD_PARAM);
}
} // namespace UserAuth
} // namespace UserIam
} // namespace OHOS