/*
 * Copyright (c) 2022 Shenzhen Kaihong DID Co., Ltd.
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

#ifndef BUFFER_MANAGER_WRAPPER_H
#define BUFFER_MANAGER_WRAPPER_H

#include "codec_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct BufferManagerWrapper {
    void *inputBufferManager;
    void *outputBufferManager;

    bool (*IsInputDataBufferReady)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    CodecBuffer* (*GetInputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    CodecBuffer* (*GetUsedInputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    void (*PutInputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, CodecBuffer *info);
    void (*PutUsedInputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, CodecBuffer *info);

    bool (*IsUsedOutputDataBufferReady)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    CodecBuffer* (*GetOutputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    CodecBuffer* (*GetUsedOutputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, uint32_t timeoutMs);
    void (*PutOutputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, CodecBuffer *info);
    void (*PutUsedOutputDataBuffer)(struct BufferManagerWrapper *bufferManagerWrapper, CodecBuffer *info);
};

struct BufferManagerWrapper* GetBufferManager(void);
void DeleteBufferManager(struct BufferManagerWrapper **ppBufferManager);

#ifdef __cplusplus
}
#endif
#endif  // BUFFER_MANAGER_WRAPPER_H
