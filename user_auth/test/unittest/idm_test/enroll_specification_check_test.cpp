/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "securec.h"

#include "defines.h"
#include "adaptor_time.h"
#include "adaptor_memory.h"
#include "enroll_specification_check.h"
#include "idm_common.h"

extern "C" {
    extern struct SessionInfo {
        int32_t userId;
        uint32_t authType;
        uint64_t time;
        uint64_t validAuthTokenTime;
        uint8_t challenge[CHALLENGE_LEN];
        uint64_t scheduleId;
        bool isUpdate;
        bool isScheduleValid;
    } *g_session;
    extern LinkedList *g_userInfoList;
    extern UserInfo *g_currentUser;
    extern ResultCode GenerateChallenge(uint8_t *challenge, uint32_t challengeLen);
    extern ResultCode UserAuthTokenSign(UserAuthTokenHal *userAuthToken);
    extern ResultCode GetTokenDataCipherResult(const TokenDataToEncrypt *data, UserAuthTokenHal *authToken);
}

namespace OHOS {
namespace UserIam {
namespace UserAuth {
using namespace testing;
using namespace testing::ext;

class EnrollCheckTest : public testing::Test {
public:
    static void SetUpTestCase() {};

    static void TearDownTestCase() {};

    void SetUp() {};

    void TearDown() {};
};

#define GENERATE_TOKEN(dataToEncrypt, userAuthTokenHal) \
{ \
    EXPECT_EQ(GetTokenDataCipherResult(&(dataToEncrypt), &(userAuthTokenHal)), RESULT_SUCCESS); \
    EXPECT_EQ(UserAuthTokenSign(&(userAuthTokenHal)), RESULT_SUCCESS); \
}

HWTEST_F(EnrollCheckTest, TestCheckIdmOperationToken_001, TestSize.Level0)
{
    g_session = nullptr;
    EXPECT_EQ(CheckIdmOperationToken(0, nullptr), RESULT_BAD_PARAM);
    UserAuthTokenHal token = {};
    token.tokenDataPlain.authType = FACE_AUTH;
    EXPECT_EQ(CheckIdmOperationToken(0, &token), RESULT_BAD_MATCH);
    TokenDataToEncrypt data = {};
    token.tokenDataPlain.time = GetSystemTime();
    GENERATE_TOKEN(data, token);
    EXPECT_EQ(CheckIdmOperationToken(0, &token), RESULT_VERIFY_TOKEN_FAIL);
    token.tokenDataPlain.authType = PIN_AUTH;
    GENERATE_TOKEN(data, token);
    EXPECT_EQ(CheckIdmOperationToken(0, &token), RESULT_BAD_MATCH);
}

HWTEST_F(EnrollCheckTest, TestCheckIdmOperationToken_002, TestSize.Level0)
{
    g_userInfoList = nullptr;
    struct SessionInfo session = {};
    session.userId = 2661;
    session.validAuthTokenTime = 100;
    g_session = &session;
    EXPECT_EQ(GenerateChallenge(session.challenge, CHALLENGE_LEN), RESULT_SUCCESS);
    UserAuthTokenHal token = {};
    token.tokenDataPlain.authType = 1;
    token.tokenDataPlain.time = GetSystemTime();
    TokenDataToEncrypt data = {
        .userId = 0,
        .secureUid = 10,
        .enrolledId = 2,
        .credentialId = 3,
    };
    EXPECT_EQ(memcpy_s(token.tokenDataPlain.challenge, CHALLENGE_LEN, session.challenge, CHALLENGE_LEN), EOK);
    GENERATE_TOKEN(data, token);
    EXPECT_EQ(CheckIdmOperationToken(0, &token), RESULT_BAD_MATCH);
    EXPECT_EQ(CheckIdmOperationToken(session.userId, &token), RESULT_BAD_MATCH);

    data.userId = session.userId;
    GENERATE_TOKEN(data, token);
    EXPECT_EQ(CheckIdmOperationToken(session.userId, &token), RESULT_BAD_MATCH);

    g_userInfoList = CreateLinkedList(DestroyUserInfoNode);
    EXPECT_NE(g_userInfoList, nullptr);
    UserInfo *userInfo = static_cast<UserInfo *>(Malloc(sizeof(UserInfo)));
    userInfo->userId = session.userId;
    userInfo->secUid = 20;
    userInfo->credentialInfoList = CreateLinkedList(DestroyCredentialNode);
    userInfo->enrolledInfoList = CreateLinkedList(DestroyEnrolledNode);
    g_userInfoList->insert(g_userInfoList, static_cast<void *>(userInfo));
    EXPECT_EQ(CheckIdmOperationToken(session.userId, &token), RESULT_BAD_MATCH);

    userInfo->secUid = 10;
    EXPECT_EQ(CheckIdmOperationToken(session.userId, &token), RESULT_SUCCESS);
    g_session = nullptr;
    DestroyLinkedList(g_userInfoList);
    g_userInfoList = nullptr;
}

HWTEST_F(EnrollCheckTest, TestCheckSpecification, TestSize.Level0)
{
    g_userInfoList = nullptr;
    g_currentUser = nullptr;
    int32_t userId = 2361;
    uint32_t authType = 1;
    EXPECT_EQ(CheckSpecification(userId, authType), RESULT_UNKNOWN);
}
} // namespace UserAuth
} // namespace UserIam
} // namespace OHOS