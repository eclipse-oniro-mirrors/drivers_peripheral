/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "camera_error_log_detector.h"

std::string CameraErrorLogDetector::errLog_;
void CameraErrorLogDetector::RegisterErrLogCallback(
    const LogType type, const LogLevel, const unsigned int domain, const char* tag, const char* msg)
{
    errLog_ = msg;
    std::cout << "errLog: " + errLog_ << std::endl;
}

bool CameraErrorLogDetector::IsErrorLogContains(const std::string& substr)
{
    std::cout << "checking: " + errLog_ << std::endl;
    return errLog_.find(substr) != std::string::npos;
}