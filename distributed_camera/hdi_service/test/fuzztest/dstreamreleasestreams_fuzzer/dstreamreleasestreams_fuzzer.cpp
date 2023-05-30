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

#include "dstreamreleasestreams_fuzzer.h"

#include <cstddef>
#include <cstdint>

#include "dstream_operator.h"
#include "v1_0/dcamera_types.h"

namespace OHOS {
namespace DistributedHardware {
void DstreamReleaseStreamsFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size < sizeof(int32_t))) {
        return;
    }
    std::vector<int32_t> streamIds;
    streamIds.push_back(*(reinterpret_cast<const int*>(data)));

    std::string abilityInfo(reinterpret_cast<const char*>(data), size);
    std::shared_ptr<DMetadataProcessor> dMetadataProcessor = std::make_shared<DMetadataProcessor>();
    dMetadataProcessor->InitDCameraAbility(abilityInfo);
    OHOS::sptr<DStreamOperator> dCameraStreamOperator = new (std::nothrow) DStreamOperator(dMetadataProcessor);

    dCameraStreamOperator->ReleaseStreams(streamIds);
}
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DistributedHardware::DstreamReleaseStreamsFuzzTest(data, size);
    return 0;
}

