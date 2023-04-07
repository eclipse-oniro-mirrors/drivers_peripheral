/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "camera_ability_test.h"

using namespace testing::ext;

void CameraAbilityTest::SetUpTestCase(void) {}

void CameraAbilityTest::TearDownTestCase(void) {}

void CameraAbilityTest::SetUp(void)
{
    if (display_ == nullptr) {
        display_ = std::make_shared<TestDisplay>();
    }
    display_->Init();
}

OHOS::Camera::RetCode CameraAbilityTest::GetSensorOrientation(std::shared_ptr<CameraAbility> &ability)
{
    common_metadata_header_t *data = display_->ability->get();
    int32_t sensorOrientation;
    camera_metadata_item_t entry;
    int ret = OHOS::Camera::FindCameraMetadataItem(data, OHOS_SENSOR_ORIENTATION, &entry);
    if (ret != 0) {
        CAMERA_LOGE("get OHOS_SENSOR_ORIENTATION error.");
        return OHOS::Camera::RC_ERROR;
    }
    sensorOrientation = *(entry.data.i32);
    CAMERA_LOGD("get sensorOrientation = %{public}d", sensorOrientation);
    return OHOS::Camera::RC_OK;
}

OHOS::Camera::RetCode CameraAbilityTest::GetFlashAvailable(std::shared_ptr<CameraAbility> &ability)
{
    common_metadata_header_t *data = display_->ability->get();
    uint8_t flashAvailable;
    camera_metadata_item_t entry;
    int ret = OHOS::Camera::FindCameraMetadataItem(data, OHOS_ABILITY_FLASH_AVAILABLE, &entry);
    if (ret != 0) {
        CAMERA_LOGE("get OHOS_ABILITY_FLASH_AVAILABLE error.");
        return OHOS::Camera::RC_ERROR;
    }
    flashAvailable = *(entry.data.u8);
    CAMERA_LOGD("get flashAvailable = %{public}d", static_cast<int>(flashAvailable));
    return OHOS::Camera::RC_OK;
}

OHOS::Camera::RetCode CameraAbilityTest::GetAfAvailable(std::shared_ptr<CameraAbility> &ability)
{
    common_metadata_header_t *data = display_->ability->get();
    std::vector<uint8_t> afAvailable;
    camera_metadata_item_t entry;
    int ret = OHOS::Camera::FindCameraMetadataItem(data, OHOS_CONTROL_AF_AVAILABLE_MODES, &entry);
    if (ret != 0) {
        CAMERA_LOGE("get OHOS_CONTROL_AF_AVAILABLE_MODES error.");
        return OHOS::Camera::RC_ERROR;
    }
    uint32_t count = entry.count;
    CAMERA_LOGD("count = %{public}u", count);

    for (int i = 0; i < count; i++) {
        afAvailable.push_back(*(entry.data.u8 + i));
    }

    for (auto it = afAvailable.begin(); it != afAvailable.end(); it++) {
        CAMERA_LOGI("afAvailable = %{public}d", static_cast<int>(*it));
    }
    return OHOS::Camera::RC_OK;
}

OHOS::Camera::RetCode CameraAbilityTest::GetZoomRatioRange(std::shared_ptr<CameraAbility> &ability)
{
    common_metadata_header_t *data = display_->ability->get();
    std::vector<float> zoomRatioRange;
    camera_metadata_item_t entry;
    int ret = OHOS::Camera::FindCameraMetadataItem(data, OHOS_ABILITY_ZOOM_RATIO_RANGE, &entry);
    if (ret != 0) {
        CAMERA_LOGE("get OHOS_ABILITY_ZOOM_RATIO_RANGE error.");
        return OHOS::Camera::RC_ERROR;
    }
    uint32_t count = entry.count;
    CAMERA_LOGD("count = %{public}u", count);

    for (int i = 0; i < count; i++) {
        zoomRatioRange.push_back(*(entry.data.f + i));
    }

    for (auto it = zoomRatioRange.begin(); it != zoomRatioRange.end(); it++) {
        CAMERA_LOGI("zoomRatioRange = %{public}f", *it);
    }
    return OHOS::Camera::RC_OK;
}

OHOS::Camera::RetCode CameraAbilityTest::GetJpegOrientation(std::shared_ptr<CameraAbility> &ability)
{
    common_metadata_header_t *data = display_->ability->get();
    int32_t jpegOrientation;
    camera_metadata_item_t entry;
    int ret = OHOS::Camera::FindCameraMetadataItem(data, OHOS_JPEG_ORIENTATION, &entry);
    if (ret != 0) {
        CAMERA_LOGE("get OHOS_JPEG_ORIENTATION error.");
        return OHOS::Camera::RC_ERROR;
    }
    jpegOrientation = *(entry.data.i32);
    CAMERA_LOGD("get jpegOrientation = %{public}d", jpegOrientation);
    return OHOS::Camera::RC_OK;
}

void CameraAbilityTest::TearDown(void)
{
    display_->Close();
}

/**
 * @tc.name: get camera ability
 * @tc.desc: get camera ability
 * @tc.level: Level1
 * @tc.size: MediumTest
 * @tc.type: Function
 */
static HWTEST_F(CameraAbilityTest, camera_ability_001, TestSize.Level1)
{
    EXPECT_EQ(true, display_->ability != nullptr);
    if (display_->ability == nullptr) {
        CAMERA_LOGE("ability is null.");
        return;
    }
    GetSensorOrientation(display_->ability);
    GetFlashAvailable(display_->ability);
    GetAfAvailable(display_->ability);
    GetZoomRatioRange(display_->ability);
    GetJpegOrientation(display_->ability);
}
