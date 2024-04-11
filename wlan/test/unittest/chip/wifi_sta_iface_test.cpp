/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <hdf_log.h>
#include "../../../chip/hdi_service/wifi_sta_iface.h"

using namespace testing::ext;
using namespace OHOS::HDI::Wlan::Chip::V1_0;

namespace WifiStaIfaceTest {
class WifiStaIfaceTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp()
    {
        std::string ifname = "sta";
        std::weak_ptr<IfaceTool> ifaceTool = std::make_shared<IfaceTool>();
        WifiHalFn fn;
        std::weak_ptr<WifiVendorHal> vendorHal = std::make_shared<WifiVendorHal>(ifaceTool, fn, true);
        staIface = new (std::nothrow) WifiStaIface(ifname, vendorHal);
    }
    void TearDown()
    {
        delete staIface;
        if (staIface != nullptr) {
            staIface = nullptr;
        }
    }

public:
    sptr<WifiStaIface> staIface;
};

/**
 * @tc.name: WifiStaIfaceTest001
 * @tc.desc: wifiStaIface
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(WifiStaIfaceTest, WifiStaIfaceTest001, TestSize.Level1)
{
    HDF_LOGI("WifiStaIfaceTest001 start");
    if (staIface == nullptr) {
        return;
    }
    staIface->Invalidate();
    EXPECT_FALSE(staIface->IsValid());
    EXPECT_TRUE(staIface->IsValid());
    EXPECT_TRUE(staIface->GetName() == "sta");
    IfaceType type = IfaceType::AP;
    EXPECT_TRUE(staIface->GetIfaceType(type) == HDF_SUCCESS);
    EXPECT_TRUE(type == IfaceType::STA);
    std::string name = "test";
    EXPECT_TRUE(staIface->GetIfaceName(name) == HDF_SUCCESS);
    EXPECT_TRUE(name == "sta");
}

/**
 * @tc.name: wifistaIfaceTest
 * @tc.desc: wifistaIface
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(WifiStaIfaceTest, wifistaIfaceTest, TestSize.Level1)
{
    if (staIface == nullptr) {
        return;
    }
    BandType band = BandType::TYPE_5GHZ;
    std::vector<uint32_t>frequencies;
    EXPECT_TRUE(staIface->GetSupportFreqs(band, frequencies) == HDF_SUCCESS);
    band = BandType::UNSPECIFIED;
    EXPECT_TRUE(staIface->GetSupportFreqs(band, frequencies) == HDF_FAILURE);
}
}