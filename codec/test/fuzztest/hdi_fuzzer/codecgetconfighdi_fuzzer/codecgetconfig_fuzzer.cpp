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

#include "codecgetconfig_fuzzer.h"
#include "codeccommon_fuzzer.h"

#include <securec.h>

namespace {
    struct AllParameters {
        uint32_t index;
        int8_t *cfgStruct;
        uint32_t cfgStructLen;
    };
}

namespace OHOS {
namespace Codec {
    bool CodecGetConfig(const uint8_t *data, size_t size)
    {
        struct AllParameters params;
        if (data == nullptr) {
            return false;
        }

        uint8_t *rawData = const_cast<uint8_t *>(data);
        params.index = Convert2Uint32(rawData);
        if (size > sizeof(uint32_t) + sizeof(int8_t *)) {
            rawData = rawData + sizeof(uint32_t);
            size = size - sizeof(uint32_t);
            params.cfgStruct = reinterpret_cast<int8_t *>(rawData);
            params.cfgStructLen = size;
        } else {
            params.cfgStruct = reinterpret_cast<int8_t *>(rawData);
            params.cfgStructLen = size;
        }

        bool result = Preconditions();
        if (!result) {
            HDF_LOGE("%{public}s: Preconditions failed\n", __func__);
            return false;
        }

        std::vector<int8_t> inCfgStruct, outCfgStruct;
        ObjectToVector(params.cfgStruct, inCfgStruct);

        int32_t ret = g_component->GetConfig(params.index, inCfgStruct, outCfgStruct);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%{public}s: GetConfig failed, ret is [%{public}x]\n", __func__, ret);
        }

        result = Destroy();
        if (!result) {
            HDF_LOGE("%{public}s: Destroy failed\n", __func__);
            return false;
        }

        return true;
    }
} // namespace codec
} // namespace OHOS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::Codec::CodecGetConfig(data, size);
    return 0;
}
