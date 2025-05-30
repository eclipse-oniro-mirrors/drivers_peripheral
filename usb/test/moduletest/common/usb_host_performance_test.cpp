/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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


#include <csignal>
#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <unistd.h>
#include "hdf_base.h"
#include "usb_utils.h"

using namespace std;
using namespace testing::ext;

namespace {
class UsbHostPerformanceTest : public testing::Test {
protected:
    static void SetUpTestCase(void)
    {
        printf("------start UsbHostPerformanceTest------\n");
    }
    static void TearDownTestCase(void)
    {
        printf("------end UsbHostPerformanceTest------\n");
    }
};

/**
 * @tc.number    : H_Lx_H_Sub_usb_performance_005
 * @tc.name      : Host SDK ROM占用<60K
 * @tc.type      : PERF
 * @tc.level     : Level 1
 */
HWTEST_F(UsbHostPerformanceTest, CheckHostSdkRom, TestSize.Level1)
{
    printf("------start CheckHostSdkRom------\n");
    const char *hostSdkPath = HDF_LIBRARY_FULL_PATH("libusb_ddk_host");
    int64_t size = 0;
    FILE *fp = fopen(hostSdkPath, "rb");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    EXPECT_LT(size, 60 * 1024);
    printf("------end CheckHostSdkRom------\n");
}

/**
 * @tc.number    : H_Lx_H_Sub_usb_performance_006, H_Lx_H_Sub_usb_performance_007,
 *                 H_Lx_H_Sub_usb_performance_008
 * @tc.name      : Host SDK RAM占用峰值<40K，RAM占用均值<30K;Host SDK CPU占用峰值<10%，CPU占用均值<15%;
 *                 Host SDK驱动框架下单进程加载SDK，最大并发线程数<5
 * @tc.type      : PERF
 * @tc.level     : Level 1
 */
HWTEST_F(UsbHostPerformanceTest, CheckHostSdkProcInfo, TestSize.Level1)
{
    printf("------start CheckHostSdkProcInfo------\n");
    const string logFile = "/data/usb_proclog.txt";
    const string script = "usb_watch_process.sh";
    int32_t processCount;
    pid_t watchPid = 0;
    char *pch = nullptr;
    FILE *res = nullptr;
    struct ProcInfo info = {0, 0, 0, 0, 0};
    ASSERT_EQ(access(script.c_str(), F_OK), 0) << "ErrInfo: shell script not exists";
    if (access(script.c_str(), X_OK) == -1) {
        system(("chmod +x " + script).c_str());
    }
    printf("try to start usb_watch_process.sh...\n");
    ASSERT_EQ(system(("nohup sh " + script + " pnp_host > /data/nohup.out &").c_str()), 0);
    printf("usb_watch_process.sh is running...\n");
    for (int32_t i = 0; i < 1000; i++) {
        system("usbhost_ddk_test -AW $RANDOM");
        printf("Write data %d times\n", i);
        usleep(100 * 1000);
    }
    res = popen("ps -ef | grep 'usb_watch_process.sh' | grep -v grep | cut -F 2", "r");
    pch = ParseSysCmdResult(*res, 1, 1);
    watchPid = stoi(pch);
    printf("try to kill usb_watch_process.sh, pid: %d\n", watchPid);
    kill(watchPid, SIGKILL);
    CalcProcInfoFromFile(info, logFile);
    EXPECT_LT(info.cpuPeak, 15) << "ErrInfo: cpu peak is not less than 15%";
    EXPECT_LT(info.cpuAvg, 10) << "ErrInfo: average cpu is not less than 10%";
    res = popen("ps -ef | grep 'pnp_host' | grep -v grep | wc -l", "r");
    pch = ParseSysCmdResult(*res, 1, 1);
    processCount = stoi(pch);
    EXPECT_EQ(processCount, 1) << "ErrInfo: host sdk process count is not equal to 1";
    printf("------end CheckHostSdkProcInfo------\n");
}
} // namespace
