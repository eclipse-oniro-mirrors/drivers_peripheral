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

#include "battery_thread.h"
#include <cerrno>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <linux/netlink.h>
#include "core/hdf_device_desc.h"
#include "utils/hdf_log.h"

#define HDF_LOG_TAG BatteryThread

using namespace OHOS::HDI::Battery::V1_0;

namespace OHOS {
namespace HDI {
namespace Battery {
namespace V1_0 {
const int ERR_INVALID_FD = -1;
const int ERR_OPERATION_FAILED = -1;
const int UEVENT_BUFF_SIZE = (64 * 1024);
const int UEVENT_RESERVED_SIZE = 2;
const int UEVENT_MSG_LEN = (2 * 1024);
const int TIMER_INTERVAL = 10;
const int TIMER_FAST_SEC = 2;
const int TIMER_SLOW_SEC = 10;
const int SEC_TO_MSEC = 1000;
const std::string POWER_SUPPLY = "SUBSYSTEM=power_supply";
static sptr<IBatteryCallback> g_callback;

void BatteryThread::InitCallback(const sptr<IBatteryCallback>& event)
{
    g_callback = event;
    HDF_LOGI("%{public}s: g_callback is %{public}p", __func__, event.GetRefPtr());
}

int32_t BatteryThread::OpenUeventSocket(void) const
{
    int32_t bufferSize = UEVENT_BUFF_SIZE;
    struct sockaddr_nl address = {
        .nl_pid = getpid(),
        .nl_family = AF_NETLINK,
        .nl_groups = 0xffffffff
    };

    int32_t fd = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (fd == ERR_INVALID_FD) {
        HDF_LOGE("%{public}s: open uevent socket failed, fd is invalid", __func__);
        return ERR_INVALID_FD;
    }

    int32_t ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &bufferSize, sizeof(bufferSize));
    if (ret == ERR_OPERATION_FAILED) {
        HDF_LOGE("%{public}s: set socket opt failed, ret: %{public}d", __func__, ret);
        close(fd);
        return ERR_INVALID_FD;
    }

    ret = bind(fd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(struct sockaddr_nl));
    if (ret == ERR_OPERATION_FAILED) {
        HDF_LOGE("%{public}s: bind socket address failed, ret: %{public}d", __func__, ret);
        close(fd);
        return ERR_INVALID_FD;
    }
    return fd;
}

int BatteryThread::RegisterCallback(const int fd, const EventType et)
{
    struct epoll_event ev;

    ev.events = EPOLLIN;
    if (et == EVENT_TIMER_FD) {
        ev.events |= EPOLLWAKEUP;
    }

    ev.data.ptr = reinterpret_cast<void*>(this);
    ev.data.fd = fd;
    if (epoll_ctl(epFd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        HDF_LOGE("%{public}s: epoll_ctl failed, error num =%{public}d", __func__, errno);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

void BatteryThread::SetTimerInterval(int interval)
{
    struct itimerspec itval;

    if (timerFd_ == ERR_INVALID_FD) {
        return;
    }

    timerInterval_ = interval;
    if (interval < 0) {
        interval = 0;
    }

    itval.it_interval.tv_sec = interval;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = interval;
    itval.it_value.tv_nsec = 0;

    if (timerfd_settime(timerFd_, 0, &itval, nullptr) == -1) {
        HDF_LOGE("%{public}s: timer failed\n", __func__);
    }
    return;
}

void BatteryThread::UpdateEpollInterval(const int32_t chargeState)
{
    int interval;
    if ((chargeState != PowerSupplyProvider::CHARGE_STATE_NONE) &&
        (chargeState != PowerSupplyProvider::CHARGE_STATE_RESERVED)) {
        interval = TIMER_FAST_SEC;
        epollInterval_ = interval * SEC_TO_MSEC;
    } else {
        interval = TIMER_SLOW_SEC;
        epollInterval_ = -1;
    }

    if ((interval != timerInterval_) && (timerInterval_ > 0)) {
        SetTimerInterval(interval);
    }
    return;
}

int32_t BatteryThread::InitUevent()
{
    ueventFd_ = OpenUeventSocket();
    if (ueventFd_ == ERR_INVALID_FD) {
        HDF_LOGE("%{public}s: open uevent socket failed, fd is invalid", __func__);
        return HDF_ERR_BAD_FD;
    }

    fcntl(ueventFd_, F_SETFL, O_NONBLOCK);
    callbacks_.insert(std::make_pair(ueventFd_, &BatteryThread::UeventCallback));

    if (RegisterCallback(ueventFd_, EVENT_UEVENT_FD)) {
        HDF_LOGE("%{public}s: register Uevent event failed", __func__);
        return HDF_ERR_BAD_FD;
    }
    return HDF_SUCCESS;
}

int32_t BatteryThread::Init(void* service)
{
    giver_ = std::make_unique<PowerSupplyProvider>();
    if (giver_ != nullptr) {
        giver_->InitBatteryPath();
        giver_->InitPowerSupplySysfs();
    }

    epFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epFd_ == -1) {
        HDF_LOGE("%{public}s: epoll create failed, timerFd_ is invalid", __func__);
        return HDF_ERR_BAD_FD;
    }

    InitTimer();
    InitUevent();

    return HDF_SUCCESS;
}

int BatteryThread::UpdateWaitInterval()
{
    return HDF_FAILURE;
}

int32_t BatteryThread::InitTimer()
{
    timerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ == ERR_INVALID_FD) {
        HDF_LOGE("%{public}s: epoll create failed, timerFd_ is invalid", __func__);
        return HDF_ERR_BAD_FD;
    }

    SetTimerInterval(TIMER_INTERVAL);
    fcntl(timerFd_, F_SETFL, O_NONBLOCK);
    callbacks_.insert(std::make_pair(timerFd_, &BatteryThread::TimerCallback));

    if (RegisterCallback(timerFd_, EVENT_TIMER_FD)) {
        HDF_LOGE("%{public}s: register Timer event failed", __func__);
    }

    return HDF_SUCCESS;
}

void BatteryThread::TimerCallback(void* service)
{
    unsigned long long timers;

    if (read(timerFd_, &timers, sizeof(timers)) == -1) {
        HDF_LOGE("%{public}s: read timerFd_ failed", __func__);
        return;
    }

    UpdateBatteryInfo(service);

    return;
}

void BatteryThread::UeventCallback(void* service)
{
    char msg[UEVENT_MSG_LEN + UEVENT_RESERVED_SIZE] = { 0 };

    int32_t len = recv(ueventFd_, msg, UEVENT_MSG_LEN, 0);
    if (len < 0 || len >= UEVENT_MSG_LEN) {
        HDF_LOGI("%{public}s: recv return msg is invalid, len: %{public}d", __func__, len);
        return;
    }

    // msg separator
    msg[len] = '\0';
    msg[len + 1] = '\0';
    if (!IsPowerSupplyEvent(msg)) {
        return;
    }
    UpdateBatteryInfo(service, msg);
    return;
}

void BatteryThread::UpdateBatteryInfo(void* service, char* msg)
{
    CallbackInfo event;
    std::unique_ptr<BatterydInfo> batteryInfo = std::make_unique<BatterydInfo>();
    if (batteryInfo == nullptr) {
        HDF_LOGE("%{public}s instantiate batteryInfo error", __func__);
        return;
    }

    giver_->ParseUeventToBatterydInfo(msg, batteryInfo.get());
    event.capacity = batteryInfo->capacity_;
    event.voltage= batteryInfo->voltage_;
    event.temperature = batteryInfo->temperature_;
    event.healthState = batteryInfo->healthState_;
    event.pluggedType = batteryInfo->pluggedType_;
    event.pluggedMaxCurrent = batteryInfo->pluggedMaxCurrent_;
    event.pluggedMaxVoltage = batteryInfo->pluggedMaxVoltage_;
    event.chargeState = batteryInfo->chargeState_;
    event.chargeCounter = batteryInfo->chargeCounter_;
    event.present = batteryInfo->present_;
    event.technology = batteryInfo->technology_;

    HDF_LOGI("%{public}s: BatteryInfo capacity=%{public}d, voltage=%{public}d, temperature=%{public}d, " \
        "healthState=%{public}d, pluggedType=%{public}d, pluggedMaxCurrent=%{public}d, " \
        "pluggedMaxVoltage=%{public}d, chargeState=%{public}d, chargeCounter=%{public}d, present=%{public}d, " \
        "technology=%{public}s", __func__, batteryInfo->capacity_, batteryInfo->voltage_,
        batteryInfo->temperature_, batteryInfo->healthState_, batteryInfo->pluggedType_,
        batteryInfo->pluggedMaxCurrent_, batteryInfo->pluggedMaxVoltage_, batteryInfo->chargeState_,
        batteryInfo->chargeCounter_, batteryInfo->present_, batteryInfo->technology_.c_str());

    if (g_callback != nullptr) {
        HDF_LOGI("%{public}s g_callback is not nullptr", __func__);
        g_callback->Update(event);
    } else {
        HDF_LOGI("%{public}s g_callback is nullptr", __func__);
    }
    return;
}

void BatteryThread::UpdateBatteryInfo(void* service)
{
    CallbackInfo event;
    std::unique_ptr<BatterydInfo> batteryInfo = std::make_unique<BatterydInfo>();
    if (batteryInfo == nullptr) {
        HDF_LOGE("%{public}s instantiate batteryInfo error", __func__);
        return;
    }

    giver_->UpdateInfoByReadSysFile(batteryInfo.get());
    event.capacity = batteryInfo->capacity_;
    event.voltage= batteryInfo->voltage_;
    event.temperature = batteryInfo->temperature_;
    event.healthState = batteryInfo->healthState_;
    event.pluggedType = batteryInfo->pluggedType_;
    event.pluggedMaxCurrent = batteryInfo->pluggedMaxCurrent_;
    event.pluggedMaxVoltage = batteryInfo->pluggedMaxVoltage_;
    event.chargeState = batteryInfo->chargeState_;
    event.chargeCounter = batteryInfo->chargeCounter_;
    event.present = batteryInfo->present_;
    event.technology = batteryInfo->technology_;

    HDF_LOGI("%{public}s: BatteryInfo capacity=%{public}d, voltage=%{public}d, temperature=%{public}d, " \
        "healthState=%{public}d, pluggedType=%{public}d, pluggedMaxCurrent=%{public}d, " \
        "pluggedMaxVoltage=%{public}d, chargeState=%{public}d, chargeCounter=%{public}d, present=%{public}d, " \
        "technology=%{public}s", __func__, batteryInfo->capacity_, batteryInfo->voltage_,
        batteryInfo->temperature_, batteryInfo->healthState_, batteryInfo->pluggedType_,
        batteryInfo->pluggedMaxCurrent_, batteryInfo->pluggedMaxVoltage_, batteryInfo->chargeState_,
        batteryInfo->chargeCounter_, batteryInfo->present_, batteryInfo->technology_.c_str());

    if (g_callback != nullptr) {
        HDF_LOGI("%{public}s g_callback is not nullptr", __func__);
        g_callback->Update(event);
    } else {
        HDF_LOGI("%{public}s g_callback is nullptr", __func__);
    }

    return;
}

bool BatteryThread::IsPowerSupplyEvent(const char* msg) const
{
    while (*msg) {
        if (!strcmp(msg, POWER_SUPPLY.c_str())) {
            return true;
        }
        while (*msg++) {} // move to next
    }

    return false;
}

int BatteryThread::LoopingThreadEntry(void* arg)
{
    int nevents = 0;
    size_t cbct = callbacks_.size();
    struct epoll_event events[cbct];

    HDF_LOGI("%{public}s: start batteryd looping", __func__);
    while (true) {
        if (!nevents) {
            CycleMatters();
        }

        HandleStates();

        int timeout = epollInterval_;
        int waitTimeout = UpdateWaitInterval();
        if ((timeout < 0) || (waitTimeout > 0 && waitTimeout < timeout)) {
            timeout = waitTimeout;
        }
        HDF_LOGI("%{public}s: timeout=%{public}d, nevents=%{public}d", __func__, timeout, nevents);

        nevents = epoll_wait(epFd_, events, cbct, timeout);
        if (nevents == -1) {
            continue;
        }

        for (int n = 0; n < nevents; ++n) {
            if (events[n].data.ptr) {
                BatteryThread* func = const_cast<BatteryThread*>(this);
                (callbacks_.find(events[n].data.fd)->second)(func, arg);
            }
        }
    }
}

void BatteryThread::StartThread(void* service)
{
    Init(service);
    Run(service);

    return;
}

void BatteryThread::Run(void* service)
{
    std::make_unique<std::thread>(&BatteryThread::LoopingThreadEntry, this, service)->detach();
}
}  // namespace V1_0
}  // namespace Battery
}  // namespace HDI
}  // namespace OHOS
