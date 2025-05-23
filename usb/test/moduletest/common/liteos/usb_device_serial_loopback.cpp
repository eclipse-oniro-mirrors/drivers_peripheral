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

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <unistd.h>
#include "securec.h"
#include "lib_acm_test.h"
#include "hdf_base.h"

using namespace std;
using namespace testing::ext;

namespace {
class UsbDeviceSerialLoopback : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

void UsbDeviceSerialLoopback::SetUpTestCase()
{
    AcmOpen();
}

void UsbDeviceSerialLoopback::TearDownTestCase()
{
    AcmClose();
}

HWTEST_F(UsbDeviceSerialLoopback, DeviceSerialLoopback, TestSize.Level1)
{
    printf("------start DeviceSerialLoopback------\n");
    char data[256] = {0};
    int32_t ret = 0;
    while (1) {
        AcmRead(data);
        if (strlen(data) > 0) {
            if (strcmp(data, "q") == 0) {
                break;
            }
            AcmWrite(data);
            ret = memset_s(data, sizeof(data), 0, sizeof(data));
            EXPECT_EQ(HDF_SUCCESS, ret);
        }
    }
    printf("------end DeviceSerialLoopback------\n");
}
}
