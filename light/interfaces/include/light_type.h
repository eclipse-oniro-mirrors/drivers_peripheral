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

/**
 * @addtogroup Light
 * @{
 *
 * @brief Provides APIs for the light service.
 *
 * The light module provides a unified interface for the light service to access the light driver.
 * After obtaining the light driver object or proxy, the service can call related APIs to obtain light information,
 * turn on or off a light, and set the light blinking mode based on the light id.
 * @since 3.1
 */

/**
 * @file light_type.h
 *
 * @brief Defines the light data structure, including the light id, lighting mode,
 * blinking mode and duration, return values, and lighting effect.
 * @since 3.1
 */

#ifndef LIGHT_TYPE_H
#define LIGHT_TYPE_H

#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define NAME_MAX_LEN    16

/**
 * @brief Enumerates the return values of the light module.
 *
 * @since 3.1
 */
enum LightStatus {
    /** The operation is successful. */
    LIGHT_SUCCESS            = 0,
    /** The light id is not supported. */
    LIGHT_NOT_SUPPORT        = -1,
    /** Blinking setting is not supported. */
    LIGHT_NOT_FLASH          = -2,
    /** Brightness setting is not supported. */
    LIGHT_NOT_BRIGHTNESS     = -3,
};

/**
 * @brief Enumerates the light ids.
 *
 * @since 3.1
 */
enum LightId {
    /** Unknown id */
    LIGHT_ID_NONE                = 0,
    /** Power light */
    LIGHT_ID_BATTERY             = 1,
    /** Notification light */
    LIGHT_ID_NOTIFICATIONS       = 2,
    /** Alarm light */
    LIGHT_ID_ATTENTION           = 3,
    /** Invalid id */
    LIGHT_ID_BUTT                = 4,
};

/**
 * @brief Enumerates the light types.
 *
 * @since 3.2
 */
enum HdfLightType {
    HDF_LIGHT_TYPE_SINGLE_COLOR = 1,
    HDF_LIGHT_TYPE_RGB_COLOR = 2,
    HDF_LIGHT_TYPE_WRGB_COLOR = 3,
};

/**
 * @brief Enumerates the lighting modes.
 *
 * @since 3.1
 */
enum LightFlashMode {
    /** Steady on */
    LIGHT_FLASH_NONE     = 0,
    /** Blinking */
    LIGHT_FLASH_BLINK    = 1,
    /** Gradient */
    LIGHT_FLASH_GRADIENT = 2,
    /** Invalid mode */
    LIGHT_FLASH_BUTT     = 3,
};

/**
 * @brief Defines the blinking parameters.
 *
 * The parameters include the blinking mode and the on and off time for the light in a blinking period.
 *
 * @since 3.1
 */
struct LightFlashEffect {
    int32_t flashMode; /** Blinking mode. For details, see {@link LightFlashMode}. */
    int32_t onTime;      /** Duration (in ms) for which the light is on in a blinking period. */
    int32_t offTime;     /** Duration (in ms) for which the light is off in a blinking period. */
};

/**
 * @brief Defines the color and extended information of the light.
 *
 * The parameters include four parameters include red, green, blue values and extended information.
 *
 * @since 3.2
 */
struct RGBColor {
    uint8_t r;          /** Red value range 0-255. */
    uint8_t g;          /** Green value range 0-255. */
    uint8_t b;          /** Blue value range 0-255. */
    uint8_t reserved;  /** Custom extended information. */
};

/**
 * @brief Defines the color of the light.
 *
 * The parameters include the These parameters include red, green, blue and white values.
 *
 * @since 3.2
 */
struct WRGBColor {
    uint8_t w;    /** White value range 0-255. */
    uint8_t r;    /** Red value range 0-255. */
    uint8_t g;    /** Green value range 0-255. */
    uint8_t b;    /** Blue value range 0-255. */
};

/**
 * @brief Defines three modes for setting the light.
 *
 * The parameters include single color mode, RGB mode and WRGB mode.
 *
 * @since 3.2
 */
union ColorValue {
    int32_t singleColor;         /** Single color mode. */
    struct WRGBColor wrgbColor;  /** WRGB mode, see {@link WRGBColor}. */
    struct RGBColor rgbColor;    /** RGB mode, see {@link RGBColor}. */
};

/**
 * @brief Defines two modes for setting the light.
 *
 * The parameters include RGB mode and WRGB mode.
 *
 * @since 3.2
 */

struct LightColor {
    union ColorValue colorValue;
};

/**
 * @brief Defines the lighting effect parameters.
 *
 * The parameters include the brightness and blinking mode.
 *
 * @since 3.1
 */
struct LightEffect {
    struct LightColor lightColor;    /** Color and brightness corresponding to light, see {@link HdfLightColor}. */
    struct LightFlashEffect flashEffect;    /** Blinking mode. For details, see {@link LightFlashEffect}. */
};

/**
 * @brief Defines the basic light information.
 *
 * Basic light information includes the light id, light name, light number and light type.
 *
 * @since 3.1
 */
struct LightInfo {
    uint32_t lightId;                /** Light id. For details, see {@link LightId}. */
    uint32_t lightNumber;            /** Number of physical lights in the logic controller. */
    char lightName[NAME_MAX_LEN];    /** Logic light name. */
    int32_t lightType;                /** Light type. For details, see {@link HdfLightType}. */
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* LIGHT_TYPE_H */
/** @} */
