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

#include "audio_lib_common.h"
#include "audio_librender_hardwaredependence_test.h"

using namespace std;
using namespace testing::ext;
using namespace OHOS::Audio;

namespace {
const string BIND_CONTROL = "control";
const string BIND_RENDER = "render";
#ifdef PRODUCT_RK3568
    const string PATH_SWITCH_ORDER = "Headphone1 Switch";
    constexpr int MAX_VOLUME = 255;
    constexpr int MIN_VOLUME = 0;
    constexpr int BELOW_MAX_VOLUME = 254;
    constexpr int OVER_MIN_VOLUME = 1;
#else
    const string PATH_SWITCH_ORDER = "Dacl enable";
    constexpr int MAX_VOLUME = 127;
    constexpr int MIN_VOLUME = 40;
    constexpr int BELOW_MAX_VOLUME = 126;
    constexpr int OVER_MIN_VOLUME = 41;
#endif
    constexpr float MAX_GAIN = 15;
    constexpr float MIN_GAIN = 0;
class AudioLibRenderHardwareDependenceTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static struct DevHandle *(*BindServiceRenderSo)(const char *serverName);
    static int32_t (*InterfaceLibOutputRender)(struct DevHandle *handle,
                    int cmdId, struct AudioHwRenderParam *handleData);
    static int32_t (*InterfaceLibCtlRender)(struct DevHandle *handle, int cmdId, struct AudioHwRenderParam *handleData);
    static void (*CloseServiceRenderSo)(struct DevHandle *handle);
    static void *ptrHandle;
#ifdef AUDIO_MPI_SO
    static int32_t (*SdkInit)();
    static void (*SdkExit)();
#endif
    int32_t LibHwOutputRender(struct AudioHwRender *hwRender, struct DevHandle *handlerender) const;
    int32_t BindServiceAndHwRender(struct AudioHwRender *&hwRender,
        const std::string BindName, const std::string adapterNameCase, struct DevHandle *&handle) const;
};

struct DevHandle *(*AudioLibRenderHardwareDependenceTest::BindServiceRenderSo)(const char *serverName) = nullptr;
int32_t (*AudioLibRenderHardwareDependenceTest::InterfaceLibOutputRender)(struct DevHandle *handle, int cmdId,
    struct AudioHwRenderParam *handleData) = nullptr;
int32_t (*AudioLibRenderHardwareDependenceTest::InterfaceLibCtlRender)(struct DevHandle *handle, int cmdId,
    struct AudioHwRenderParam *handleData) = nullptr;
void (*AudioLibRenderHardwareDependenceTest::CloseServiceRenderSo)(struct DevHandle *handle) = nullptr;
void *AudioLibRenderHardwareDependenceTest::ptrHandle = nullptr;
#ifdef AUDIO_MPI_SO
    int32_t (*AudioLibRenderHardwareDependenceTest::SdkInit)() = nullptr;
    void (*AudioLibRenderHardwareDependenceTest::SdkExit)() = nullptr;
#endif

void AudioLibRenderHardwareDependenceTest::SetUpTestCase(void)
{
    char resolvedPath[] = HDF_LIBRARY_FULL_PATH("libhdi_audio_interface_lib_render");
    ptrHandle = dlopen(resolvedPath, RTLD_LAZY);
    if (ptrHandle == nullptr) {
        return;
    }
    BindServiceRenderSo = (struct DevHandle* (*)(const char *serverName))dlsym(ptrHandle, "AudioBindServiceRender");
    InterfaceLibOutputRender = (int32_t (*)(struct DevHandle *handle, int cmdId,
        struct AudioHwRenderParam *handleData))dlsym(ptrHandle, "AudioInterfaceLibOutputRender");
    InterfaceLibCtlRender = (int32_t (*)(struct DevHandle *handle, int cmdId,
        struct AudioHwRenderParam *handleData))dlsym(ptrHandle, "AudioInterfaceLibCtlRender");
    CloseServiceRenderSo = (void (*)(struct DevHandle *handle))dlsym(ptrHandle, "AudioCloseServiceRender");
    if (BindServiceRenderSo == nullptr || CloseServiceRenderSo == nullptr ||
        InterfaceLibCtlRender == nullptr || InterfaceLibOutputRender == nullptr) {
        dlclose(ptrHandle);
        return;
    }
#ifdef AUDIO_MPI_SO
    SdkInit = (int32_t (*)())(dlsym(ptrHandle, "MpiSdkInit"));
    if (SdkInit == nullptr) {
        return;
    }
    SdkExit = (void (*)())(dlsym(ptrHandle, "MpiSdkExit"));
    if (SdkExit == nullptr) {
        return;
    }
    SdkInit();
#endif
}

void AudioLibRenderHardwareDependenceTest::TearDownTestCase(void)
{
    if (BindServiceRenderSo != nullptr) {
        BindServiceRenderSo = nullptr;
    }
    if (CloseServiceRenderSo != nullptr) {
        CloseServiceRenderSo = nullptr;
    }
    if (InterfaceLibOutputRender != nullptr) {
        InterfaceLibOutputRender = nullptr;
    }
    if (InterfaceLibCtlRender != nullptr) {
        InterfaceLibCtlRender = nullptr;
    }
#ifdef AUDIO_MPI_SO
    SdkExit();
    if (SdkInit != nullptr) {
        SdkInit = nullptr;
    }
    if (SdkExit != nullptr) {
        SdkExit = nullptr;
    }
#endif
    if (ptrHandle != nullptr) {
        dlclose(ptrHandle);
        ptrHandle = nullptr;
    }
}

void AudioLibRenderHardwareDependenceTest::SetUp(void) {}

void AudioLibRenderHardwareDependenceTest::TearDown(void) {}

int32_t AudioLibRenderHardwareDependenceTest::LibHwOutputRender(struct AudioHwRender *hwRender,
                                                                struct DevHandle *handlerender) const
{
    if (hwRender == nullptr || handlerender == nullptr) {
        return HDF_FAILURE;
    }
    if (InterfaceLibOutputRender(handlerender, AUDIO_DRV_PCM_IOCTRL_RENDER_OPEN, &hwRender->renderParam) ||
        InterfaceLibOutputRender(handlerender, AUDIO_DRV_PCM_IOCTL_HW_PARAMS, &hwRender->renderParam) ||
        InterfaceLibOutputRender(handlerender, AUDIO_DRV_PCM_IOCTL_PREPARE, &hwRender->renderParam) ||
        InterfaceLibOutputRender(handlerender, AUDIO_DRV_PCM_IOCTRL_START, &hwRender->renderParam)) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t AudioLibRenderHardwareDependenceTest::BindServiceAndHwRender(struct AudioHwRender *&hwRender,
    const std::string BindName, const std::string adapterNameCase, struct DevHandle *&handle) const
{
    int32_t ret = HDF_FAILURE;
    handle = BindServiceRenderSo(BindName.c_str());
    if (handle == nullptr) {
        return HDF_FAILURE;
    }
    hwRender = (struct AudioHwRender *)calloc(1, sizeof(*hwRender));
    if (hwRender == nullptr) {
        CloseServiceRenderSo(handle);
        return HDF_FAILURE;
    }
    ret = InitHwRender(hwRender, adapterNameCase);
    if (ret != HDF_SUCCESS) {
        CloseServiceRenderSo(handle);
        free(hwRender);
        hwRender = nullptr;
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing volume value of AcodecIn is normal value and reading
*    this value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_001
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_ELEM_WRITE and AUDIODRV_CTL_IOCTL_ELEM_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_001, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float volumeValue = 0;
    float volumeThresholdValueMaxIn = 0;
    float volumeThresholdValueMinIn = 0;
    float volumeBoundaryValueIn = 127.9;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *hwRender = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_VOL_THRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeThresholdValueMaxIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMax;
    volumeThresholdValueMinIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMin;

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMaxIn - 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeValue = hwRender->renderParam.renderMode.ctlParam.volume;
    EXPECT_EQ(BELOW_MAX_VOLUME, volumeValue);

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMinIn + 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeValue = hwRender->renderParam.renderMode.ctlParam.volume;
    EXPECT_EQ(OVER_MIN_VOLUME, volumeValue);
    hwRender->renderParam.renderMode.ctlParam.volume = volumeBoundaryValueIn;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeValue = hwRender->renderParam.renderMode.ctlParam.volume;
    EXPECT_EQ((int)volumeBoundaryValueIn, volumeValue);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing volume value of AcodecIn is boundary value and reading
*    this value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_002
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_ELEM_WRITE and AUDIODRV_CTL_IOCTL_ELEM_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_002, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    struct AudioHwRender *hwRender = nullptr;
    struct DevHandle *handle = nullptr;
    float volumeThresholdValueMaxIn = 0;
    float volumeThresholdValueMinIn = 0;
    float volumeValue = 0;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_VOL_THRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeThresholdValueMaxIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMax;
    volumeThresholdValueMinIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMin;

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMinIn;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeValue = hwRender->renderParam.renderMode.ctlParam.volume;
    EXPECT_EQ(MIN_VOLUME, volumeValue);

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMaxIn;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeValue = hwRender->renderParam.renderMode.ctlParam.volume;
    EXPECT_EQ(MAX_VOLUME, volumeValue);

    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing volume value is invalid value of AcodecIn.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_003
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_ELEM_WRITE and AUDIODRV_CTL_IOCTL_ELEM_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_Volume_Write_Read_003, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *hwRender = nullptr;
    float volumeThresholdValueMaxIn = 0;
    float volumeThresholdValueMinIn = 0;

    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_VOL_THRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeThresholdValueMaxIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMax;
    volumeThresholdValueMinIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMin;

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMaxIn + 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_FAILURE, ret);

    hwRender->renderParam.renderMode.ctlParam.volume = volumeThresholdValueMinIn - 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_ELEM_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_FAILURE, ret);

    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}

/**
* @tc.name  test InterfaceLibCtlRender API via writing GetVolthreshold value that
*    Hardware equipment of Acodec_ChangeIn.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_GetVolthresholdRead_002
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_VOL_THRESHOLD_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_GetVolthresholdRead_002, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float volumeThresholdValueMaxIn = 0;
    float volumeThresholdValueMinIn = 0;
    struct AudioHwRender *hwRender = nullptr;
    struct DevHandle *handle = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_VOL_THRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    volumeThresholdValueMaxIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMax;
    volumeThresholdValueMinIn = hwRender->renderParam.renderMode.ctlParam.volThreshold.volMin;
    EXPECT_EQ(MAX_VOLUME, volumeThresholdValueMaxIn);
    EXPECT_EQ(MIN_VOLUME, volumeThresholdValueMinIn);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing ChannelMode value is normal value and reading this value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_ChannelMode_Write_Read_001
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE
*    and AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_ChannelMode_Write_Read_001,
         TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float channelModeExc = 1;
    struct DevHandle *handle = nullptr;
    struct DevHandle *handleRender = nullptr;
    struct AudioHwRender *impl = nullptr;
    ret = BindServiceAndHwRender(impl, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);
    handleRender = BindServiceRenderSo(BIND_RENDER.c_str());
    if (handleRender == nullptr) {
        CloseServiceRenderSo(handle);
        free(impl);
        impl = nullptr;
        ASSERT_NE(nullptr, handleRender);
    }
    ret = LibHwOutputRender(impl, handleRender);
    EXPECT_EQ(HDF_SUCCESS, ret);
    if (impl != nullptr) {
        impl->renderParam.frameRenderMode.mode = AUDIO_CHANNEL_BOTH_RIGHT;
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        channelModeExc = impl->renderParam.frameRenderMode.mode;
        EXPECT_EQ(AUDIO_CHANNEL_BOTH_RIGHT, channelModeExc);

        impl->renderParam.frameRenderMode.mode = AUDIO_CHANNEL_RIGHT_MUTE;
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE, &impl->renderParam);
#ifdef PRODUCT_RK3568
        EXPECT_EQ(HDF_FAILURE, ret);
#else
        EXPECT_EQ(HDF_SUCCESS, ret);
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        channelModeExc = impl->renderParam.frameRenderMode.mode;
        EXPECT_EQ(AUDIO_CHANNEL_RIGHT_MUTE, channelModeExc);
#endif
    }
    CloseServiceRenderSo(handleRender);
    CloseServiceRenderSo(handle);
    free(impl);
    impl = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing ChannelMode value is boundary value and reading this value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_ChannelMode_Write_Read_002
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE
*    and AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_ChannelMode_Write_Read_002,
         TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float channelModeExc = 1;
    struct DevHandle *handleRender = nullptr;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *impl = nullptr;
    ret = BindServiceAndHwRender(impl, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);
    handleRender = BindServiceRenderSo(BIND_RENDER.c_str());
    if (handleRender == nullptr) {
        free(impl);
        impl = nullptr;
        CloseServiceRenderSo(handle);
        ASSERT_NE(nullptr, handleRender);
    }
    ret = LibHwOutputRender(impl, handleRender);
    EXPECT_EQ(HDF_SUCCESS, ret);
    if (impl != nullptr) {
        impl->renderParam.frameRenderMode.mode = AUDIO_CHANNEL_BOTH_MUTE;
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE, &impl->renderParam);
#ifdef PRODUCT_RK3568
        EXPECT_EQ(HDF_FAILURE, ret);
#else
        EXPECT_EQ(HDF_SUCCESS, ret);
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        channelModeExc = impl->renderParam.frameRenderMode.mode;
        EXPECT_EQ(AUDIO_CHANNEL_BOTH_MUTE, channelModeExc);
#endif
        impl->renderParam.frameRenderMode.mode = AUDIO_CHANNEL_NORMAL;
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_WRITE, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_CHANNEL_MODE_READ, &impl->renderParam);
        EXPECT_EQ(HDF_SUCCESS, ret);
        channelModeExc = impl->renderParam.frameRenderMode.mode;
        EXPECT_EQ(AUDIO_CHANNEL_NORMAL, channelModeExc);
    }
    CloseServiceRenderSo(handleRender);
    CloseServiceRenderSo(handle);
    free(impl);
    impl = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via selecting scene.
* @tc.number  SUB_Audio_InterfaceLib_CtlRender_SelectScene_001
* @tc.desc  test InterfaceLibCtlRender,cmdId is AUDIODRV_CTL_IOCTL_SCENESELECT_WRITE.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLib_CtlRender_SelectScene_001, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *hwRender = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    struct AudioSceneDescriptor scene = {
        .scene.id = 0,
        .desc.pins = PIN_OUT_HEADSET,
    };
    hwRender->renderParam.renderMode.hwInfo.pathSelect.deviceInfo.deviceNum = 1;
    ret = strcpy_s(hwRender->renderParam.renderMode.hwInfo.pathSelect.deviceInfo.deviceSwitchs[0].deviceSwitch,
        PATHPLAN_COUNT, PATH_SWITCH_ORDER.c_str());
    hwRender->renderParam.renderMode.hwInfo.pathSelect.deviceInfo.deviceSwitchs[0].value = 0;
    hwRender->renderParam.frameRenderMode.attrs.type = (enum AudioCategory)(scene.scene.id);
    hwRender->renderParam.renderMode.hwInfo.deviceDescript.pins = scene.desc.pins;

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_SCENESELECT_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);

    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing normal gain value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_001
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_GAIN_WRITE and AUDIODRV_CTL_IOCTL_GAIN_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_001, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    struct AudioHwRender *hwRender = nullptr;
    struct DevHandle *handle = nullptr;
    float gainValue = 0;
    float gainThresholdValueMax, gainThresholdValueMin;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAINTHRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainThresholdValueMax = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMax;
    gainThresholdValueMin = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMin;
    ret = InitHwRenderMode(hwRender->renderParam.renderMode);
    EXPECT_EQ(HDF_SUCCESS, ret);

    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMax - 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainValue = hwRender->renderParam.renderMode.ctlParam.audioGain.gain;
    EXPECT_EQ(gainThresholdValueMax - 1, gainValue);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMin + 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainValue = hwRender->renderParam.renderMode.ctlParam.audioGain.gain;
    EXPECT_EQ(gainThresholdValueMin + 1, gainValue);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = 2.3;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainValue = hwRender->renderParam.renderMode.ctlParam.audioGain.gain;
    EXPECT_EQ(2, gainValue);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing boundary value of gain and reading gain value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_002
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_GAIN_WRITE and AUDIODRV_CTL_IOCTL_GAIN_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_002, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float gainValue = 0;
    float gainThresholdValueMax, gainThresholdValueMin;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *hwRender = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAINTHRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainThresholdValueMax = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMax;
    gainThresholdValueMin = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMin;
    ret = InitHwRenderMode(hwRender->renderParam.renderMode);
    EXPECT_EQ(HDF_SUCCESS, ret);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMin;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainValue = hwRender->renderParam.renderMode.ctlParam.audioGain.gain;
    EXPECT_EQ(gainThresholdValueMin, gainValue);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMax;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainValue = hwRender->renderParam.renderMode.ctlParam.audioGain.gain;
    EXPECT_EQ(gainThresholdValueMax, gainValue);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via writing gain invalid value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_003
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_GAIN_WRITE and AUDIODRV_CTL_IOCTL_GAIN_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_GainWrite_Read_003, TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float gainThresholdValueMax, gainThresholdValueMin;
    struct AudioHwRender *hwRender = nullptr;
    struct DevHandle *handle = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAINTHRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainThresholdValueMax = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMax;
    gainThresholdValueMin = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMin;
    ret = InitHwRenderMode(hwRender->renderParam.renderMode);
    EXPECT_EQ(HDF_SUCCESS, ret);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMax + 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_FAILURE, ret);
    hwRender->renderParam.renderMode.ctlParam.audioGain.gain = gainThresholdValueMin - 1;
    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAIN_WRITE, &hwRender->renderParam);
    EXPECT_EQ(HDF_FAILURE, ret);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
/**
* @tc.name  test InterfaceLibCtlRender API via getting gainthreshold value.
* @tc.number  SUB_Audio_InterfaceLibCtlRender_GetGainthresholdRead_001
* @tc.desc  test InterfaceLibCtlRender ,cmdId is AUDIODRV_CTL_IOCTL_GAINTHRESHOLD_READ.
* @tc.author: zhouyongxiao
*/
HWTEST_F(AudioLibRenderHardwareDependenceTest, SUB_Audio_InterfaceLibCtlRender_GetGainthresholdRead_001,
         TestSize.Level1)
{
    int32_t ret = HDF_FAILURE;
    float gainThresholdValueMaxGet, gainThresholdValueMinGet;
    struct DevHandle *handle = nullptr;
    struct AudioHwRender *hwRender = nullptr;
    ret = BindServiceAndHwRender(hwRender, BIND_CONTROL.c_str(), ADAPTER_NAME, handle);
    ASSERT_EQ(HDF_SUCCESS, ret);

    ret = InterfaceLibCtlRender(handle, AUDIODRV_CTL_IOCTL_GAINTHRESHOLD_READ, &hwRender->renderParam);
    EXPECT_EQ(HDF_SUCCESS, ret);
    gainThresholdValueMaxGet = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMax;
    gainThresholdValueMinGet = hwRender->renderParam.renderMode.ctlParam.audioGain.gainMin;
    EXPECT_EQ(MAX_GAIN, gainThresholdValueMaxGet);
    EXPECT_EQ(MIN_GAIN, gainThresholdValueMinGet);
    CloseServiceRenderSo(handle);
    free(hwRender);
    hwRender = nullptr;
}
}