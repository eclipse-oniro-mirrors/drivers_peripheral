/*
 * Copyright (C) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef IDM_DATABASE_H
#define IDM_DATABASE_H

#include <stdint.h>

#include "adaptor_memory.h"
#include "defines.h"
#include "idm_common.h"
#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

ResultCode InitUserInfoList(void);
void DestroyUserInfoList(void);
UserInfo *InitUserInfoNode(void);

typedef enum CredentialConditionTag {
    CREDENTIAL_CONDITION_CREDENTIAL_ID = 1,
    CREDENTIAL_CONDITION_AUTH_TYPE = 2, // 1 << 1
    CREDENTIAL_CONDITION_TEMPLATE_ID = 4, // 1 << 2
    CREDENTIAL_CONDITION_SENSOR_HINT = 8, // 1 << 3
    CREDENTIAL_CONDITION_EXECUTOR_MATCHER = 16, // 1 << 4
    CREDENTIAL_CONDITION_USER_ID = 32, // 1 << 5
    CREDENTIAL_CONDITION_NEED_CACHE_PIN = 64, // 1 << 6
    CREDENTIAL_CONDITION_NEED_ABANDON = 128, // 1 << 7
    CREDENTIAL_CONDITION_ABANDON = 256, // 1 << 8
    CREDENTIAL_CONDITION_CACHE_PIN = 512, // 1 << 8
} CredentialConditionTag;

typedef struct {
    uint64_t conditionFactor;
    uint64_t credentialId;
    uint64_t templateId;
    uint32_t authType;
    uint32_t executorSensorHint;
    uint32_t executorMatcher;
    int32_t userId;
} CredentialCondition;

ResultCode GetSecureUid(int32_t userId, uint64_t *secUid);
ResultCode GetEnrolledInfo(int32_t userId, EnrolledInfoHal **enrolledInfos, uint32_t *num);
ResultCode GetEnrolledInfoAuthType(int32_t userId, uint32_t authType, EnrolledInfoHal *enrolledInfo);
ResultCode DeleteUserInfo(int32_t userId, LinkedList **creds);

LinkedList *QueryCredentialLimit(const CredentialCondition *limit);
ResultCode QueryCredentialUserId(uint64_t credentialId, int32_t *userId);

ResultCode AddCredentialInfo(int32_t userId, CredentialInfoHal *credentialInfo, int32_t userType);
ResultCode SetPinSubType(int32_t userId, uint64_t pinSubType);
ResultCode GetPinSubType(int32_t userId, uint64_t *pinSubType);
ResultCode DeleteCredentialInfo(int32_t userId, uint64_t credentialId, CredentialInfoHal *credentialInfo);
void ClearCachePin(int32_t userId);

void SetCredentialConditionCredentialId(CredentialCondition *condition, uint64_t credentialId);
void SetCredentialConditionTemplateId(CredentialCondition *condition, uint64_t templateId);
void SetCredentialConditionAuthType(CredentialCondition *condition, uint32_t authType);
void SetCredentialConditionExecutorSensorHint(CredentialCondition *condition, uint32_t executorSensorHint);
void SetCredentialConditionExecutorMatcher(CredentialCondition *condition, uint32_t executorMatcher);
void SetCredentialConditionUserId(CredentialCondition *condition, int32_t userId);
void SetCredentialConditionNeedCachePin(CredentialCondition *condition);
void SetCredentialConditionNeedAbandonPin(CredentialCondition *condition);
void SetCredentialConditionAbandonPin(CredentialCondition *condition);
void SetCredentialConditionCachePin(CredentialCondition *condition);
ResultCode GetAllExtUserInfo(UserInfoResult *userInfos, uint32_t userInfolen, uint32_t *userInfoCount);
ResultCode GetEnrolledState(int32_t userId, uint32_t authType, EnrolledStateHal *enrolledStateHal);
ResultCode SaveGlobalConfigParam(GlobalConfigParamHal *param);
ResultCode QueryPinExpiredInfo(int64_t *pinExpiredPeriod);
ResultCode GetPinExpiredInfo(int32_t userId, PinExpiredInfo *expiredInfo);
bool GetEnableStatus(int32_t userId, uint32_t authType);
ResultCode QueryCredentialByIdFunc(uint64_t credentialId, LinkedList **creds);
ResultCode QueryAbandonCredential(int32_t userId, LinkedList **creds);
ResultCode GetCredentialListByAuthType(int32_t userId, uint32_t authType, LinkedList **creds);
ResultCode GetCredentialListByCachePin(int32_t userId, LinkedList **creds);
ResultCode GetCredentialListByAbandonFlag(int32_t userId, uint32_t authType, LinkedList **creds);
ResultCode GetCredentialByUserIdAndCredId(int32_t userId, uint64_t credentialId, CredentialInfoHal *credentialInfo);
ResultCode UpdateAbandonResultForReset(int32_t userId, bool *isDelete, CredentialInfoHal *credentialInfo);
ResultCode UpdateAbandonResultForUpdate(int32_t userId, bool *isDelete, CredentialInfoHal *credentialInfo);
bool IsAbandonCredentialExpired(const CredentialInfoHal *credentialInfo);
int64_t CalcCredentialValidPeriod(const CredentialInfoHal *credentialInfo);
int64_t GetCredentialValidPeriod(int32_t userId, uint64_t credentialId);
ResultCode ClearAbandonExpiredCredential(int32_t userId, CredentialInfoHal *credentialInfo);

#ifdef __cplusplus
}
#endif

#endif // IDM_DATABASE_H