/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef WIFI_FEATURE_FLAGS_H
#define WIFI_FEATURE_FLAGS_H

#include "v1_0/ichip_controller.h"
#include "v1_0/chip_types.h"
#include <string>

#define SUBCHIP_PROP "ohos.boot.odm.conn.schiptype"
#define PROP_SUBCHIPTYPE_LEN 10
#define SUPPORT_COEXCHIP ""
#define PROP_MAX_LEN 128

namespace OHOS {
namespace HDI {
namespace Wlan {
namespace Chip {
namespace V1_0 {

namespace chip_mode_ids {
constexpr int32_t K_INVALID = UINT32_MAX;
constexpr int32_t K_V1_STA = 0;
constexpr int32_t K_V1_AP = 1;
constexpr int32_t K_V3 = 3;
}

class WifiChipModes {
public:
    WifiChipModes();
    virtual ~WifiChipModes() = default;
    virtual std::vector<UsableMode> GetChipModes(
        bool isPrimary);

private:
    std::vector<UsableMode> GetChipModesForPrimary();
    std::vector<UsableMode> GetChipModesForTriple();
    UsableMode MakeComModes(int staNum, int apNum, int p2pNum, int modeId);
};
}
}
}
}
}

#endif