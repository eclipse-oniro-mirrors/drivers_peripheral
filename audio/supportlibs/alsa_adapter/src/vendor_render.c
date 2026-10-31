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

#include "alsa_snd_render.h"
#include "common.h"

#define HDF_LOG_TAG HDF_AUDIO_HAL_RENDER

/*
 * The HDI service scales its 0.0-1.0 float volume by VOLUME_CHANGE (100) before
 * invoking the adapter (see AudioRenderSetVolume in audio_render.c), so the
 * interface-lib volume domain is 0..VOLUME_CHANGE. The ES1370's AK4531 codec
 * exposes "Master Playback Volume" with a 0..31 hardware range: map between
 * the two domains and remember the last requested abstract value so that
 * get/set round trips stay lossless (the hardware granularity is coarser).
 */
typedef struct RenderData {
    struct AlsaMixerCtlElement ctrlLeftVolume;
    struct AlsaMixerCtlElement ctrlRightVolume;
    long tempVolume;
    long hwVolMin; /* hardware range of the volume element */
    long hwVolMax;
    bool hwVolLoaded;
    long volume; /* last requested abstract volume, 0..VOLUME_CHANGE */
    bool volumeValid;
} RenderData;

static int32_t RenderLoadHwVolRange(struct AlsaSoundCard *cardIns, RenderData *priData)
{
    if (priData->hwVolLoaded) {
        return HDF_SUCCESS;
    }
    if (SndElementReadRange(cardIns, &priData->ctrlLeftVolume, &priData->hwVolMin, &priData->hwVolMax) !=
        HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("SndElementReadRange fail!");
        return HDF_FAILURE;
    }
    if (priData->hwVolMax <= priData->hwVolMin) {
        AUDIO_FUNC_LOGE("Invalid hardware volume range!");
        return HDF_FAILURE;
    }
    priData->hwVolLoaded = true;
    return HDF_SUCCESS;
}

static int32_t RenderInitImpl(struct AlsaRender *renderIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    if (renderIns->priData != NULL) {
        return HDF_SUCCESS;
    }

    RenderData *priData = (RenderData *)OsalMemCalloc(sizeof(RenderData));
    if (priData == NULL) {
        AUDIO_FUNC_LOGE("Failed to allocate memory!");
        return HDF_FAILURE;
    }

    /* Look the elements up by name: numids are card-specific while the
     * AK4531/ES1370 control names are stable. */
    SndElementItemInit(&priData->ctrlLeftVolume);
    SndElementItemInit(&priData->ctrlRightVolume);
    priData->ctrlLeftVolume.name = SND_ELEM_DACL_PLAYBACK_VOL;
    priData->ctrlRightVolume.name = SND_ELEM_DACR_PLAYBACK_VOL;
    RenderSetPriData(renderIns, (RenderPriData)priData);

    return HDF_SUCCESS;
}

static int32_t RenderGetVolThresholdImpl(struct AlsaRender *renderIns, long *volMin, long *volMax)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(volMin);
    CHECK_NULL_PTR_RETURN_DEFAULT(volMax);
    int32_t ret = HDF_SUCCESS;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)renderIns;
    RenderData *priData = RenderGetPriData(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    /* Query the hardware range to make sure the volume element is available,
     * but report the adapter's abstract volume domain (see RenderData above). */
    ret = RenderLoadHwVolRange(cardIns, priData);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    *volMin = 0;
    *volMax = VOLUME_CHANGE;

    return HDF_SUCCESS;
}

static int32_t RenderGetVolumeImpl(struct AlsaRender *renderIns, long *volume)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(volume);
    int32_t ret = HDF_SUCCESS;
    long volLeft = 0;
    long volRight = 0;
    long hwSpan;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)renderIns;
    RenderData *priData = RenderGetPriData(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    if (priData->volumeValid) {
        /* The hardware granularity is coarser than the abstract domain, so
         * scaling down and back up would not return the requested value. */
        *volume = priData->volume;
        return HDF_SUCCESS;
    }

    ret = RenderLoadHwVolRange(cardIns, priData);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    ret = SndElementReadInt(cardIns, &priData->ctrlLeftVolume, &volLeft);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("Read left volume fail!");
        return HDF_FAILURE;
    }
    ret = SndElementReadInt(cardIns, &priData->ctrlRightVolume, &volRight);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("Read right volume fail!");
        return HDF_FAILURE;
    }
    hwSpan = priData->hwVolMax - priData->hwVolMin;
    long hwVol = ((volLeft + volRight) >> 1) - priData->hwVolMin;
    if (hwVol < 0) {
        hwVol = 0;
    }
    if (hwVol > hwSpan) {
        hwVol = hwSpan;
    }
    *volume = hwVol * VOLUME_CHANGE / hwSpan;
    if (*volume > VOLUME_CHANGE) {
        *volume = VOLUME_CHANGE;
    }

    return HDF_SUCCESS;
}

static int32_t RenderSetVolumeImpl(struct AlsaRender *renderIns, long volume)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    int32_t ret = HDF_SUCCESS;
    long hwSpan;
    long hwVol;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)renderIns;
    RenderData *priData = RenderGetPriData(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    if (volume < 0) {
        volume = 0;
    }
    if (volume > VOLUME_CHANGE) {
        volume = VOLUME_CHANGE;
    }
    ret = RenderLoadHwVolRange(cardIns, priData);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    hwSpan = priData->hwVolMax - priData->hwVolMin;

    hwVol = priData->hwVolMin + volume * hwSpan / VOLUME_CHANGE;
    ret = SndElementWriteInt(cardIns, &priData->ctrlLeftVolume, hwVol);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("Write left volume fail!");
        return HDF_FAILURE;
    }
    ret = SndElementWriteInt(cardIns, &priData->ctrlRightVolume, hwVol);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("Write right volume fail!");
        return HDF_FAILURE;
    }
    priData->volume = volume;
    priData->volumeValid = true;

    return HDF_SUCCESS;
}

#ifdef AUDIO_HAL_P7885
static int32_t RenderSelectSceneImpl(struct AlsaRender *renderIns, const struct AudioHwRenderParam *handleData)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    renderIns->descPins = handleData->renderMode.hwInfo.deviceDescript.pins;
    return HDF_SUCCESS;
}
#else
static int32_t RenderSelectSceneImpl(struct AlsaRender *renderIns, enum AudioPortPin descPins,
    const struct PathDeviceInfo *deviceInfo)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    renderIns->descPins = descPins;
    return HDF_SUCCESS;
}
#endif

static bool RenderGetMuteImpl(struct AlsaRender *renderIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    return renderIns->muteState;
}

static int32_t RenderSetMuteImpl(struct AlsaRender *renderIns, bool muteFlag)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    int32_t ret = HDF_SUCCESS;
    long vol = 0;
    long setVol = 0;
    RenderData *priData = RenderGetPriData(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    ret = renderIns->GetVolume(renderIns, &vol);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("GetVolume failed!");
        return HDF_FAILURE;
    }
    
    if (muteFlag) {
        priData->tempVolume = vol;
        setVol = 0;
    } else {
        setVol = priData->tempVolume;
    }
    
    renderIns->SetVolume(renderIns, setVol);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("SetVolume failed!");
        return HDF_FAILURE;
    }
    renderIns->muteState = muteFlag;
    
    return HDF_SUCCESS;
}

#ifdef AUDIO_HAL_P7885
static int32_t RenderStartImpl(struct AlsaRender *renderIns, const struct AudioHwRenderParam *handleData)
{
    return HDF_SUCCESS;
}
#else
static int32_t RenderStartImpl(struct AlsaRender *renderIns)
{
    return HDF_SUCCESS;
}
#endif

static int32_t RenderStopImpl(struct AlsaRender *renderIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    /* The pcm may have failed to open (for example the device is busy);
     * snd_pcm_drain(NULL) would crash the process, so check it first. */
    if (renderIns->soundCard.pcmHandle == NULL) {
        AUDIO_FUNC_LOGE("pcmHandle is null and return INVALID_PARAM");
        return HDF_ERR_INVALID_PARAM;
    }
    snd_pcm_drain(renderIns->soundCard.pcmHandle);
    return HDF_SUCCESS;
}

static int32_t RenderGetGainThresholdImpl(struct AlsaRender *renderIns, float *gainMin, float *gainMax)
{
    AUDIO_FUNC_LOGI("emulator not support gain operation");
    return HDF_SUCCESS;
}

static int32_t RenderGetGainImpl(struct AlsaRender *renderIns, float *volume)
{
    AUDIO_FUNC_LOGI("emulator not support gain operation");
    return HDF_SUCCESS;
}

static int32_t RenderSetGainImpl(struct AlsaRender *renderIns, float volume)
{
    AUDIO_FUNC_LOGI("emulator not support gain operation");
    return HDF_SUCCESS;
}

static int32_t RenderGetChannelModeImpl(struct AlsaRender *renderIns, enum AudioChannelMode *mode)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(mode);
    /* The alsa adapter has no channel-mode control; report the default. */
    *mode = AUDIO_CHANNEL_NORMAL;
    return HDF_SUCCESS;
}

static int32_t RenderSetChannelModeImpl(struct AlsaRender *renderIns, enum AudioChannelMode mode)
{
    if (renderIns == NULL) {
        AUDIO_FUNC_LOGE("Parameter error!");
        return HDF_ERR_INVALID_PARAM;
    }
    switch (mode) {
        case AUDIO_CHANNEL_NORMAL:
        case AUDIO_CHANNEL_BOTH_LEFT:
        case AUDIO_CHANNEL_BOTH_RIGHT:
            /* The alsa adapter has no channel-mode control; the basic modes
             * are accepted as no-ops, mirroring GetChannelMode's default. */
            return HDF_SUCCESS;
        case AUDIO_CHANNEL_EXCHANGE:
        case AUDIO_CHANNEL_MIX:
        case AUDIO_CHANNEL_LEFT_MUTE:
        case AUDIO_CHANNEL_RIGHT_MUTE:
        case AUDIO_CHANNEL_BOTH_MUTE:
        default:
            AUDIO_FUNC_LOGI("emulator not support channel mode: %{public}d", mode);
            return HDF_ERR_NOT_SUPPORT;
    }
}

static int32_t RenderSetPauseStateImpl(struct AlsaRender *renderIns, bool pauseFlag)
{
    (void)pauseFlag;
    if (renderIns == NULL) {
        AUDIO_FUNC_LOGE("Parameter error!");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t RenderOverrideFunc(struct AlsaRender *renderIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(renderIns);
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)renderIns;

    if (cardIns->cardType == SND_CARD_PRIMARY) {
        renderIns->Init = RenderInitImpl;
        renderIns->SelectScene = RenderSelectSceneImpl;
        renderIns->Start = RenderStartImpl;
        renderIns->Stop = RenderStopImpl;
        renderIns->GetVolThreshold = RenderGetVolThresholdImpl;
        renderIns->GetVolume = RenderGetVolumeImpl;
        renderIns->SetVolume = RenderSetVolumeImpl;
        renderIns->GetGainThreshold = RenderGetGainThresholdImpl;
        renderIns->GetGain = RenderGetGainImpl;
        renderIns->SetGain = RenderSetGainImpl;
        renderIns->GetMute = RenderGetMuteImpl;
        renderIns->SetMute = RenderSetMuteImpl;
        renderIns->GetChannelMode = RenderGetChannelModeImpl;
        renderIns->SetChannelMode = RenderSetChannelModeImpl;
        renderIns->SetPauseState = RenderSetPauseStateImpl;
    }

    return HDF_SUCCESS;
}
