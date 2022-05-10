/*
 * Copyright 2022 Shenzhen Kaihong DID Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CODEC_HDI_DECODE_H
#define CODEC_HDI_DECODE_H

#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_VideoExt.h>
#include <ashmem.h>
#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "codec_callback_type_service.h"
#include "codec_callback_type_stub.h"
#include "codec_component_manager.h"
#include "codec_component_type.h"
#include "codec_types.h"

enum class codecMime { AVC, HEVC };

enum class PortIndex { PORT_INDEX_INPUT = 0, PORT_INDEX_OUTPUT = 1 };

struct inParameters {
    codecMime codec = codecMime::AVC;
    std::string path;
    unsigned int width = 144;
    unsigned int height = 176;
};

struct privateApp {
    codecMime codec = codecMime::AVC;
    unsigned int width = 640;
    unsigned int height = 480;
    OMX_COLOR_FORMATTYPE format = OMX_COLOR_FormatYUV420SemiPlanar;
    OMX_U32 framerate = 30;
    OMX_HANDLETYPE handle;
    OMX_PTR pAppData = nullptr;
    bool isSupply_ = false;
};

using OmxCodecBuffer = struct OmxCodecBuffer;
struct BufferInfo {
    std::shared_ptr<OmxCodecBuffer> omxBuffer;
    std::shared_ptr<OHOS::Ashmem> avSharedPtr;
    PortIndex portIndex;
    BufferInfo()
    {
        omxBuffer = nullptr;
        avSharedPtr = nullptr;
        portIndex = PortIndex::PORT_INDEX_INPUT;
    }
    ~BufferInfo()
    {
        omxBuffer = nullptr;
        if (avSharedPtr) {
            avSharedPtr->UnmapAshmem();
            avSharedPtr->CloseAshmem();
            avSharedPtr = nullptr;
        }
        portIndex = PortIndex::PORT_INDEX_INPUT;
    }
};
using BufferInfo = struct BufferInfo;
class CodecHdiDecode {
public:
    explicit CodecHdiDecode();
    ~CodecHdiDecode();
    bool Init(int width, int height, std::string &filename, codecMime codec);
    bool Configure();
    bool UseBuffers();
    void FreeBuffers();
    void Run();
    void Release();
    static int32_t OnEvent(struct CodecCallbackType *self, enum OMX_EVENTTYPE event, struct EventInfo *info);
    static int32_t OnEmptyBufferDone(struct CodecCallbackType *self, int8_t *appData, uint32_t appDataLen,
                                     const struct OmxCodecBuffer *buffer);
    static int32_t OnFillBufferDone(struct CodecCallbackType *self, int8_t *appData, uint32_t appDataLen,
                                    struct OmxCodecBuffer *buffer);
    template <typename T> inline void InitParam(T &param)
    {
        memset_s(&param, sizeof(param), 0x0, sizeof(param));
        param.nSize = sizeof(param);
        param.nVersion.s.nVersionMajor = 1;  // mVersion.s.nVersionMajor;
    }
    void WaitForStatusChanged();
    void onStatusChanged();
    bool ReadOnePacket(FILE *fp, char *buf, uint32_t &filledCount);

private:
    int32_t UseBufferOnPort(PortIndex portIndex);
    int32_t UseBufferOnPort(PortIndex portIndex, int bufferCount, int bufferSize);
    int32_t OnEmptyBufferDone(const struct OmxCodecBuffer &buffer);
    int32_t OnFillBufferDone(struct OmxCodecBuffer &buffer);
    int GetYuvSize();
    int32_t ConfigPortDefine();
    bool FillAllTheBuffer();
    int GetFreeBufferId();

private:
    FILE *fpIn_;  // input file
    FILE *fpOut_;
    unsigned int width_;
    unsigned int height_;
    struct CodecComponentType *client_;
    struct CodecCallbackType *callback_;
    struct CodecComponentManager *omxMgr_;
    std::map<int, std::shared_ptr<BufferInfo>> omxBuffers_;  // key is buferid
    std::list<int> unUsedInBuffers_;
    std::list<int> unUsedOutBuffers_;
    std::mutex lockInputBuffers_;
    std::condition_variable statusCondition_;
    std::mutex statusLock_;
    bool exit_;
    bool isSupply_;
    codecMime codecMime_;
};
#endif /* CODEC_HDI_DECODE_H */