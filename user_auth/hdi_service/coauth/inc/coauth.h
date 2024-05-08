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

#ifndef USERIAM_COAUTH_H
#define USERIAM_COAUTH_H

#include "c_array.h"
#include "defines.h"
#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_SESSION_ID 0
#define MAX_EXECUTOR_SIZE 2
#define MAX_SCHEDULE_NUM 51

typedef union AssociateId {
    uint64_t contextId;
    int32_t userId;
} AssociateId;

typedef struct ScheduleParam {
    uint32_t authType;
    ScheduleMode scheduleMode;
    AssociateId associateId;
    Uint64Array *templateIds;
    uint32_t collectorSensorHint;
    uint32_t verifierSensorHint;
    uint32_t executorMatcher;
    int32_t userType;
} ScheduleParam;

typedef struct CoAuthSchedule {
    uint64_t scheduleId;
    uint32_t authType;
    ScheduleMode scheduleMode;
    AssociateId associateId;
    Uint64Array templateIds;
    uint32_t executorSize;
    ExecutorInfoHal executors[MAX_EXECUTOR_SIZE];
    int32_t userType;
} CoAuthSchedule;

ResultCode InitCoAuth(void);
void DestoryCoAuth(void);

CoAuthSchedule *GenerateSchedule(const ScheduleParam *param);

ResultCode AddCoAuthSchedule(const CoAuthSchedule *coAuthSchedule);
ResultCode RemoveCoAuthSchedule(uint64_t scheduleId);
const CoAuthSchedule *GetCoAuthSchedule(uint64_t scheduleId);
uint32_t GetScheduleVeriferSensorHint(const CoAuthSchedule *coAuthSchedule);
void DestroyCoAuthSchedule(CoAuthSchedule *coAuthSchedule);
CoAuthSchedule *CopyCoAuthSchedule(const CoAuthSchedule *coAuthSchedule);
void DestroyScheduleNode(void *data);
bool IsTemplateArraysValid(const Uint64Array *templateIds);
ResultCode CopyTemplateArrays(const Uint64Array *in, Uint64Array *out);

#ifdef __cplusplus
}
#endif

#endif
