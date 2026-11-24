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

#include "pcie_host_service.h"

#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <if_system_ability_manager.h>
#include <iservice_registry.h>
#include <memory>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <system_ability_definition.h>
#include <unistd.h>
#include <vector>

#include "hdf_base.h"
#include "hdf_log.h"
#include "parameter.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
static constexpr uint32_t LOG_DOMAIN = 0xD002516;
constexpr int32_t DSL_KEXT_SIGNING_STATE_LENGTH = 1;
constexpr char DSL_KEXT_SIGNING_ALLOWED = '1';

namespace OHOS {
namespace HDI {
namespace Pcie {
namespace Driver {
constexpr int32_t MODULE_SUFFIX_LEN = 3;
namespace {

constexpr int32_t EDM_SA_LOAD_TIMEOUT_SECONDS = 4;
constexpr int32_t EDM_SA_MAX_LOAD_ATTEMPTS = 3;
constexpr char DSL_KEXT_SIGNING_STATE[] = "security.dsl.kext_signing.state";
constexpr char DSL_KEXT_SIGNING_DENIED[] = "0";
constexpr const char *SANDBOX_ROOT = "/data/service/el1/public/pcie_driver";

bool GetDriverFileName(const std::string &driverPath, std::string &fileName, int32_t &resolveErrorCode)
{
    std::string sandbox(SANDBOX_ROOT);
    sandbox += '/';
    if (driverPath.rfind(sandbox, 0) != 0) {
        errno = EXDEV;
        HDF_LOGE("ResolveDriverPath: path outside sandbox");
        resolveErrorCode = EXDEV;
        return false;
    }
    fileName = driverPath.substr(sandbox.size());
    if (fileName.size() <= MODULE_SUFFIX_LEN || fileName.find('/') != std::string::npos ||
        fileName.compare(fileName.size() - MODULE_SUFFIX_LEN, MODULE_SUFFIX_LEN, ".ko") != 0) {
        errno = EINVAL;
        HDF_LOGE("ResolveDriverPath: invalid driver filename");
        resolveErrorCode = EINVAL;
        return false;
    }
    return true;
}

int32_t ResolveDriverPath(const std::string &driverPath, std::string &resolvedDriverPath, int32_t &driverFd,
    struct stat &validatedFileStatus, int32_t &resolveErrorCode)
{
    driverFd = -1;
    resolveErrorCode = 0;
    std::string fileName;
    if (!GetDriverFileName(driverPath, fileName, resolveErrorCode)) {
        return HDF_ERR_INVALID_PARAM;
    }

    int32_t rootFd = open(SANDBOX_ROOT, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (rootFd < 0) {
        int32_t savedErrno = errno;
        HDF_LOGE(
            "ResolveDriverPath: open sandbox failed errno=%{public}d(%{public}s)", savedErrno, strerror(savedErrno));
        resolveErrorCode = savedErrno;
        return HDF_FAILURE;
    }

    int32_t openedDriverFd = openat(rootFd, fileName.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    int32_t openErrno = errno;
    close(rootFd);
    if (openedDriverFd < 0) {
        HDF_LOGE("ResolveDriverPath: open failed errno=%{public}d(%{public}s)",
            openErrno, strerror(openErrno));
        errno = openErrno;
        resolveErrorCode = openErrno;
        return HDF_FAILURE;
    }

    int32_t statRet = fstat(openedDriverFd, &validatedFileStatus);
    if (statRet != 0 || !S_ISREG(validatedFileStatus.st_mode)) {
        int32_t savedErrno = statRet != 0 ? errno : EINVAL;
        HDF_LOGE("ResolveDriverPath: invalid file errno=%{public}d(%{public}s)",
            savedErrno, strerror(savedErrno));
        close(openedDriverFd);
        errno = savedErrno;
        resolveErrorCode = savedErrno;
        return HDF_FAILURE;
    }

    resolvedDriverPath = driverPath;
    driverFd = openedDriverFd;
    return HDF_SUCCESS;
}

int32_t FinitModule(int32_t driverFd, const std::string &options)
{
    int32_t finitModuleResult = -1;
    errno = ENOSYS;
#ifdef SYS_finit_module
    finitModuleResult = static_cast<int32_t>(syscall(SYS_finit_module, driverFd, options.c_str(), 0));
#endif
    return finitModuleResult;
}

PcieDriverLoadStatus MapKernelErrnoToLoadStatus(int32_t errorCode)
{
    static constexpr struct {
        int32_t errorCode;
        PcieDriverLoadStatus status;
    } mappings[] = {
        {EBUSY, PcieDriverLoadStatus::LOAD_RESOURCE_BUSY},
        {EPERM, PcieDriverLoadStatus::LOAD_OPERATION_NOT_PERMITTED},
        {EACCES, PcieDriverLoadStatus::LOAD_ACCESS_DENIED},
        {ENOMEM, PcieDriverLoadStatus::LOAD_OUT_OF_MEMORY},
        {EIO, PcieDriverLoadStatus::LOAD_IO_ERROR},
        {EBADF, PcieDriverLoadStatus::LOAD_INVALID_FILE_DESCRIPTOR},
#if defined(EBADFD) && EBADFD != EBADF
        {EBADFD, PcieDriverLoadStatus::LOAD_INVALID_FILE_DESCRIPTOR},
#endif
        {ENOSYS, PcieDriverLoadStatus::LOAD_NOT_SUPPORTED},
        {EFAULT, PcieDriverLoadStatus::LOAD_INVALID_MEMORY_ADDRESS},
        {EINVAL, PcieDriverLoadStatus::LOAD_INVALID_ARGUMENT},
        {ENOEXEC, PcieDriverLoadStatus::LOAD_INVALID_MODULE_FORMAT},
        {EEXIST, PcieDriverLoadStatus::LOAD_MODULE_ALREADY_EXISTS},
#ifdef ENOKEY
        {ENOKEY, PcieDriverLoadStatus::LOAD_SIGNING_KEY_MISSING},
#endif
#ifdef EKEYREJECTED
        {EKEYREJECTED, PcieDriverLoadStatus::LOAD_SIGNATURE_REJECTED},
#endif
#ifdef EKEYEXPIRED
        {EKEYEXPIRED, PcieDriverLoadStatus::LOAD_SIGNING_KEY_EXPIRED},
#endif
#ifdef EKEYREVOKED
        {EKEYREVOKED, PcieDriverLoadStatus::LOAD_SIGNING_KEY_REVOKED},
#endif
#ifdef EBADMSG
        {EBADMSG, PcieDriverLoadStatus::LOAD_INVALID_DATA},
#endif
        {EFBIG, PcieDriverLoadStatus::LOAD_FILE_TOO_LARGE},
        {ETXTBSY, PcieDriverLoadStatus::LOAD_FILE_BUSY},
        {ENOENT, PcieDriverLoadStatus::LOAD_REQUIRED_OBJECT_MISSING},
        {ENOSPC, PcieDriverLoadStatus::LOAD_INSUFFICIENT_SPACE},
        {ENODEV, PcieDriverLoadStatus::LOAD_DEVICE_UNAVAILABLE},
    };
    for (const auto &mapping : mappings) {
        if (mapping.errorCode == errorCode) {
            return mapping.status;
        }
    }
    return PcieDriverLoadStatus::LOAD_UNKNOWN;
}

void SetFinitModuleResult(PcieDriverLoadResult &loadResult, int32_t finitModuleResult, int32_t savedErrno)
{
    if (finitModuleResult == 0) {
        loadResult.statusCode = static_cast<int32_t>(PcieDriverLoadStatus::LOADED);
        loadResult.statusMessage = "driver loaded";
    } else {
        HDF_LOGE("PcieDriverHost: finit_module failed errno=%{public}d(%{public}s)", savedErrno, strerror(savedErrno));
        loadResult.statusMessage = "kernel rejected driver load";
        loadResult.statusCode = static_cast<int32_t>(MapKernelErrnoToLoadStatus(savedErrno));
    }
}

} // namespace

PcieDriverHostService::PcieDriverHostService(PcieSysfsScanner scanner, PcieDriverHostDeps injectedDependencies)
    : scanner_(std::move(scanner)), injectedDependencies_(std::move(injectedDependencies))
{
    if (!injectedDependencies_.resolveDriverPath) {
        injectedDependencies_.resolveDriverPath = ResolveDriverPath;
    }
    if (!injectedDependencies_.finitModule) {
        injectedDependencies_.finitModule = FinitModule;
    }
}

int32_t PcieDriverHostService::GetPcieDevices(std::vector<PcieDeviceInfo> &devices)
{
    return scanner_.Scan(devices);
}

bool PcieDriverHostService::EnsureEdmSaLoaded() const
{
    // Tests can inject SA availability and loading functions; otherwise, SAMgr queries and loads the EDM SA.
    auto isEdmSaAvailable = injectedDependencies_.isEdmSaAvailable;
    auto loadEdmSaAndWait = injectedDependencies_.loadEdmSaAndWait;
    if (!isEdmSaAvailable) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        if (!samgr) {
            HDF_LOGE("EDM SA manager unavailable");
            return false;
        }
        // Tests use the injected availability check; production queries SAMgr, where a non-null result means available.
        isEdmSaAvailable = [samgr] {
            return samgr->CheckSystemAbility(HDF_EXTERNAL_DEVICE_MANAGER_SA_ID) != nullptr;
        };
        // Tests inject the loading result; production uses the synchronous SAMgr interface.
        //  SAMgr waits for completion and returns null on loading failure or timeout.
        loadEdmSaAndWait = [samgr] {
            return samgr->LoadSystemAbility(HDF_EXTERNAL_DEVICE_MANAGER_SA_ID, EDM_SA_LOAD_TIMEOUT_SECONDS) != nullptr;
        };
    }
    for (int32_t attempt = 1; attempt <= EDM_SA_MAX_LOAD_ATTEMPTS; ++attempt) {
        if (isEdmSaAvailable()) {
            return true;
        }
        if (!loadEdmSaAndWait) {
            return false;
        }
        bool loaded = loadEdmSaAndWait();
        HDF_LOGI("EDM SA 5110 load attempt=%{public}d succeeded=%{public}d", attempt, loaded);
        if (loaded) {
            return true;
        }
    }
    HDF_LOGE("EDM SA 5110 load failed after %{public}d attempts", EDM_SA_MAX_LOAD_ATTEMPTS);
    return false;
}

bool PcieDriverHostService::StartEdmSaForPcieDevices()
{
    std::vector<PcieDeviceInfo> devices;
    if (GetPcieDevices(devices) != 0) {
        HDF_LOGE("PCIe board scan failed");
        return false;
    }
    if (devices.empty()) {
        HDF_LOGI("PCIe board scan empty, skip EDM SA load");
        return true;
    }
    return EnsureEdmSaLoaded();
}

PcieDriverLoadResult PcieDriverHostService::LoadDriverModule(const PcieDriverLoadItem &loadRequest) const
{
    HDF_LOGI("PCIE_LOAD_TRACE HOST_LOAD_REQUEST board=%{public}s module=%{public}s "
             "options_length=%{public}zu",
        loadRequest.driverIdentity.boardId.c_str(), loadRequest.driverIdentity.moduleName.c_str(),
        loadRequest.options.size());
    PcieDriverLoadResult loadResult {};
    loadResult.driverIdentity = loadRequest.driverIdentity;

    char dslState[PARAM_VALUE_LEN_MAX] = {};
    int32_t dslStateLength = GetParameter(DSL_KEXT_SIGNING_STATE, DSL_KEXT_SIGNING_DENIED, dslState, sizeof(dslState));
    if (dslStateLength != DSL_KEXT_SIGNING_STATE_LENGTH || dslState[0] != DSL_KEXT_SIGNING_ALLOWED) {
        HDF_LOGE("PcieDriverHost: DSL denies module loading, module=%{public}s",
            loadRequest.driverIdentity.moduleName.c_str());
        loadResult.statusCode = static_cast<int32_t>(PcieDriverLoadStatus::LOAD_ACCESS_DENIED);
        loadResult.statusMessage = "DSL denies driver loading or DSL state is unavailable";
        return loadResult;
    }

    std::string resolvedDriverPath;
    struct stat validatedFileStatus = {};
    int32_t driverFd = -1;
    int32_t resolveErrorCode = 0;
    // Tests can replace file resolution and module loading; production validates files and calls finit_module.
    int32_t resolveStatus = injectedDependencies_.resolveDriverPath(
        loadRequest.driverIdentity.driverPath, resolvedDriverPath, driverFd, validatedFileStatus, resolveErrorCode);
    if (resolveStatus != HDF_SUCCESS) {
        HDF_LOGE("PCIE_LOAD_TRACE HOST_PATH_FAILED module=%{public}s status=%{public}d errno=%{public}d",
            loadRequest.driverIdentity.moduleName.c_str(), resolveStatus, resolveErrorCode);
        HDF_LOGE("PcieDriverHost: path REJECTED errno=%{public}d(%{public}s)",
            resolveErrorCode, strerror(resolveErrorCode));
        if (resolveStatus == HDF_ERR_INVALID_PARAM) {
            loadResult.statusCode = static_cast<int32_t>(PcieDriverLoadStatus::PATH_REJECTED);
            loadResult.statusMessage = "driver path rejected";
        } else {
            loadResult.statusCode = static_cast<int32_t>(PcieDriverLoadStatus::OPEN_FAILED);
            loadResult.statusMessage = "driver file open or validation failed";
        }
        return loadResult;
    }
    loadResult.driverIdentity.driverPath = resolvedDriverPath;

    //  Tests use the injected callback; production calls FinitModule to insert the module into the kernel.
    int32_t finitModuleResult = injectedDependencies_.finitModule(driverFd, loadRequest.options);
    int32_t savedErrno = errno; /* save before close() may overwrite it */
    HDF_LOGI("PCIE_LOAD_TRACE HOST_FINIT_RESULT module=%{public}s result=%{public}d errno=%{public}d",
        loadRequest.driverIdentity.moduleName.c_str(), finitModuleResult, savedErrno);
    close(driverFd);

    SetFinitModuleResult(loadResult, finitModuleResult, savedErrno);

    return loadResult;
}

PcieDriverHostExecutionResult PcieDriverHostService::ExecuteDriverLoads(
    const std::vector<PcieDriverLoadItem> &loadRequests)
{
    PcieDriverHostExecutionResult executionResult;
    for (const auto &loadRequest : loadRequests) {
        PcieDriverLoadResult loadResult = LoadDriverModule(loadRequest);
        HDF_LOGI("PCIE_LOAD_TRACE HOST_LOAD_RESULT module=%{public}s status=%{public}d",
            loadResult.driverIdentity.moduleName.c_str(), loadResult.statusCode);
        executionResult.loadResults.push_back(std::move(loadResult));
    }
    HDF_LOGI("PCIE_LOAD_TRACE HOST_EXECUTE_REPLY results=%{public}zu", executionResult.loadResults.size());
    return executionResult;
}

} // namespace Driver
} // namespace Pcie
} // namespace HDI
} // namespace OHOS
