/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file expected in compliance with the License.
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
#include "camera_flashlight_uttest_v1_2.h"

using namespace OHOS;
using namespace std;
using namespace testing::ext;
using namespace OHOS::Camera;

void CameraFlashlightUtTestV1_2::SetUpTestCase(void) {}
void CameraFlashlightUtTestV1_2::TearDownTestCase(void) {}
void CameraFlashlightUtTestV1_2::SetUp(void)
{
    cameraTest = std::make_shared<OHOS::Camera::Test>();
    cameraTest->Init(); // assert inside
}

void CameraFlashlightUtTestV1_2::TearDown(void)
{
    cameraTest->Close();
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_025
 * @tc.desc:SetCallbackV1_2, Callback object = nullptr;
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_025, TestSize.Level1)
{
    int32_t ret;
    // step 1: get serviceV1_2
    cameraTest->serviceV1_2 = OHOS::HDI::Camera::V1_2::ICameraHost::Get("camera_service", false);
    EXPECT_NE(cameraTest->serviceV1_2, nullptr);
    CAMERA_LOGI("V1_2::ICameraHost get success");
    // step 2: set callback object which is nullptr
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, HDI::Camera::V1_2::INVALID_ARGUMENT);
}


/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_026
 * @tc.desc:SetCallbackV1_2, Callback object exits;
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_026, TestSize.Level1)
{
    int32_t ret;
    // step 1: get serviceV1_2
    cameraTest->serviceV1_2 = OHOS::HDI::Camera::V1_2::ICameraHost::Get("camera_service", false);
    EXPECT_NE(cameraTest->serviceV1_2, nullptr);
    CAMERA_LOGI("V1_2::ICameraHost get success");
    // step 2: set callback object which is exits
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_027
 * @tc.desc:SetFlashlightV1_2, turn off the flashlight with the camera closed
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_027, TestSize.Level1)
{
    int32_t ret;
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 1: set callback object
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
    // step 2: turn off the flashlight
    if (cameraTest->cameraDevice == nullptr) {
        cameraTest->statusV1_2 = 0.0f;
        cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
        EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_0::NO_ERROR);
        // delay for obtaining statucCallback
        sleep(UT_SECOND_TIMES);
        EXPECT_EQ(OHOS::Camera::Test::statusCallback, HDI::Camera::V1_0::FLASHLIGHT_OFF);
    }
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_028
 * @tc.desc:SetFlashlightV1_2, turn on the flashlight with the camera closed
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_028, TestSize.Level1)
{
    int32_t ret;
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 1: set callback object
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
    // step 2: turn on the flashlight
    if (cameraTest->cameraDevice == nullptr) {
        cameraTest->statusV1_2 = 1.0f;
        cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
        EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_0::NO_ERROR);
        // delay for obtaining statucCallback
        sleep(UT_SECOND_TIMES);
        EXPECT_EQ(OHOS::Camera::Test::statusCallback, HDI::Camera::V1_0::FLASHLIGHT_ON);
    }
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_029
 * @tc.desc:Turn on the flashlight with the camera open
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_029, TestSize.Level1)
{
    int32_t ret;
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 1: set callback object
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
    // step 2: open the cameraDevice
    cameraTest->Open(DEVICE_0);
    cameraTest->intents = {PREVIEW};
    cameraTest->StartStream(cameraTest->intents);
    cameraTest->StartCapture(cameraTest->streamIdPreview, cameraTest->captureIdPreview, false, true);
    // step 3: turn on the flashlight
    cameraTest->statusV1_2 = 1.0f;
    cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
    EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_2::DEVICE_CONFLICT);
    EXPECT_EQ(OHOS::Camera::Test::statusCallback, HDI::Camera::V1_0::FLASHLIGHT_UNAVAILABLE);
    // step 4: close the cameraDevice
    cameraTest->captureIds = {cameraTest->captureIdPreview};
    cameraTest->streamIds = {cameraTest->streamIdPreview};
    cameraTest->StopStream(cameraTest->captureIds, cameraTest->streamIds);
    cameraTest->Close();
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_030
 * @tc.desc:Turn off the flashlight with the camera open
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_030, TestSize.Level1)
{
    int32_t ret;
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 1: set callback object
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
    // step 2: open the cameraDevice
    cameraTest->Open(DEVICE_0);
    cameraTest->intents = {PREVIEW};
    cameraTest->StartStream(cameraTest->intents);
    cameraTest->StartCapture(cameraTest->streamIdPreview, cameraTest->captureIdPreview, false, true);
    // step 3: turn off the flashlight
    cameraTest->statusV1_2 = 0.0f;
    cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
    EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_2::DEVICE_CONFLICT);
    // step 4: close the cameraDevice
    cameraTest->captureIds = {cameraTest->captureIdPreview};
    cameraTest->streamIds = {cameraTest->streamIdPreview};
    cameraTest->StopStream(cameraTest->captureIds, cameraTest->streamIds);
    // delay for obtaining statusCallback
    sleep(UT_SECOND_TIMES);
    EXPECT_EQ(OHOS::Camera::Test::statusCallback, HDI::Camera::V1_0::FLASHLIGHT_UNAVAILABLE);
}

/**
 * @tc.name:Camera_Flashlight_Hdi_V1_2_031
 * @tc.desc:Turn off the flashlight with the camera closed
 * @tc.size:MediumTest
 * @tc.type:Function
*/
HWTEST_F(CameraFlashlightUtTestV1_2, Camera_Flashlight_Hdi_V1_2_031, TestSize.Level1)
{
    int32_t ret;
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 1: set callback object
    cameraTest->hostCallbackV1_2 = new OHOS::Camera::Test::TestCameraHostCallbackV1_2();
    ret = cameraTest->serviceV1_2->SetCallbackV1_2(cameraTest->hostCallbackV1_2);
    EXPECT_EQ(ret, 0);
    // step 2: open the cameraDevice
    cameraTest->Open(DEVICE_0);
    cameraTest->intents = {PREVIEW};
    cameraTest->StartStream(cameraTest->intents);
    cameraTest->StartCapture(cameraTest->streamIdPreview, cameraTest->captureIdPreview, false, true);
    // step 3: turn on the flashlight
    cameraTest->statusV1_2 = 1.0f;
    cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
    EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_2::DEVICE_CONFLICT);
    // step 4: close the cameraDevice
    cameraTest->captureIds = {cameraTest->captureIdPreview};
    cameraTest->streamIds = {cameraTest->streamIdPreview};
    cameraTest->StopStream(cameraTest->captureIds, cameraTest->streamIds);
    cameraTest->Close();
    EXPECT_EQ(true, cameraTest->cameraDevice == nullptr);
    // step 5: trun off the flashlight
    cameraTest->statusV1_2 = 0.0f;
    cameraTest->rc = cameraTest->serviceV1_2->SetFlashlightV1_2(cameraTest->statusV1_2);
    EXPECT_EQ(cameraTest->rc, HDI::Camera::V1_2::NO_ERROR);
    // delay for obtaining statusCallback
    sleep(UT_SECOND_TIMES);
    EXPECT_EQ(OHOS::Camera::Test::statusCallback, HDI::Camera::V1_0::FLASHLIGHT_OFF);
}