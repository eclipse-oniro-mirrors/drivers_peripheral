/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IAM_LOGGER_H
#define IAM_LOGGER_H

#include "hilog/log.h"
namespace OHOS {
namespace UserIam {
namespace Common {
#ifdef __FILE_NAME__
#define USER_AUTH_FILE __FILE_NAME__
#else
#define USER_AUTH_FILE (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif

#define LOG_DOMAIN 0xD002411

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define USER_AUTH_DEBUG(...) HILOG_DEBUG(LOG_CORE, __VA_ARGS__)
#define USER_AUTH_INFO(...) HILOG_INFO(LOG_CORE, __VA_ARGS__)
#define USER_AUTH_WARN(...) HILOG_WARN(LOG_CORE, __VA_ARGS__)
#define USER_AUTH_ERROR(...) HILOG_ERROR(LOG_CORE, __VA_ARGS__)
#define USER_AUTH_FATAL(...) HILOG_FATAL(LOG_CORE, __VA_ARGS__)

#define ARGS(fmt, ...) "[%{public}s@%{public}s:%{public}d] " fmt, __FUNCTION__, USER_AUTH_FILE, __LINE__, ##__VA_ARGS__
#define USER_LOG(level, ...) USER_AUTH_##level(ARGS(__VA_ARGS__))

#define IAM_LOGD(...) USER_LOG(DEBUG, __VA_ARGS__)
#define IAM_LOGI(...) USER_LOG(INFO, __VA_ARGS__)
#define IAM_LOGW(...) USER_LOG(WARN, __VA_ARGS__)
#define IAM_LOGE(...) USER_LOG(ERROR, __VA_ARGS__)
#define IAM_LOGF(...) USER_LOG(FATAL, __VA_ARGS__)

} // namespace Common
} // namespace UserIam
} // namespace OHOS

#endif // IAM_LOGGER_H
