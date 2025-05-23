/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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
#ifndef HDI_WPA_COMMON_H
#define HDI_WPA_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD001566

#define WPA_MESSAGE_KEY_LENGTH 64
#define WPA_MESSAGE_VALUE_LENGTH 256
#define IFNAME_LEN_MIN 3
#define IFNAME_LEN_MAX 15

typedef struct StWpaCtrl {
    struct wpa_ctrl *pSend;
    struct wpa_ctrl *pRecv;
} WpaCtrl;

typedef struct StWpaKeyValue {
    char key[WPA_MESSAGE_KEY_LENGTH];
    char value[WPA_MESSAGE_VALUE_LENGTH];
} WpaKeyValue;

#define INIT_WPA_KEY_VALUE {{0}, {0}}

int InitWpaCtrl(WpaCtrl *pCtrl, const char *ifname);
void ReleaseWpaCtrl(WpaCtrl *pCtrl);
int WpaCliCmd(const char *cmd, char *buf, size_t bufLen);
int Hex2Dec(const char *str);
void TrimQuotationMark(char *str, char c);
int IsSockRemoved(const char *ifName, int len);

#ifdef __cplusplus
}
#endif
#endif