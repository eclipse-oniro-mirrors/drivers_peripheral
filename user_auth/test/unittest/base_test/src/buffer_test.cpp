/*
 * Copyright (C) 2022-2023 Huawei Device Co., Ltd.
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

#include "buffer_test.h"

#include "buffer.h"
#include "securec.h"

namespace OHOS {
namespace UserIam {
namespace UserAuth {
using namespace testing;
using namespace testing::ext;
namespace {
    constexpr uint32_t MAX_BUFFER_SIZE = 512000 + 1;
    constexpr uint32_t BUFFER_LEN_1 = 1;
    constexpr uint32_t BUFFER_LEN_2 = 2;
    constexpr uint32_t BUFFER_LEN_4 = 4;
    constexpr uint32_t BUFFER_LEN_5 = 5;
    constexpr uint32_t BUFFER_LEN_6 = 6;
} // namespace

void BufferTest::SetUpTestCase()
{
}

void BufferTest::TearDownTestCase()
{
}

void BufferTest::SetUp()
{
}

void BufferTest::TearDown()
{
}

/**
 * @tc.name: CreateBufferBySize test
 * @tc.desc: verify CreateBufferBySize
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, CreateBufferBySize_test, TestSize.Level0)
{
    Buffer *buffer1 = CreateBufferBySize(0);
    EXPECT_EQ(buffer1, nullptr);
    bool result = IsBufferValid(buffer1);
    EXPECT_EQ(result, false);

    Buffer *buffer2 = CreateBufferBySize(MAX_BUFFER_SIZE);
    EXPECT_EQ(buffer2, nullptr);
    result = IsBufferValid(buffer2);
    EXPECT_EQ(result, false);

    Buffer *buffer3 = CreateBufferBySize(5);
    EXPECT_NE(buffer3, nullptr);
    result = IsBufferValid(buffer3);
    EXPECT_EQ(result, true);
    DestoryBuffer(buffer3);
}

HWTEST_F(BufferTest, GetTmpBuffer_test, TestSize.Level0)
{
    Buffer buffer1 = GetTmpBuffer(nullptr, 0, 0);
    Buffer buffer2 = {
        .buf = nullptr,
        .contentSize = 0,
        .maxSize = 0,
    };
    EXPECT_EQ(memcmp(&buffer1, &buffer2, sizeof(Buffer)), 0);
}

/**
 * @tc.name: CreateBufferByData test
 * @tc.desc: verify CreateBufferByData
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, CreateBufferByData_test, TestSize.Level0)
{
    Buffer *buffer1 = CreateBufferByData(nullptr, 0);
    EXPECT_EQ(buffer1, nullptr);
    bool result = IsBufferValid(buffer1);
    EXPECT_EQ(result, false);

    Buffer *buffer2 = CreateBufferByData(nullptr, MAX_BUFFER_SIZE);
    EXPECT_EQ(buffer2, nullptr);
    result = IsBufferValid(buffer2);
    EXPECT_EQ(result, false);

    Buffer *data = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(data, nullptr);
    (void)memset_s(data->buf, data->maxSize, BUFFER_LEN_1, data->maxSize);
    data->contentSize = data->maxSize;

    Buffer *buffer3 = CreateBufferByData(data->buf, 0);
    EXPECT_EQ(buffer3, nullptr);
    result = IsBufferValid(buffer3);
    EXPECT_EQ(result, false);

    Buffer *buffer4 = CreateBufferByData(data->buf, MAX_BUFFER_SIZE);
    EXPECT_EQ(buffer4, nullptr);
    result = IsBufferValid(buffer4);
    EXPECT_EQ(result, false);

    Buffer *buffer5 = CreateBufferByData(data->buf, data->contentSize);
    EXPECT_NE(buffer5, nullptr);
    result = IsBufferValid(buffer5);
    EXPECT_EQ(result, true);

    DestoryBuffer(buffer5);
    DestoryBuffer(data);
}

/**
 * @tc.name: CheckBufferWithSize test
 * @tc.desc: verify CheckBufferWithSize
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, CheckBufferWithSize_test, TestSize.Level0)
{
    Buffer *buffer1 = CreateBufferByData(nullptr, 0);
    EXPECT_EQ(buffer1, nullptr);

    bool result = CheckBufferWithSize(buffer1, BUFFER_LEN_5);
    EXPECT_EQ(result, false);

    Buffer *data = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(data, nullptr);
    (void)memset_s(data->buf, data->maxSize, BUFFER_LEN_1, data->maxSize);
    data->contentSize = data->maxSize;

    result = CheckBufferWithSize(data, BUFFER_LEN_4);
    EXPECT_EQ(result, false);

    result = CheckBufferWithSize(data, BUFFER_LEN_5);
    EXPECT_EQ(result, true);

    DestoryBuffer(data);
}

/**
 * @tc.name: CopyBuffer test
 * @tc.desc: verify CopyBuffer
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, CopyBuffer_test, TestSize.Level0)
{
    Buffer *buffer0 = CreateBufferBySize(0);
    EXPECT_EQ(buffer0, nullptr);
    Buffer *result = CopyBuffer(buffer0);
    EXPECT_EQ(result, nullptr);

    Buffer *buffer2 = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(buffer2, nullptr);
    result = CopyBuffer(buffer2);
    EXPECT_NE(result, nullptr);
    DestoryBuffer(buffer2);
    DestoryBuffer(result);
}

/**
 * @tc.name: CompareBuffer test
 * @tc.desc: verify CompareBuffer
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, CompareBuffer_test, TestSize.Level0)
{
    bool result = CompareBuffer(nullptr, nullptr);
    EXPECT_EQ(result, false);

    Buffer *buffer1 = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(buffer1, nullptr);
    buffer1->contentSize = buffer1->maxSize;
    Buffer *buffer2 = CreateBufferBySize(BUFFER_LEN_6);
    EXPECT_NE(buffer2, nullptr);
    buffer2->contentSize = buffer2->maxSize;
    result = CompareBuffer(buffer1, buffer2);
    EXPECT_EQ(result, false);
    DestoryBuffer(buffer1);
    DestoryBuffer(buffer2);

    Buffer *buffer3 = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(buffer3, nullptr);
    (void)memset_s(buffer3->buf, buffer3->maxSize, BUFFER_LEN_1, buffer3->maxSize);
    buffer3->contentSize = buffer3->maxSize;

    Buffer *buffer4 = CreateBufferBySize(BUFFER_LEN_6);
    EXPECT_NE(buffer4, nullptr);
    (void)memset_s(buffer4->buf, buffer4->maxSize, BUFFER_LEN_2, buffer4->maxSize);
    buffer4->contentSize = buffer4->maxSize;
    result = CompareBuffer(buffer3, buffer4);
    EXPECT_EQ(result, false);
    DestoryBuffer(buffer3);
    DestoryBuffer(buffer4);

    Buffer *buffer5 = CreateBufferBySize(5);
    EXPECT_NE(buffer5, nullptr);
    (void)memset_s(buffer5->buf, buffer5->maxSize, BUFFER_LEN_1, buffer5->maxSize);
    buffer5->contentSize = buffer5->maxSize;
    result = CompareBuffer(buffer5, buffer5);
    EXPECT_EQ(result, true);
    DestoryBuffer(buffer5);
}

/**
 * @tc.name: GetBufferData test
 * @tc.desc: verify GetBufferData
 * @tc.type: FUNC
 * @tc.require: #I64XCB
 */
HWTEST_F(BufferTest, GetBufferData_test, TestSize.Level0)
{
    Buffer *buffer1 = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(buffer1, nullptr);
    (void)memset_s(buffer1->buf, buffer1->maxSize, BUFFER_LEN_1, buffer1->maxSize);
    buffer1->contentSize = buffer1->maxSize;

    uint32_t result = GetBufferData(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, RESULT_BAD_PARAM);

    result = GetBufferData(buffer1, nullptr, nullptr);
    EXPECT_EQ(result, RESULT_BAD_PARAM);

    Buffer *res = CreateBufferBySize(BUFFER_LEN_4);
    EXPECT_NE(res, nullptr);
    res->contentSize = res->maxSize;

    result = GetBufferData(buffer1, res->buf, &(res->contentSize));
    EXPECT_EQ(result, RESULT_BAD_COPY);
    DestoryBuffer(res);

    Buffer *res1 = CreateBufferBySize(BUFFER_LEN_5);
    EXPECT_NE(res1, nullptr);
    res1->contentSize = res1->maxSize;

    result = GetBufferData(buffer1, res1->buf, &(res1->contentSize));
    EXPECT_EQ(result, RESULT_SUCCESS);
    DestoryBuffer(res1);

    DestoryBuffer(buffer1);
}
} // namespace UserAuth
} // namespace UserIam
} // namespace OHOS
