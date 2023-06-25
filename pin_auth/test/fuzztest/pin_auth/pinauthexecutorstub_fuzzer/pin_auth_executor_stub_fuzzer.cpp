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

#include "pin_auth_executor_stub_fuzzer.h"
#include "parcel.h"
#include "iam_logger.h"
#include "pin_auth_hdi.h"
#include "executor_impl.h"
#include "v1_1/executor_service.h"
#include "v1_1/executor_stub.h"

#define LOG_LABEL OHOS::UserIam::Common::LABEL_PIN_AUTH_HDI

#undef private

using namespace std;
using namespace OHOS::UserIam::Common;

namespace OHOS {
namespace HDI {
namespace PinAuth {
namespace {
constexpr uint32_t PIN_AUTH_EXECUTOR_CODE_MIN = 0;
constexpr uint32_t PIN_AUTH_EXECUTOR_CODE_MAX = 11;
constexpr uint32_t PIN_AUTH_EXECUTOR_CODE_MIN_V1_1 = 10;
const std::u16string PIN_AUTH_EXECUTOR_TOKEN_V1_0 = u"ohos.hdi.pin_auth.v1_0.IExecutor";
const std::u16string PIN_AUTH_EXECUTOR_TOKEN_V1_1 = u"ohos.hdi.pin_auth.v1_1.IExecutor";
bool PinAuthExecutorStubFuzzTest(const uint8_t *rawData, size_t size)
{
    IAM_LOGI("start");
    if (rawData == nullptr) {
        return false;
    }
    std::shared_ptr<OHOS::UserIam::PinAuth::PinAuth> pinHdi = std::make_shared<OHOS::UserIam::PinAuth::PinAuth>();
    ExecutorImpl *impl = new (std::nothrow) ExecutorImpl(pinHdi);
    if (impl == nullptr) {
        IAM_LOGE("%{public}s:get serviceImpl failed.", __func__);
        return false;
    }
    sptr<OHOS::HDI::PinAuth::V1_1::ExecutorStub> executorStub =
        new OHOS::HDI::PinAuth::V1_1::ExecutorStub(impl);
    if (executorStub == nullptr) {
        IAM_LOGE("%{public}s:new executorStub failed.", __func__);
        return false;
    }
    for (uint32_t code = PIN_AUTH_EXECUTOR_CODE_MIN; code <= PIN_AUTH_EXECUTOR_CODE_MAX; code++) {
        MessageParcel data;
        MessageParcel reply;
        MessageOption optionSync = MessageOption::TF_SYNC;
        MessageOption optionAsync = MessageOption::TF_ASYNC;
        std::u16string pin_auth_executor_token;
        if (code < PIN_AUTH_EXECUTOR_CODE_MIN_V1_1) {
            pin_auth_executor_token = PIN_AUTH_EXECUTOR_TOKEN_V1_0;
        } else {
            pin_auth_executor_token = PIN_AUTH_EXECUTOR_TOKEN_V1_1;
        }
        // Sync
        data.WriteInterfaceToken(pin_auth_executor_token);
        data.WriteBuffer(rawData, size);
        data.RewindRead(0);
        (void)executorStub->OnRemoteRequest(code, data, reply, optionSync);
        // Async
        data.WriteInterfaceToken(pin_auth_executor_token);
        data.WriteBuffer(rawData, size);
        data.RewindRead(0);
        (void)executorStub->OnRemoteRequest(code, data, reply, optionAsync);
    }
    return true;
}
} // namespace
} // namespace PinAuth
} // namespace UserIam
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::HDI::PinAuth::PinAuthExecutorStubFuzzTest(data, size);
    return 0;
}
