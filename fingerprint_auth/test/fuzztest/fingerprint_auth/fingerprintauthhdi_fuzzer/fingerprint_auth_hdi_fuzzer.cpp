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

#include "fingerprint_auth_hdi_fuzzer.h"

#include <cstddef>
#include <cstdint>

#include "parcel.h"

#include "iam_fuzz_test.h"
#include "iam_logger.h"
#include "iam_fuzz_test.h"

#include "executor_impl.h"

#define LOG_LABEL OHOS::UserIam::Common::LABEL_FINGERPRINT_AUTH_HDI

#undef private

using namespace std;
using namespace OHOS::UserIam::Common;

namespace OHOS {
namespace HDI {
namespace FingerprintAuth {
namespace {
class DummyIExecutorCallback : public IExecutorCallback {
public:
    DummyIExecutorCallback(int32_t result, int32_t tip) : result_(result), tip_(tip)
    {
    }

    int32_t OnResult(int32_t result, const std::vector<uint8_t> &extraInfo) override
    {
        IAM_LOGI("result %{public}d extraInfo len %{public}zu", result, extraInfo.size());
        return result_;
    }

    int32_t OnTip(int32_t tip, const std::vector<uint8_t> &extraInfo) override
    {
        IAM_LOGI("tip %{public}d extraInfo len %{public}zu", tip, extraInfo.size());
        return tip_;
    }

private:
    int32_t result_;
    int32_t tip_;
};

class DummyISaCommandCallback : public ISaCommandCallback {
public:
    explicit DummyISaCommandCallback(int32_t result) : result_(result)
    {
    }

    int32_t OnSaCommands(const std::vector<SaCommand> &commands) override
    {
        return result_;
    }

private:
    int32_t result_;
};

ExecutorImpl g_executorImpl;

void FillFuzzExecutorInfo(Parcel &parcel, ExecutorInfo &executorInfo)
{
    executorInfo.sensorId = parcel.ReadUint16();
    executorInfo.executorType = parcel.ReadUint32();
    executorInfo.executorRole = static_cast<ExecutorRole>(parcel.ReadInt32());
    executorInfo.authType = static_cast<AuthType>(parcel.ReadInt32());
    executorInfo.esl = static_cast<ExecutorSecureLevel>(parcel.ReadInt32());
    FillFuzzUint8Vector(parcel, executorInfo.publicKey);
    FillFuzzUint8Vector(parcel, executorInfo.extraInfo);
    IAM_LOGI("success");
}

void FillFuzzTemplateInfo(Parcel &parcel, TemplateInfo &templateInfo)
{
    templateInfo.executorType = parcel.ReadUint32();
    templateInfo.lockoutDuration = parcel.ReadInt32();
    templateInfo.remainAttempts = parcel.ReadInt32();
    FillFuzzUint8Vector(parcel, templateInfo.extraInfo);
    IAM_LOGI("success");
}

void FillFuzzIExecutorCallback(Parcel &parcel, sptr<IExecutorCallback> &callbackObj)
{
    bool isNull = parcel.ReadBool();
    if (isNull) {
        callbackObj = nullptr;
    } else {
        callbackObj = new (std::nothrow) DummyIExecutorCallback(parcel.ReadInt32(), parcel.ReadInt32());
        if (callbackObj == nullptr) {
            IAM_LOGE("callbackObj construct fail");
        }
    }
    IAM_LOGI("success");
}

void FillFuzzISaCommandCallback(Parcel &parcel, sptr<ISaCommandCallback> &callbackObj)
{
    bool isNull = parcel.ReadBool();
    if (isNull) {
        callbackObj = nullptr;
    } else {
        callbackObj = new (std::nothrow) DummyISaCommandCallback(parcel.ReadInt32());
        if (callbackObj == nullptr) {
            IAM_LOGE("callbackObj construct fail");
        }
    }
    IAM_LOGI("success");
}

void FillFuzzGetPropertyTypeVector(Parcel &parcel, std::vector<GetPropertyType> &types)
{
    std::vector<uint32_t> propertyTypeUint32;
    FillFuzzUint32Vector(parcel, propertyTypeUint32);
    for (const auto& val : propertyTypeUint32) {
        types.push_back(static_cast<GetPropertyType>(val));
    }

    IAM_LOGI("success");
}

void FillFuzzProperty(Parcel &parcel, Property &property)
{
    property.authSubType = parcel.ReadUint64();
    property.lockoutDuration = parcel.ReadInt32();
    property.remainAttempts = parcel.ReadInt32();
    FillFuzzString(parcel, property.enrollmentProgress);
    FillFuzzString(parcel, property.sensorInfo);

    IAM_LOGI("success");
}

void FuzzGetExecutorInfo(Parcel &parcel)
{
    IAM_LOGI("begin");
    ExecutorInfo executorInfo;
    FillFuzzExecutorInfo(parcel, executorInfo);
    g_executorImpl.GetExecutorInfo(executorInfo);
    IAM_LOGI("end");
}

void FuzzGetTemplateInfo(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t templateId = parcel.ReadUint64();
    TemplateInfo templateInfo;
    FillFuzzTemplateInfo(parcel, templateInfo);
    g_executorImpl.GetTemplateInfo(templateId, templateInfo);
    IAM_LOGI("end");
}

void FuzzOnRegisterFinish(Parcel &parcel)
{
    IAM_LOGI("begin");
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);
    std::vector<uint8_t> frameworkPublicKey;
    FillFuzzUint8Vector(parcel, frameworkPublicKey);
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    g_executorImpl.OnRegisterFinish(templateIdList, frameworkPublicKey, extraInfo);
    IAM_LOGI("end");
}

void FuzzEnroll(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t scheduleId = parcel.ReadUint64();
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    sptr<IExecutorCallback> callbackObj;
    FillFuzzIExecutorCallback(parcel, callbackObj);
    g_executorImpl.Enroll(scheduleId, extraInfo, callbackObj);
    IAM_LOGI("end");
}

void FuzzAuthenticate(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t scheduleId = parcel.ReadUint64();
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    sptr<IExecutorCallback> callbackObj;
    FillFuzzIExecutorCallback(parcel, callbackObj);
    g_executorImpl.Authenticate(scheduleId, templateIdList, extraInfo, callbackObj);
    IAM_LOGI("end");
}

void FuzzAuthenticateV1_1(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t scheduleId = parcel.ReadUint64();
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);
    bool endAfterFirstFail = parcel.ReadBool();
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    sptr<IExecutorCallback> callbackObj;
    FillFuzzIExecutorCallback(parcel, callbackObj);
    g_executorImpl.AuthenticateV1_1(scheduleId, templateIdList, endAfterFirstFail, extraInfo, callbackObj);
    IAM_LOGI("end");
}

void FuzzIdentify(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t scheduleId = parcel.ReadUint64();
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    sptr<IExecutorCallback> callbackObj;
    FillFuzzIExecutorCallback(parcel, callbackObj);
    g_executorImpl.Identify(scheduleId, extraInfo, callbackObj);
    IAM_LOGI("end");
}

void FuzzDelete(Parcel &parcel)
{
    IAM_LOGI("begin");
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);
    g_executorImpl.Delete(templateIdList);
    IAM_LOGI("end");
}

void FuzzCancel(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint64_t scheduleId = parcel.ReadUint64();
    g_executorImpl.Cancel(scheduleId);
    IAM_LOGI("end");
}

void FuzzSendCommand(Parcel &parcel)
{
    IAM_LOGI("begin");
    uint8_t commandId = parcel.ReadUint8();
    std::vector<uint8_t> extraInfo;
    FillFuzzUint8Vector(parcel, extraInfo);
    sptr<IExecutorCallback> callbackObj;
    FillFuzzIExecutorCallback(parcel, callbackObj);
    g_executorImpl.SendCommand(commandId, extraInfo, callbackObj);
    IAM_LOGI("end");
}


void FuzzGetProperty(Parcel &parcel)
{
    IAM_LOGI("begin");
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);
    std::vector<GetPropertyType> propertyTypes;
    FillFuzzGetPropertyTypeVector(parcel, propertyTypes);
    Property property;
    FillFuzzProperty(parcel, property);

    g_executorImpl.GetProperty(templateIdList, propertyTypes, property);
    IAM_LOGI("end");
}

void FuzzSetCachedTemplates(Parcel &parcel)
{
    IAM_LOGI("begin");
    std::vector<uint64_t> templateIdList;
    FillFuzzUint64Vector(parcel, templateIdList);

    g_executorImpl.SetCachedTemplates(templateIdList);
    IAM_LOGI("end");
}

void FuzzRegisterSaCommandCallback(Parcel &parcel)
{
    IAM_LOGI("begin");
    sptr<ISaCommandCallback> callbackObj = nullptr;
    FillFuzzISaCommandCallback(parcel, callbackObj);

    g_executorImpl.RegisterSaCommandCallback(callbackObj);
    IAM_LOGI("end");
}

using FuzzFunc = decltype(FuzzGetExecutorInfo);
FuzzFunc *g_fuzzFuncs[] = {
    FuzzGetExecutorInfo,
    FuzzGetTemplateInfo,
    FuzzOnRegisterFinish,
    FuzzEnroll,
    FuzzAuthenticate,
    FuzzAuthenticateV1_1,
    FuzzIdentify,
    FuzzDelete,
    FuzzCancel,
    FuzzSendCommand,
    FuzzGetProperty,
    FuzzSetCachedTemplates,
    FuzzRegisterSaCommandCallback,
};

void FingerprintAuthHdiFuzzTest(const uint8_t *data, size_t size)
{
    Parcel parcel;
    parcel.WriteBuffer(data, size);
    parcel.RewindRead(0);
    uint32_t index = parcel.ReadUint32() % (sizeof(g_fuzzFuncs) / sizeof(FuzzFunc *));
    auto fuzzFunc = g_fuzzFuncs[index];
    fuzzFunc(parcel);
    return;
}
} // namespace
} // namespace FingerprintAuth
} // namespace HDI
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::HDI::FingerprintAuth::FingerprintAuthHdiFuzzTest(data, size);
    return 0;
}
