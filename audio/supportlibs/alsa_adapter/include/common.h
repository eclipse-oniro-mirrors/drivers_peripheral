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

#ifndef ALSA_SND_COMMON_H
#define ALSA_SND_COMMON_H

/*
 * Mixer elements of the ES1370 sound card (QEMU x86_64_virt). The card is
 * driven by the kernel ens1370 driver whose AK4531 codec exposes these
 * controls. Elements are looked up BY NAME: numids are card-specific (the
 * AK4531 control table registers "Master Playback Switch" first, so its
 * "Master Playback Volume" ends up at numid 2), while the names are stable.
 */

/* Master playback volume (AK4531 master register, 0..31). */
#define     SND_ELEM_DACL_PLAYBACK_VOL       "Master Playback Volume"
#define     SND_ELEM_DACR_PLAYBACK_VOL       "Master Playback Volume"

/* Mic input gain (AK4531 mic register, 0..31) used as the capture volume. */
#define     SND_ELEM_DACL_CAPTURE_VOL        "Mic Volume"
#define     SND_ELEM_DACR_CAPTURE_VOL        "Mic Volume"

#endif /* ALSA_SND_COMMON_H */
