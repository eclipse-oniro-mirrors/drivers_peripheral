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

#ifndef OHOS_BLUETOOTH_A2DP_AUDIO_SRC_OBSERVER_STUB_H
#define OHOS_BLUETOOTH_A2DP_AUDIO_SRC_OBSERVER_STUB_H

#include <map>

#include "iremote_stub.h"
#include "i_bluetooth_a2dp_src_observer.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
#define LOG_DOMAIN 0xD000105

namespace OHOS {
namespace Bluetooth {
class BluetoothA2dpAudioSrcObserverStub : public IRemoteStub<IBluetoothA2dpSourceObserver> {
public:
    BluetoothA2dpAudioSrcObserverStub();
    virtual ~BluetoothA2dpAudioSrcObserverStub();

    virtual int OnRemoteRequest(
        uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

private:
    int32_t OnConnectionStateChangedInner(MessageParcel &data, MessageParcel &reply);
    int32_t OnPlayingStatusChangedInner(MessageParcel &data, MessageParcel &reply);
    int32_t OnConfigurationChangedInner(MessageParcel &data, MessageParcel &reply);
    int32_t OnMediaStackChangedInner(MessageParcel &data, MessageParcel &reply);
    using BluetoothA2dpAudioSrcObserverFunc =
        int32_t (BluetoothA2dpAudioSrcObserverStub::*)(MessageParcel &data, MessageParcel &reply);
    std::map<uint32_t, BluetoothA2dpAudioSrcObserverFunc> funcMap_;

    DISALLOW_COPY_AND_MOVE(BluetoothA2dpAudioSrcObserverStub);
};
}
}
#endif