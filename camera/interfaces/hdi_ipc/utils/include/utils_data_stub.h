/*
 * Copyright (c) 2021 - 2023 Huawei Device Co., Ltd.
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

#ifndef HDI_UTILS_DATA_STUB_INF_H
#define HDI_UTILS_DATA_STUB_INF_H

#include <list>
#include <map>
#include <vector>
#include <message_parcel.h>
#include <iservmgr_hdi.h>
#include "v1_0/types.h"
#include "camera_metadata_info.h"

namespace OHOS::Camera {
using namespace OHOS::HDI::Camera::V1_0;
class UtilsDataStub {
public:
    static bool EncodeCameraMetadata(const std::shared_ptr<CameraMetadata> &metadata,
        MessageParcel &data);
    static void DecodeCameraMetadata(MessageParcel &data, std::shared_ptr<CameraMetadata> &metadata);
    static bool EncodeStreamInfo(const std::shared_ptr<StreamInfo> &pInfo, MessageParcel &parcel);
    static void DecodeStreamInfo(MessageParcel &parcel, std::shared_ptr<StreamInfo> &pInfo);
    static bool ConvertMetadataToVec(const std::shared_ptr<CameraMetadata> &metadata,
        std::vector<uint8_t>& cameraAbility);
    static void ConvertVecToMetadata(const std::vector<uint8_t> &cameraAbility,
        std::shared_ptr<CameraMetadata> &metadata);

private:
    static int32_t GetDataSize(uint8_t type);
    static bool WriteMetadata(const camera_metadata_item_t &entry, MessageParcel &data);
    static bool ReadMetadata(camera_metadata_item_t &entry, MessageParcel &data);
    static void EntryDataToBuffer(const camera_metadata_item_t &entry, void **buffer);
    static bool WriteMetadataDataToVec(const camera_metadata_item_t &entry, std::vector<uint8_t> &cameraAbility);
    static bool ReadMetadataDataFromVec(int32_t &index, camera_metadata_item_t &entry,
        const std::vector<uint8_t>& cameraAbility);
    template <class T> std::enable_if_t<std::is_pod<T>, void> WriteData(T data, std::vector<uint8_t> &cameraAbility);
    template <class T> std::enable_if_t<std::is_pod<T>, void> ReadData(T &data, int32_t &index,
        const std::vector<uint8_t> &cameraAbility);
};

template <class T>
static std::enable_if_t<std::is_pod<T>, void> UtilsDataStub::WriteData(T data, std::vector<uint8_t> &cameraAbility)
{
    T dataTemp = data;
    uint8_t *dataPtr = (uint8_t *)&dataTemp;
    for (size_t j = 0; j < sizeof(T); j++) {
        cameraAbility.push_back(*(dataPtr + j));
    }
}

template <class T>
static std::enable_if_t<std::is_pod<T>, void> UtilsDataStub::ReadData(T &data, int32_t &index,
    const std::vector<uint8_t> &cameraAbility)
{
    constexpr uint32_t typeLen = sizeof(T);
    uint8_t array[typeLen] = {0};
    T *ptr = nullptr;
    for (size_t j = 0; j < sizeof(T); j++) {
        array[j] = cameraAbility.at(index++);
    }
    ptr = (T *)array;
    data = *ptr;
}
}
#endif // HDI_UTILS_DATA_STUB_INF_H
