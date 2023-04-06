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

/**
 * @addtogroup Audio
 * @{
 *
 * @brief Defines audio-related APIs, including custom data types and functions for loading drivers,
 * accessing a driver adapter, and rendering and capturing audios.
 *
 * @since 1.0
 * @version 1.0
 */

/**
 * @file i_audio_render.h
 *
 * @brief Declares APIs for audio rendering.
 *
 * @since 1.0
 * @version 1.0
 */

#ifndef I_AUDIO_RENDER_H
#define I_AUDIO_RENDER_H

#include "i_audio_types.h"
#include "i_audio_control.h"
#include "i_audio_attribute.h"
#include "i_audio_scene.h"
#include "i_audio_volume.h"

/**
 * @brief Provides capabilities for audio rendering, including controlling the rendering, setting audio attributes,
 * scenes, and volume, obtaining hardware latency, and rendering audio frames.
 *
 * @see AudioControlHwi
 * @see AudioHwiAttribute
 * @see AudioHwiScene
 * @see AudioHwiVolume
 * @since 1.0
 * @version 1.0
 */
struct AudioHwiRender {
    /**
     * @brief Defines the audio control. For details, see {@link AudioControlHwi}.
     */
    struct AudioControlHwi control;
    /**
     * @brief Defines the audio attribute. For details, see {@link AudioHwiAttribute}.
     */
    struct AudioHwiAttribute attr;
    /**
     * @brief Defines the audio scene. For details, see {@link AudioHwiScene}.
     */
    struct AudioHwiScene scene;
    /**
     * @brief Defines audio volume. For details, see {@link AudioHwiVolume}.
     */
    struct AudioHwiVolume volume;

    /**
     * @brief Obtains the estimated latency of the audio device driver.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param ms Indicates the pointer to the latency (in milliseconds) to be obtained.
     * @return Returns <b>0</b> if the latency is obtained; returns a negative value otherwise.
     */
    int32_t (*GetLatency)(struct AudioHwiRender *render, uint32_t *ms);

    /**
     * @brief Writes a frame of output data (downlink data) into the audio driver for rendering.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param frame Indicates the pointer to the frame to write.
     * @param requestBytes Indicates the size of the frame, in bytes.
     * @param replyBytes Indicates the pointer to the actual length (in bytes) of the audio data to write.
     * @return Returns <b>0</b> if the data is written successfully; returns a negative value otherwise.
     */
    int32_t (*RenderFrame)(struct AudioHwiRender *render, const void *frame, uint64_t requestBytes,
        uint64_t *replyBytes);

    /**
     * @brief Obtains the last number of output audio frames.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param frames Indicates the pointer to the last number of output audio frames.
     * @param time Indicates the pointer to the timestamp associated with the frame.
     * @return Returns <b>0</b> if the last number is obtained; returns a negative value otherwise.
     * @see RenderFrame
     */
    int32_t (*GetRenderPosition)(struct AudioHwiRender *render, uint64_t *frames, struct AudioHwiTimeStamp *time);

    /**
     * @brief Sets the audio rendering speed.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param speed Indicates the rendering speed to set.
     * @return Returns <b>0</b> if the setting is successful; returns a negative value otherwise.
     * @see GetRenderSpeed
     */
    int32_t (*SetRenderSpeed)(struct AudioHwiRender *render, float speed);

    /**
     * @brief Obtains the current audio rendering speed.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param speed Indicates the pointer to the current rendering speed to obtain.
     * @return Returns <b>0</b> if the speed is successfully obtained; returns a negative value otherwise.
     * @see SetRenderSpeed
     */
    int32_t (*GetRenderSpeed)(struct AudioHwiRender *render, float *speed);

    /**
     * @brief Sets the channel mode for audio rendering.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param mode Indicates the channel mode to set.
     * @return Returns <b>0</b> if the setting is successful; returns a negative value otherwise.
     * @see GetChannelMode
     */
    int32_t (*SetChannelMode)(struct AudioHwiRender *render, enum AudioHwiChannelMode mode);

    /**
     * @brief Obtains the current channel mode for audio rendering.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param mode Indicates the pointer to the channel mode to obtain.
     * @return Returns <b>0</b> if the mode is successfully obtained; returns a negative value otherwise.
     * @see SetChannelMode
     */
    int32_t (*GetChannelMode)(struct AudioHwiRender *render, enum AudioHwiChannelMode *mode);

    /**
     * @brief Registers an audio callback that will be invoked during playback when buffer data writing or
     * buffer drain is complete.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param callback Indicates the callback to register.
     * @param cookie Indicates the pointer to the callback parameters.
     * @return Returns <b>0</b> if the operation is successful; returns a negative value otherwise.
     * @see RegCallback
     */
    int32_t (*RegCallback)(struct AudioHwiRender *render, RenderHwiCallback callback, void* cookie);

    /**
     * @brief Drains the buffer.
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param type Indicates the pointer to the execution type of this function. For details,
     * see {@link AudioHwiDrainNotifyType}.
     * @return Returns <b>0</b> if the operation is successful; returns a negative value otherwise.
     * @see RegCallback
     */
    int32_t (*DrainBuffer)(struct AudioHwiRender *render, enum AudioHwiDrainNotifyType *type);

    /**
     * @brief query whether the vendor supports draining buffer
     *
     * @param render Indicates the pointer to the <b>AudioHwiRender</b> object to operate.
     * @param support indicates the state whether the vendor supports draining buffer. Value <b>true</b> means that
     * the vendor supports, and <b>false</b> means the opposite.
     * @return Returns <b>0</b> if the operation is successful; returns a negative value otherwise.
     * @see IsSupportsDrain
     */
    int32_t (*IsSupportsDrain)(struct AudioHwiRender *render, bool *support);
};

#endif /* I_AUDIO_RENDER_H */
/** @} */