/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "pcie_sysfs_scanner.h"
#include "hdf_base.h"
#include "hdf_log.h"

#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
static constexpr uint32_t LOG_DOMAIN = 0xD002516;

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace Driver {

static std::string Trim(std::string text)
{
    auto leadingEnd = std::find_if(text.begin(), text.end(), [](unsigned char character) {
        return character != '\n' && character != '\r' && character != '\t' && character != ' ';
    });
    text.erase(text.begin(), leadingEnd);
    auto trailingBegin = std::find_if(text.rbegin(), text.rend(), [](unsigned char character) {
        return character != '\n' && character != '\r' && character != '\t' && character != ' ';
    });
    text.erase(trailingBegin.base(), text.end());
    return text;
}

static bool IsDir(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

PcieSysfsScanner::PcieSysfsScanner(std::string sysfsRoot) : sysfsRoot_(std::move(sysfsRoot)) {}

std::string PcieSysfsScanner::ReadOneLine(const std::string &path) const
{
    std::ifstream inputStream(path);
    std::string line;
    return std::getline(inputStream, line) ? Trim(line) : "";
}

PcieDeviceInfo PcieSysfsScanner::BuildDeviceInfo(const std::string &devicePath) const
{
    PcieDeviceInfo boardInfo;
    auto slash = devicePath.find_last_of('/');
    boardInfo.boardId = slash != std::string::npos ? devicePath.substr(slash + 1) : devicePath;
    boardInfo.sysfsPath = devicePath;
    boardInfo.vendorId = ReadOneLine(devicePath + "/vendor");
    boardInfo.deviceId = ReadOneLine(devicePath + "/device");
    boardInfo.classCode = ReadOneLine(devicePath + "/class");
    boardInfo.modalias = ReadOneLine(devicePath + "/modalias");

    /* Read bound driver from /sys/.../driver symlink target */
    // Check whether the board is currently bound to a driver.
    char linkTarget[256];
    std::string driverPath = devicePath + "/driver";
    ssize_t len = readlink(driverPath.c_str(), linkTarget, sizeof(linkTarget) - 1);
    if (len > 0) {
        linkTarget[len] = '\0';
        std::string target(linkTarget);
        auto lastSlash = target.find_last_of('/');
        boardInfo.boundDriver = lastSlash != std::string::npos ? target.substr(lastSlash + 1) : target;
    }

    boardInfo.isModaliasValid = !boardInfo.boardId.empty() && !boardInfo.vendorId.empty() &&
        !boardInfo.deviceId.empty() && !boardInfo.modalias.empty();
    return boardInfo;
}

int32_t PcieSysfsScanner::Scan(std::vector<PcieDeviceInfo> &deviceInfos) const
{
    deviceInfos.clear();
    std::string devicesRoot = sysfsRoot_ + "/devices";
    DIR *dir = opendir(devicesRoot.c_str());
    if (!dir) {
        HDF_LOGE("PcieScanner: opendir FAILED errno=%{public}d(%{public}s)", errno,
            strerror(errno));
        return HDF_FAILURE;
    }
    struct dirent *entry = nullptr;
    for (errno = 0; (entry = readdir(dir)) != nullptr; errno = 0) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        std::string devicePath = devicesRoot + "/" + name;
        if (!IsDir(devicePath)) {
            continue;
        }

        PcieDeviceInfo boardInfo = BuildDeviceInfo(devicePath);
        deviceInfos.push_back(std::move(boardInfo));
    }
    const int readError = errno;
    if (readError != 0) {
        HDF_LOGE("PcieScanner: readdir failed errno=%{public}d", readError);
        closedir(dir);
        deviceInfos.clear();
        return HDF_FAILURE;
    }
    closedir(dir);
    if (deviceInfos.empty()) {
        HDF_LOGW("PcieScanner: scan returned 0 PCIe devices in sysfs");
    }
    return HDF_SUCCESS;
}

} // namespace Driver
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
