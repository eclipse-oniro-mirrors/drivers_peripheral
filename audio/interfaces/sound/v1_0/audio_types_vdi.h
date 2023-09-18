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

#ifndef OHOS_VDI_AUDIO_V1_0_AUDIOTYPES_H
#define OHOS_VDI_AUDIO_V1_0_AUDIOTYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum AudioPortDirectionVdi {
    PORT_VDI_OUT = 1,
    PORT_VDI_IN = 2,
    PORT_VDI_OUT_IN = 3,
    PORT_VDI_DIR_BUTT,
};

enum AudioPortPinVdi {
    PIN_VDI_NONE = 0,
    PIN_VDI_OUT_SPEAKER = 1 << 0,
    PIN_VDI_OUT_HEADSET = 1 << 1,
    PIN_VDI_OUT_LINEOUT = 1 << 2,
    PIN_VDI_OUT_HDMI = 1 << 3,
    PIN_VDI_OUT_USB = 1 << 4,
    PIN_VDI_OUT_USB_EXT = 1 << 5,
    PIN_VDI_OUT_EARPIECE = 1 << 5 | 1 << 4,
    PIN_VDI_OUT_BLUETOOTH_SCO = 1 << 6,
    PIN_VDI_OUT_DAUDIO_DEFAULT = 1 << 7,
    PIN_VDI_OUT_HEADPHONE = 1 << 8,
    PIN_VDI_OUT_USB_HEADSET = 1 << 9,
    PIN_VDI_IN_MIC = 1 << 27 | 1 << 0,
    PIN_VDI_IN_HS_MIC = 1 << 27 | 1 << 1,
    PIN_VDI_IN_LINEIN = 1 << 27 | 1 << 2,
    PIN_VDI_IN_USB_EXT = 1 << 27 | 1 << 3,
    PIN_VDI_IN_BLUETOOTH_SCO_HEADSET = 1 << 27 | 1 << 4,
    PIN_VDI_IN_DAUDIO_DEFAULT = 1 << 27 | 1 << 5,
    PIN_VDI_IN_USB_HEADSET = 1 << 27 | 1 << 6,
    PIN_VDI_IN_BUTT,
};

enum AudioCategoryVdi {
    AUDIO_VDI_IN_MEDIA = 0,
    AUDIO_VDI_IN_COMMUNICATION = 1,
    AUDIO_VDI_IN_RINGTONE = 2,
    AUDIO_VDI_IN_CALL = 3,
    AUDIO_VDI_MMAP_NOIRQ = 4,
    AUDIO_VDI_CATEGORY_BUTT,
};

enum AudioFormatVdi {
    AUDIO_VDI_FORMAT_TYPE_PCM_8_BIT = 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_PCM_16_BIT = 1 << 1,
    AUDIO_VDI_FORMAT_TYPE_PCM_24_BIT = 1 << 1 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_PCM_32_BIT = 1 << 2,
    AUDIO_VDI_FORMAT_TYPE_AAC_MAIN = 1 << 24 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_AAC_LC = 1 << 24 | 1 << 1,
    AUDIO_VDI_FORMAT_TYPE_AAC_LD = 1 << 24 | 1 << 1 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_AAC_ELD = 1 << 24 | 1 << 2,
    AUDIO_VDI_FORMAT_TYPE_AAC_HE_V1 = 1 << 24 | 1 << 2 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_AAC_HE_V2 = 1 << 24 | 1 << 2 | 1 << 1,
    AUDIO_VDI_FORMAT_TYPE_G711A = 1 << 25 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_G711U = 1 << 25 | 1 << 1,
    AUDIO_VDI_FORMAT_TYPE_G726 = 1 << 25 | 1 << 1 | 1 << 0,
    AUDIO_VDI_FORMAT_TYPE_BUTT,
};

enum AudioChannelMaskVdi {
    AUDIO_VDI_CHANNEL_FRONT_LEFT = 1,
    AUDIO_VDI_CHANNEL_FRONT_RIGHT = 2,
    AUDIO_VDI_CHANNEL_MONO = 1,
    AUDIO_VDI_CHANNEL_STEREO = 3,
    AUDIO_VDI_CHANNEL_MASK_BUTT,
};

enum AudioSampleRatesMaskVdi {
    AUDIO_VDI_SAMPLE_RATE_MASK_8000 = 1 << 0,
    AUDIO_VDI_SAMPLE_RATE_MASK_12000 = 1 << 1,
    AUDIO_VDI_SAMPLE_RATE_MASK_11025 = 1 << 2,
    AUDIO_VDI_SAMPLE_RATE_MASK_16000 = 1 << 3,
    AUDIO_VDI_SAMPLE_RATE_MASK_22050 = 1 << 4,
    AUDIO_VDI_SAMPLE_RATE_MASK_24000 = 1 << 5,
    AUDIO_VDI_SAMPLE_RATE_MASK_32000 = 1 << 6,
    AUDIO_VDI_SAMPLE_RATE_MASK_44100 = 1 << 7,
    AUDIO_VDI_SAMPLE_RATE_MASK_48000 = 1 << 8,
    AUDIO_VDI_SAMPLE_RATE_MASK_64000 = 1 << 9,
    AUDIO_VDI_SAMPLE_RATE_MASK_96000 = 1 << 10,
    AUDIO_VDI_SAMPLE_RATE_MASK_INVALID = 4294967295,
    AUDIO_VDI_SAMPLE_RATE_MASK_BUTT,
};

enum AudioPortPassthroughModeVdi {
    PORT_VDI_PASSTHROUGH_LPCM = 1,
    PORT_VDI_PASSTHROUGH_RAW = 2,
    PORT_VDI_PASSTHROUGH_HBR2LBR = 4,
    PORT_VDI_PASSTHROUGH_AUTO = 8,
    PORT_VDI_PASSTHROUGH_MODE_BUTT,
};

enum AudioSampleFormatVdi {
    AUDIO_VDI_SAMPLE_FORMAT_S8 = 0,
    AUDIO_VDI_SAMPLE_FORMAT_S8P = 1,
    AUDIO_VDI_SAMPLE_FORMAT_U8 = 2,
    AUDIO_VDI_SAMPLE_FORMAT_U8P = 3,
    AUDIO_VDI_SAMPLE_FORMAT_S16 = 4,
    AUDIO_VDI_SAMPLE_FORMAT_S16P = 5,
    AUDIO_VDI_SAMPLE_FORMAT_U16 = 6,
    AUDIO_VDI_SAMPLE_FORMAT_U16P = 7,
    AUDIO_VDI_SAMPLE_FORMAT_S24 = 8,
    AUDIO_VDI_SAMPLE_FORMAT_S24P = 9,
    AUDIO_VDI_SAMPLE_FORMAT_U24 = 10,
    AUDIO_VDI_SAMPLE_FORMAT_U24P = 11,
    AUDIO_VDI_SAMPLE_FORMAT_S32 = 12,
    AUDIO_VDI_SAMPLE_FORMAT_S32P = 13,
    AUDIO_VDI_SAMPLE_FORMAT_U32 = 14,
    AUDIO_VDI_SAMPLE_FORMAT_U32P = 15,
    AUDIO_VDI_SAMPLE_FORMAT_S64 = 16,
    AUDIO_VDI_SAMPLE_FORMAT_S64P = 17,
    AUDIO_VDI_SAMPLE_FORMAT_U64 = 18,
    AUDIO_VDI_SAMPLE_FORMAT_U64P = 19,
    AUDIO_VDI_SAMPLE_FORMAT_F32 = 20,
    AUDIO_VDI_SAMPLE_FORMAT_F32P = 21,
    AUDIO_VDI_SAMPLE_FORMAT_F64 = 22,
    AUDIO_VDI_SAMPLE_FORMAT_F64P = 23,
    AUDIO_VDI_SAMPLE_FORMAT_BUTT,
};

enum AudioChannelModeVdi {
    AUDIO_VDI_CHANNEL_NORMAL = 0,
    AUDIO_VDI_CHANNEL_BOTH_LEFT = 1,
    AUDIO_VDI_CHANNEL_BOTH_RIGHT = 2,
    AUDIO_VDI_CHANNEL_EXCHANGE = 3,
    AUDIO_VDI_CHANNEL_MIX = 4,
    AUDIO_VDI_CHANNEL_LEFT_MUTE = 5,
    AUDIO_VDI_CHANNEL_RIGHT_MUTE = 6,
    AUDIO_VDI_CHANNEL_BOTH_MUTE = 7,
    AUDIO_VDI_CHANNEL_MODE_BUTT,
};

enum AudioDrainNotifyTypeVdi {
    AUDIO_VDI_DRAIN_NORMAL_MODE = 0,
    AUDIO_VDI_DRAIN_EARLY_MODE = 1,
    AUDIO_VDI_DRAIN_TYPE_BUTT,
};

enum AudioCallbackTypeVdi {
    AUDIO_VDI_NONBLOCK_WRITE_COMPLETED = 0,
    AUDIO_VDI_DRAIN_COMPLETED = 1,
    AUDIO_VDI_FLUSH_COMPLETED = 2,
    AUDIO_VDI_RENDER_FULL = 3,
    AUDIO_VDI_ERROR_OCCUR = 4,
    AUDIO_VDI_CALLBACK_TYPE_BUTT,
};

enum AudioPortRoleVdi {
    AUDIO_VDI_PORT_UNASSIGNED_ROLE = 0,
    AUDIO_VDI_PORT_SOURCE_ROLE = 1,
    AUDIO_VDI_PORT_SINK_ROLE = 2,
    AUDIO_VDI_PORT_ROLE_BUTT,
};

enum AudioPortTypeVdi {
    AUDIO_VDI_PORT_UNASSIGNED_TYPE = 0,
    AUDIO_VDI_PORT_DEVICE_TYPE = 1,
    AUDIO_VDI_PORT_MIX_TYPE = 2,
    AUDIO_VDI_PORT_SESSION_TYPE = 3,
     AUDIO_VDI_PORT_TYPE_BUTT,
};

enum AudioSessionTypeVdi {
    AUDI_VDI_OUTPUT_STAGE_SESSION = 0,
    AUDI_VDI_OUTPUT_MIX_SESSION = 1,
    AUDI_VDI_ALLOCATE_SESSION = 2,
    AUDI_VDI_INVALID_SESSION = 3,
    AUDI_VDI_SESSION_TYPE_BUTT,
};

enum AudioDeviceTypeVdi {
    AUDIO_VDI_LINEOUT = 1 << 0,
    AUDIO_VDI_HEADPHONE = 1 << 1,
    AUDIO_VDI_HEADSET = 1 << 2,
    AUDIO_VDI_USB_HEADSET = 1 << 3,
    AUDIO_VDI_USB_HEADPHONE = 1 << 4,
    AUDIO_VDI_USBA_HEADSET = 1 << 5,
    AUDIO_VDI_USBA_HEADPHONE = 1 << 6,
    AUDIO_VDI_PRIMARY_DEVICE = 1 << 7,
    AUDIO_VDI_USB_DEVICE = 1 << 8,
    AUDIO_VDI_A2DP_DEVICE = 1 << 9,
    AUDIO_VDI_DEVICE_UNKNOWN,
    AUDIO_VDI_DEVICE_TYPE_BUTT,
};

enum AudioEventTypeVdi {
    AUDIO_VDI_DEVICE_ADD = 1,
    AUDIO_VDI_DEVICE_REMOVE = 2,
    AUDIO_VDI_LOAD_SUCCESS = 3,
    AUDIO_VDI_LOAD_FAILURE = 4,
    AUDIO_VDI_UNLOAD = 5,
    AUDIO_VDI_SERVICE_VALID = 7,
    AUDIO_VDI_SERVICE_INVALID = 8,
    AUDIO_VDI_CAPTURE_THRESHOLD = 9,
    AUDIO_VDI_EVENT_UNKNOWN = 10,
    AUDIO_VDI_EVENT_TYPE_BUTT,
};

enum AudioExtParamKeyVdi {
    AUDIO_VDI_EXT_PARAM_KEY_NONE = 0,
    AUDIO_VDI_EXT_PARAM_KEY_VOLUME = 1,
    AUDIO_VDI_EXT_PARAM_KEY_FOCUS = 2,
    AUDIO_VDI_EXT_PARAM_KEY_BUTTON = 3,
    AUDIO_VDI_EXT_PARAM_KEY_EFFECT = 4,
    AUDIO_VDI_EXT_PARAM_KEY_STATUS = 5,
    AUDIO_VDI_EXT_PARAM_KEY_USB_DEVICE = 101, 
    AUDIO_VDI_EXT_PARAM_KEY_LOWPOWER = 1000,
    AUDIO_VDI_EXT_PARAM_KEY_BUTT,
};

struct AudioDeviceStatusVdi {
    uint32_t pnpStatus;
} __attribute__ ((aligned(8)));

union SceneDescVdi {
    uint32_t id;
}  __attribute__ ((aligned(8)));

struct AudioPortVdi {
    enum AudioPortDirectionVdi dir;
    uint32_t portId;
    char *portName;
};

struct AudioAdapterDescriptorVdi {
    char *adapterName;
    struct AudioPortVdi *ports;
    uint32_t portsLen;
};

struct AudioDeviceDescriptorVdi {
    uint32_t portId;
    enum AudioPortPinVdi pins;
    char *desc;
};

struct AudioSceneDescriptorVdi {
    union SceneDescVdi scene;
    struct AudioDeviceDescriptorVdi desc;
};

enum AudioInputTypeVdi {
    AUDIO_VDI_INPUT_DEFAULT_TYPE             = 0,
    AUDIO_VDI_INPUT_MIC_TYPE                 = 1 << 0,
    AUDIO_VDI_INPUT_SPEECH_WAKEUP_TYPE       = 1 << 1,
    AUDIO_VDI_INPUT_VOICE_COMMUNICATION_TYPE = 1 << 2,
    AUDIO_VDI_INPUT_VOICE_RECOGNITION_TYPE   = 1 << 3,
};

struct AudioSampleAttributesVdi {
    enum AudioCategoryVdi type;
    bool interleaved;
    enum AudioFormatVdi format;
    uint32_t sampleRate;
    uint32_t channelCount;
    uint32_t period;
    uint32_t frameSize;
    bool isBigEndian;
    bool isSignedData;
    uint32_t startThreshold;
    uint32_t stopThreshold;
    uint32_t silenceThreshold;
    int32_t streamId;
    int32_t sourceType;
} __attribute__ ((aligned(8)));

struct AudioTimeStampVdi {
    int64_t tvSec;
    int64_t tvNSec;
} __attribute__ ((aligned(8)));

struct AudioSubPortCapabilityVdi {
    uint32_t portId;
    char *desc;
    enum AudioPortPassthroughModeVdi mask;
};

struct AudioPortCapabilityVdi {
    uint32_t deviceType;
    uint32_t deviceId;
    bool hardwareMode;
    uint32_t formatNum;
    enum AudioFormatVdi *formats;
    uint32_t formatsLen;
    uint32_t sampleRateMasks;
    enum AudioChannelMaskVdi channelMasks;
    uint32_t channelCount;
    struct AudioSubPortCapabilityVdi *subPorts;
    uint32_t subPortsLen;
    enum AudioSampleFormatVdi *supportSampleFormats;
    uint32_t supportSampleFormatsLen;
};

struct AudioMmapBufferDescriptorVdi {
    int8_t *memoryAddress;
    uint32_t memoryAddressLen;
    int32_t memoryFd;
    int32_t totalBufferFrames;
    int32_t transferFrameSize;
    int32_t isShareable;
    uint32_t offset;
    char *filePath;
};

struct AudioDevExtInfoVdi {
    int32_t moduleId;
    enum AudioPortPinVdi type;
    char *desc;
};

struct AudioMixExtInfoVdi {
    int32_t moduleId;
    int32_t streamId;
} __attribute__ ((aligned(8)));

struct AudioSessionExtInfoVdi {
    enum AudioSessionTypeVdi sessionType;
} __attribute__ ((aligned(8)));

struct AudioInfoVdi {
    struct AudioDevExtInfoVdi device;
    struct AudioMixExtInfoVdi mix;
    struct AudioSessionExtInfoVdi session;
};

struct AudioRouteNodeVdi {
    int32_t portId;
    enum AudioPortRoleVdi role;
    enum AudioPortTypeVdi type;
    struct AudioInfoVdi ext;
};

struct AudioRouteVdi {
    struct AudioRouteNodeVdi *sources;
    uint32_t sourcesLen;
    struct AudioRouteNodeVdi *sinks;
    uint32_t sinksLen;
};

struct AudioEventVdi {
    uint32_t eventType;
    uint32_t deviceType;
} __attribute__ ((aligned(8)));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OHOS_VDI_AUDIO_V1_0_AUDIOTYPES_H */