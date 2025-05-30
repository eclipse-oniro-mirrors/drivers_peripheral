
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
#include "agnss_event_callback_mock.h"

#include <hdf_base.h>

namespace OHOS {
namespace HDI {
namespace Location {
namespace Agnss {
namespace V2_0 {

int32_t AgnssEventCallbackMock::RequestSetUpAgnssDataLink(const AGnssDataLinkRequest& request)
{
    return HDF_SUCCESS;
}

int32_t AgnssEventCallbackMock::RequestSubscriberSetId(SubscriberSetIdType type)
{
    return HDF_SUCCESS;
}

int32_t AgnssEventCallbackMock::RequestAgnssRefInfo(AGnssRefInfoType type)
{
    return HDF_SUCCESS;
}


} // V2_0
} // Gnss
} // Location
} // HDI
} // OHOS