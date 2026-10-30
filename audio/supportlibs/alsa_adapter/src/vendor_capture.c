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

#include "alsa_snd_capture.h"
#include "common.h"

#define HDF_LOG_TAG HDF_AUDIO_HAL_CAPTURE

/*
 * The HDI service scales its 0.0-1.0 float volume by VOLUME_CHANGE (100) before
 * invoking the adapter (see AudioCaptureSetVolume in audio_capture.c), so the
 * interface-lib volume domain is 0..VOLUME_CHANGE. The ES1370's AK4531 codec
 * has no master capture volume; "Mic Volume" (0..31) is the mic input gain used
 * as the capture volume. Map between the two domains and remember the last
 * requested abstract value so that get/set round trips stay lossless.
 */
typedef struct CaptureData {
    struct AlsaMixerCtlElement ctrlLeftVolume;
    struct AlsaMixerCtlElement ctrlRightVolume;
    long tempVolume;
    long hwVolMin; /* hardware range of the volume element */
    long hwVolMax;
    bool hwVolLoaded;
    long volume; /* last requested abstract volume, 0..VOLUME_CHANGE */
    bool volumeValid;
} CaptureData;

static int32_t CaptureLoadHwVolRange(struct AlsaSoundCard *cardIns, CaptureData *priData)
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

static int32_t CaptureInitImpl(struct AlsaCapture* captureIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    if (captureIns->priData != NULL) {
        return HDF_SUCCESS;
    }

    CaptureData *priData = (CaptureData *)OsalMemCalloc(sizeof(CaptureData));
    if (priData == NULL) {
        AUDIO_FUNC_LOGE("Failed to allocate memory!");
        return HDF_FAILURE;
    }

    /* Look the elements up by name: numids are card-specific while the
     * AK4531/ES1370 control names are stable. */
    SndElementItemInit(&priData->ctrlLeftVolume);
    SndElementItemInit(&priData->ctrlRightVolume);
    priData->ctrlLeftVolume.name = SND_ELEM_DACL_CAPTURE_VOL;
    priData->ctrlRightVolume.name = SND_ELEM_DACR_CAPTURE_VOL;
    CaptureSetPriData(captureIns, (CapturePriData)priData);

    return HDF_SUCCESS;
}

#ifdef AUDIO_HAL_P7885
static int32_t CaptureSelectSceneImpl(struct AlsaCapture *captureIns, const struct AudioHwCaptureParam *handleData)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    captureIns->descPins = descPins;
    return HDF_SUCCESS;
}
#else
static int32_t CaptureSelectSceneImpl(struct AlsaCapture *captureIns, enum AudioPortPin descPins,
    const struct PathDeviceInfo *deviceInfo)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    captureIns->descPins = descPins;
    return HDF_SUCCESS;
}
#endif

static int32_t CaptureGetVolThresholdImpl(struct AlsaCapture *captureIns, long *volMin, long *volMax)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(volMin);
    CHECK_NULL_PTR_RETURN_DEFAULT(volMax);
    int32_t ret = HDF_SUCCESS;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)captureIns;
    CaptureData *priData = CaptureGetPriData(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    /* Query the hardware range to make sure the volume element is available,
     * but report the adapter's abstract volume domain (see CaptureData above). */
    ret = CaptureLoadHwVolRange(cardIns, priData);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    *volMin = 0;
    *volMax = VOLUME_CHANGE;

    return HDF_SUCCESS;
}

static int32_t CaptureGetVolumeImpl(struct AlsaCapture *captureIns, long *volume)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(volume);
    int32_t ret = HDF_SUCCESS;
    long volLeft = 0;
    long volRight = 0;
    long hwSpan;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)captureIns;
    CaptureData *priData = CaptureGetPriData(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    if (priData->volumeValid) {
        /* The hardware granularity is coarser than the abstract domain, so
         * scaling down and back up would not return the requested value. */
        *volume = priData->volume;
        return HDF_SUCCESS;
    }

    ret = CaptureLoadHwVolRange(cardIns, priData);
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

static int32_t CaptureSetVolumeImpl(struct AlsaCapture *captureIns, long volume)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    int32_t ret = HDF_SUCCESS;
    long hwSpan;
    long hwVol;
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)captureIns;
    CaptureData *priData = CaptureGetPriData(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(cardIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);

    if (volume < 0) {
        volume = 0;
    }
    if (volume > VOLUME_CHANGE) {
        volume = VOLUME_CHANGE;
    }
    ret = CaptureLoadHwVolRange(cardIns, priData);
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

static int32_t CaptureSetMuteImpl(struct AlsaCapture *captureIns, bool muteFlag)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    int32_t ret = HDF_SUCCESS;
    long vol = 0;
    long setVol = 0;
    CaptureData *priData = CaptureGetPriData(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    CHECK_NULL_PTR_RETURN_DEFAULT(priData);
    ret = captureIns->GetVolume(captureIns, &vol);
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
    captureIns->SetVolume(captureIns, setVol);
    if (ret != HDF_SUCCESS) {
        AUDIO_FUNC_LOGE("SetVolume failed!");
        return HDF_FAILURE;
    }
    captureIns->muteState = muteFlag;
    return HDF_SUCCESS;
}

#ifdef AUDIO_HAL_P7885
static int32_t CaptureStartImpl(struct AlsaCapture *captureIns, const struct AudioHwCaptureParam *handleData)
{
    return HDF_SUCCESS;
}
#else
static int32_t CaptureStartImpl(struct AlsaCapture *captureIns)
{
    return HDF_SUCCESS;
}
#endif

static int32_t CaptureStopImpl(struct AlsaCapture *captureIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    /* The pcm may have failed to open (for example the device is busy);
     * snd_pcm_drop(NULL) would crash the process, so check it first. */
    if (captureIns->soundCard.pcmHandle == NULL) {
        AUDIO_FUNC_LOGE("pcmHandle is null and return INVALID_PARAM");
        return HDF_ERR_INVALID_PARAM;
    }
    snd_pcm_drop(captureIns->soundCard.pcmHandle);
    return HDF_SUCCESS;
}

static int32_t CaptureGetGainThresholdImpl(struct AlsaCapture *captureIns, float *gainMin, float *gainMax)
{
    AUDIO_FUNC_LOGE("emulator not support gain operation");
    return HDF_SUCCESS;
}

static int32_t CaptureGetGainImpl(struct AlsaCapture *captureIns, float *volume)
{
    AUDIO_FUNC_LOGE("emulator not support gain operation");
    return HDF_SUCCESS;
}

static int32_t CaptureSetGainImpl(struct AlsaCapture *captureIns, float volume)
{
    AUDIO_FUNC_LOGE("emulator not support gain operation");
    return HDF_SUCCESS;
}

static bool CaptureGetMuteImpl(struct AlsaCapture *captureIns)
{
    CHECK_NULL_PTR_RETURN_DEFAULT(captureIns);
    return captureIns->muteState;
}

int32_t CaptureOverrideFunc(struct AlsaCapture *captureIns)
{
    if (captureIns == NULL) {
        return HDF_FAILURE;
    }
    struct AlsaSoundCard *cardIns = (struct AlsaSoundCard *)captureIns;

    if (cardIns->cardType == SND_CARD_PRIMARY) {
        captureIns->Init = CaptureInitImpl;
        captureIns->SelectScene = CaptureSelectSceneImpl;
        captureIns->Start = CaptureStartImpl;
        captureIns->Stop = CaptureStopImpl;
        captureIns->GetVolThreshold = CaptureGetVolThresholdImpl;
        captureIns->GetVolume = CaptureGetVolumeImpl;
        captureIns->SetVolume = CaptureSetVolumeImpl;
        captureIns->GetGainThreshold = CaptureGetGainThresholdImpl;
        captureIns->GetGain = CaptureGetGainImpl;
        captureIns->SetGain = CaptureSetGainImpl;
        captureIns->GetMute = CaptureGetMuteImpl;
        captureIns->SetMute = CaptureSetMuteImpl;
    }
    
    return HDF_SUCCESS;
}