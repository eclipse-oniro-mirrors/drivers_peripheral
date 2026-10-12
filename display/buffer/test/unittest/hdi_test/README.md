# Display Buffer HDI 测试用例说明文档

## 1. 概述

本文档介绍 `display/buffer` 模块 HDI 接口测试用例的设计思路、测试单元说明、参数意义及代码逐行解析。

测试用例覆盖 `display/buffer` 模块从 v1_0 到 v1_4 的全部接口，包括三大子接口：
- **IAllocator**（内存分配器）：`AllocMem`、`CloneDmaBufferHandle`
- **IMapper**（内存映射器）：`FreeMem`、`Mmap`、`Unmap`、`FlushCache`、`InvalidateCache`、`GetImageLayout`、`AllocMem`、`ReAllocMem`、`IsSupportAllocPassthrough`
- **IMetadata**（元数据管理器）：`RegisterBuffer`、`SetMetadata`、`GetMetadata`、`ListMetadataKeys`、`EraseMetadataKey`

以及统一入口接口 **IDisplayBuffer**，它封装了以上三个子接口的全部方法。

## 2. 文件结构

| 文件 | 说明 |
|------|------|
| `display_buffer_hdi_test.h` | 公共头文件，定义测试类、常量、公共辅助方法 |
| `display_buffer_hdi_test.cpp` | 统一 IDisplayBuffer 接口测试（v1_0~v1_4 全部方法） |
| `allocator_interface_test.cpp` | IAllocator 接口直调测试 |
| `mapper_interface_test.cpp` | IMapper 接口直调测试 |
| `metadata_interface_test.cpp` | IMetadata 接口直调测试 |
| `BUILD.gn` | 构建配置文件 |

## 3. 接口版本说明

| 版本 | 接口 | 方法 | 说明 |
|------|------|------|------|
| v1_0 | IAllocator | AllocMem | 分配 DMA 内存 |
| v1_0 | IMapper | FreeMem | 释放内存 |
| v1_0 | IMapper | Mmap | 将内存映射到进程地址空间 |
| v1_0 | IMapper | Unmap | 取消内存映射 |
| v1_0 | IMapper | FlushCache | 将缓存数据刷写到内存 |
| v1_0 | IMapper | InvalidateCache | 使缓存失效，从内存更新 |
| v1_0 | IDisplayBuffer | IsSupportedAlloc | 检查分配信息是否可分配 |
| v1_1 | IMetadata | RegisterBuffer | IPC 后初始化 Buffer |
| v1_1 | IMetadata | SetMetadata | 设置元数据 |
| v1_1 | IMetadata | GetMetadata | 获取元数据 |
| v1_1 | IMetadata | ListMetadataKeys | 列出所有元数据键 |
| v1_1 | IMetadata | EraseMetadataKey | 按键擦除元数据 |
| v1_2 | IMapper | GetImageLayout | 获取图像布局信息 |
| v1_3 | IMapper | AllocMem | 通过 Mapper 直调分配内存（Passthrough） |
| v1_3 | IMapper | ReAllocMem | 基于已有 Buffer 重新分配 |
| v1_3 | IMapper | IsSupportAllocPassthrough | 检查是否支持 Passthrough 分配 |
| v1_4 | IAllocator | CloneDmaBufferHandle | 克隆 DMA Buffer 句柄 |

## 4. 关键数据结构说明

### 4.1 AllocInfo

```cpp
struct AllocInfo {
    unsigned int width;          // 请求内存的宽度（像素）
    unsigned int height;         // 请求内存的高度（像素）
    unsigned long usage;         // 内存用途标志位（如 HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE）
    unsigned int format;         // 像素格式（如 PIXEL_FMT_RGBX_8888）
    unsigned int expectedSize;   // 请求者指定的期望大小（0 表示由驱动决定）
};
```

### 4.2 VerifyAllocInfo

```cpp
struct VerifyAllocInfo {
    unsigned int width;          // 内存宽度
    unsigned int height;         // 内存高度
    unsigned long usage;         // 内存用途
    unsigned int format;         // 像素格式
};
```

### 4.3 ImageLayout (v1_2)

```cpp
struct ImagePlane {
    unsigned int offset;         // 平面偏移量
    unsigned int hStride;        // 水平增量（行间距）
    unsigned int vStride;        // 垂直增量（平面间距）
};

struct ImageLayout {
    unsigned int pixelFormat;           // 图像格式
    struct ImagePlane[] planes;         // 平面数据数组
};
```

### 4.4 常用 Usage 标志位

| 标志位 | 值 | 说明 |
|--------|-----|------|
| `HBM_USE_CPU_READ` | 0x2 | CPU 可读 |
| `HBM_USE_CPU_WRITE` | 0x1 | CPU 可写 |
| `HBM_USE_MEM_DMA` | 0x1000 | 使用 DMA 内存 |
| `HBM_USE_CPU_HW_BOTH` | 0x10000 | CPU 和硬件共用 |
| `HBM_USE_HW_COMPOSER` | 0x800 | 硬件合成器使用 |
| `HBM_USE_VIDEO_DECODER` | 0x400 | 视频解码器使用 |

### 4.5 常用像素格式

| 格式 | 说明 |
|------|------|
| `PIXEL_FMT_RGBX_8888` | 32位 RGBX（不透明 Alpha） |
| `PIXEL_FMT_RGBA_8888` | 32位 RGBA |
| `PIXEL_FMT_BGRA_8888` | 32位 BGRA |
| `PIXEL_FMT_YCBCR_420_SP` | YCbCr 4:2:0 半平面（NV12） |
| `PIXEL_FMT_YCRCB_420_SP` | YCrCb 4:2:0 半平面（NV21） |
| `PIXEL_FMT_YCBCR_420_P` | YCbCr 4:2:0 平面（I420） |
| `PIXEL_FMT_YCRCB_420_P` | YCrCb 4:2:0 平面（YV12） |
| `PIXEL_FMT_RGBA_1010102` | 32位 RGBA 10位色深 |

## 5. 测试类说明

### 5.1 DisplayBufferHdiTest

统一接口测试类，通过 `V1_4::IDisplayBuffer::Get()` 获取接口实例，测试全部继承的接口方法。

**公共辅助方法：**

| 方法 | 说明 |
|------|------|
| `AllocBuffer(info)` | 调用 `AllocMem` 分配 Buffer，返回 `BufferHandle*` |
| `FreeBuffer(handle)` | 调用 `FreeMem` 释放 Buffer |
| `FullLifecycleTest(info)` | 完整生命周期测试：分配->映射->失效缓存->写数据->刷缓存->取消映射->释放 |
| `MetadataFullTest(handle)` | 元数据完整流程：注册->设置->获取->列键->擦除->验证 |
| `ImageLayoutTest(handle)` | 获取图像布局信息并验证 |
| `CloneBufferTest(handle)` | 克隆 DMA Buffer 句柄并验证 |

### 5.2 AllocatorInterfaceTest

直接调用 `IAllocator` 接口的测试类。

| 方法 | 说明 |
|------|------|
| `DoAlloc(info)` | 通过 `IAllocator::AllocMem` 分配 `NativeBuffer` |

### 5.3 MapperInterfaceTest

直接调用 `IMapper` 接口的测试类。

| 方法 | 说明 |
|------|------|
| `AllocViaMapper(info)` | 通过 `V1_3::IMapper::AllocMem` 分配 `NativeBuffer` |

### 5.4 MetadataInterfaceTest

直接调用 `IMetadata` 接口的测试类。

| 方法 | 说明 |
|------|------|
| `AllocBufferForMetadata(info)` | 通过 `IAllocator::AllocMem` 分配 Buffer 供元数据测试使用 |

## 6. 测试用例逐行解析

### 6.1 display_buffer_hdi_test.cpp

#### test_AllocMem001 — RGBX_8888 格式分配

```cpp
// 构造分配信息：宽度1080、高度1920、DMA内存+CPU读写、RGBX_8888格式
AllocInfo info = {
    .width = ALLOC_WIDTH_1080,
    .height = ALLOC_HEIGHT_1920,
    .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
    .format = PIXEL_FMT_RGBX_8888,
    .expectedSize = 0
};
BufferHandle* handle = nullptr;                    // 初始化句柄指针为空
int32_t ret = displayBuffer_->AllocMem(info, handle);  // 调用 AllocMem 分配内存
EXPECT_EQ(ret, HDF_SUCCESS);                       // 期望返回成功
EXPECT_NE(handle, nullptr);                        // 期望句柄不为空
if (handle != nullptr) {
    EXPECT_GE(handle->size, 0);                    // 期望分配的内存大小 >= 0
    EXPECT_EQ(handle->width, static_cast<int>(ALLOC_WIDTH_1080));  // 验证宽度
    EXPECT_EQ(handle->height, static_cast<int>(ALLOC_HEIGHT_1920)); // 验证高度
    FreeBuffer(handle);                            // 释放分配的内存
}
```

#### test_AllocMem002~008 — 多格式分配

每个测试用例使用不同的像素格式（RGBA_8888、BGRA_8888、YCBCR_420_SP、YCRCB_420_SP、YCBCR_420_P、YCRCB_420_P）和不同的 usage 标志位（仅 DMA、DMA+读、DMA+写、CPU_HW_BOTH 等）调用 `AllocMem`，验证返回 `HDF_SUCCESS` 且句柄非空。

#### test_AllocMem013~014 — 不同分辨率分配

测试 2560x1080 和 640x480 等不同分辨率下的内存分配。

#### test_AllocMemRepeat — 重复分配释放

```cpp
for (int32_t i = 0; i < TEST_REPEAT_COUNT; i++) {  // 循环10次
    BufferHandle* handle = nullptr;
    int32_t ret = displayBuffer_->AllocMem(info, handle);  // 每次分配
    EXPECT_EQ(ret, HDF_SUCCESS);
    EXPECT_NE(handle, nullptr);
    FreeBuffer(handle);                            // 每次释放
}
```

#### test_Mmap001 — 内存映射

```cpp
BufferHandle* handle = AllocBuffer(info);          // 分配 Buffer
ASSERT_NE(handle, nullptr);
void* vAddr = displayBuffer_->Mmap(*handle);       // 映射到进程地址空间
EXPECT_NE(vAddr, nullptr);                         // 期望返回非空虚拟地址
if (vAddr != nullptr) {
    displayBuffer_->Unmap(*handle);                // 取消映射
}
FreeBuffer(handle);                                // 释放
```

#### test_FlushCache001 — 刷写缓存

```cpp
void* vAddr = displayBuffer_->Mmap(*handle);       // 先映射
ASSERT_NE(vAddr, nullptr);
int32_t ret = displayBuffer_->FlushCache(*handle); // 将 CPU 写入的数据刷到内存
EXPECT_EQ(ret, HDF_SUCCESS);                       // 期望成功
displayBuffer_->Unmap(*handle);                    // 取消映射
FreeBuffer(handle);
```

#### test_InvalidateCache001 — 使缓存失效

```cpp
void* vAddr = displayBuffer_->Mmap(*handle);       // 先映射
ASSERT_NE(vAddr, nullptr);
int32_t ret = displayBuffer_->InvalidateCache(*handle);  // 使缓存失效，从内存重新加载
EXPECT_EQ(ret, HDF_SUCCESS);
displayBuffer_->Unmap(*handle);
FreeBuffer(handle);
```

#### test_FullLifecycle001 — 完整生命周期

```cpp
// AllocBuffer: 分配内存
// Mmap: 映射到进程地址空间
// InvalidateCache: 读取前使缓存失效
// memset_s: 写入测试数据 (0x55)
// FlushCache: 将写入的数据刷到内存
// InvalidateCache: 再次使缓存失效
// Unmap: 取消映射
// FreeMem: 释放内存
int32_t ret = FullLifecycleTest(info);
EXPECT_EQ(ret, HDF_SUCCESS);
```

#### test_MetadataRegisterBuffer001 — 注册 Buffer

```cpp
BufferHandle* handle = AllocBuffer(info);          // 分配 Buffer
ASSERT_NE(handle, nullptr);
int32_t ret = displayBuffer_->RegisterBuffer(*handle);  // 注册 Buffer（IPC 后初始化）
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);  // 成功或不支持均可
FreeBuffer(handle);
```

#### test_MetadataSetAndGet001 — 设置并获取元数据

```cpp
displayBuffer_->RegisterBuffer(*handle);           // 先注册
uint32_t key = METADATA_KEY_DEFAULT;               // 元数据键 = 1
std::vector<uint8_t> setValue = {0x10, 0x20, 0x30, 0x40};  // 设置值
int32_t ret = displayBuffer_->SetMetadata(*handle, key, setValue);  // 设置元数据
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);

std::vector<uint8_t> getValue;                     // 获取值的容器
ret = displayBuffer_->GetMetadata(*handle, key, getValue);  // 获取元数据
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
if (ret == HDF_SUCCESS) {
    EXPECT_EQ(getValue, setValue);                 // 验证获取值 == 设置值
}
```

#### test_MetadataListKeys001 — 列出元数据键

```cpp
displayBuffer_->SetMetadata(*handle, METADATA_KEY_DEFAULT, setValue);  // 设置3个键
displayBuffer_->SetMetadata(*handle, METADATA_KEY_COLOR, setValue);
displayBuffer_->SetMetadata(*handle, METADATA_KEY_CROP, setValue);

std::vector<uint32_t> keys;                        // 键列表容器
int32_t ret = displayBuffer_->ListMetadataKeys(*handle, keys);  // 列出所有键
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
```

#### test_MetadataEraseKey001 — 擦除元数据键

```cpp
displayBuffer_->SetMetadata(*handle, METADATA_KEY_DEFAULT, setValue);  // 先设置
int32_t ret = displayBuffer_->EraseMetadataKey(*handle, METADATA_KEY_DEFAULT);  // 擦除
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);

if (ret == HDF_SUCCESS) {
    std::vector<uint8_t> getValue;
    ret = displayBuffer_->GetMetadata(*handle, METADATA_KEY_DEFAULT, getValue);  // 擦除后获取
    EXPECT_NE(ret, HDF_SUCCESS);                  // 期望获取失败
}
```

#### test_GetImageLayout001 — 获取图像布局

```cpp
ImageLayout layout;                                // 布局结构体
int32_t ret = displayBuffer_->GetImageLayout(*handle, layout);  // 获取布局
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
if (ret == HDF_SUCCESS) {
    // layout.pixelFormat: 图像格式
    // layout.planes: 平面数组，每个包含 offset/hStride/vStride
}
```

#### test_ReAllocMem001 — 重新分配内存

```cpp
BufferHandle* inBuffer = AllocBuffer(allocInfo);   // 先分配原始 Buffer
ASSERT_NE(inBuffer, nullptr);

AllocInfo reallocInfo = {
    .width = ALLOC_WIDTH_1920,                     // 新宽度
    .height = ALLOC_HEIGHT_1080,                   // 新高度
    .usage = HBM_USE_MEM_DMA | HBM_USE_VIDEO_DECODER | HBM_USE_HW_COMPOSER,  // 新用途
    .format = PIXEL_FMT_YCBCR_420_P,               // 新格式
    .expectedSize = 0
};
BufferHandle* outBuffer = nullptr;
int32_t ret = displayBuffer_->ReAllocMem(reallocInfo, *inBuffer, outBuffer);  // 重新分配
EXPECT_EQ(ret, HDF_SUCCESS);
EXPECT_NE(outBuffer, nullptr);

FreeBuffer(inBuffer);                              // 释放原始 Buffer
if (outBuffer != nullptr) {
    FreeBuffer(outBuffer);                         // 释放新 Buffer
}
```

#### test_CloneDmaBufferHandle001 — 克隆 DMA Buffer 句柄

```cpp
BufferHandle* handle = AllocBuffer(info);          // 分配原始 Buffer
ASSERT_NE(handle, nullptr);
int32_t ret = CloneBufferTest(*handle);            // 克隆并验证
// CloneBufferTest 内部:
//   BufferHandle* clonedHandle = nullptr;
//   displayBuffer_->CloneDmaBufferHandle(handle, clonedHandle);  // 克隆句柄
//   验证 clonedHandle 的 width/height/format 与原始一致
//   FreeBuffer(clonedHandle);                    // 释放克隆的 Buffer
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
FreeBuffer(handle);
```

#### test_IsSupportedAlloc001 — 检查分配支持

```cpp
std::vector<VerifyAllocInfo> infos;                // 验证信息数组
VerifyAllocInfo info1 = {
    .width = ALLOC_WIDTH_1080,
    .height = ALLOC_HEIGHT_1920,
    .usage = HBM_USE_MEM_DMA | HBM_USE_CPU_READ | HBM_USE_CPU_WRITE,
    .format = PIXEL_FMT_RGBX_8888
};
infos.push_back(info1);                            // 添加验证信息

std::vector<bool> supporteds;                      // 支持结果数组
int32_t ret = displayBuffer_->IsSupportedAlloc(infos, supporteds);  // 检查是否支持
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
```

#### test_AllocMmapWriteFlushUnmapFree — 数据完整性验证

```cpp
BufferHandle* handle = AllocBuffer(info);          // 分配
void* vAddr = displayBuffer_->Mmap(*handle);        // 映射
int32_t ret = displayBuffer_->InvalidateCache(*handle);  // 使缓存失效
// 写入测试数据：每个字节 = (i & 0xFF)
for (uint32_t i = 0; i < handle->size; i++) {
    static_cast<uint8_t*>(vAddr)[i] = static_cast<uint8_t>(i & 0xFF);
}
ret = displayBuffer_->FlushCache(*handle);          // 刷缓存
ret = displayBuffer_->InvalidateCache(*handle);     // 再次失效
// 验证数据完整性：读回每个字节检查是否匹配
bool dataValid = true;
for (uint32_t i = 0; i < handle->size; i++) {
    if (static_cast<uint8_t*>(vAddr)[i] != static_cast<uint8_t>(i & 0xFF)) {
        dataValid = false;
        break;
    }
}
EXPECT_TRUE(dataValid);                            // 期望数据完整
displayBuffer_->Unmap(*handle);                    // 取消映射
FreeBuffer(handle);                                // 释放
```

### 6.2 allocator_interface_test.cpp

#### test_AllocatorAllocMem001~010 — 直接调用 IAllocator::AllocMem

每个用例直接通过 `IAllocator::Get(true)` 获取分配器接口，调用 `AllocMem(info, handle)` 分配 `NativeBuffer`。参数覆盖不同格式、分辨率和 usage 组合。

#### test_AllocatorAllocMemVerifyHandle — 验证句柄属性

```cpp
sptr<NativeBuffer> handle = nullptr;
int32_t ret = allocator_->AllocMem(info, handle);  // 直接通过 IAllocator 分配
BufferHandle* buffer = handle->GetBufferHandle();  // 获取底层 BufferHandle
EXPECT_EQ(buffer->width, static_cast<int>(ALLOC_WIDTH_1080));   // 验证宽度
EXPECT_EQ(buffer->height, static_cast<int>(ALLOC_HEIGHT_1920)); // 验证高度
EXPECT_EQ(buffer->format, PIXEL_FMT_RGBX_8888);   // 验证格式
EXPECT_GT(buffer->size, 0);                        // 验证大小 > 0
EXPECT_GE(buffer->fd, 0);                          // 验证 fd >= 0
```

#### test_CloneDmaBufferHandle001~003 — V1_4 克隆句柄

```cpp
allocatorV1_4_ = V1_4::IAllocator::Get(true);     // 获取 V1_4 分配器
sptr<NativeBuffer> inHandle = nullptr;
allocator_->AllocMem(info, inHandle);               // 分配原始 Buffer

sptr<NativeBuffer> outHandle = nullptr;
int32_t ret = allocatorV1_4_->CloneDmaBufferHandle(inHandle, outHandle);  // 克隆
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
if (ret == HDF_SUCCESS) {
    // 验证克隆后的 Buffer 属性与原始一致
    BufferHandle* inBuffer = inHandle->GetBufferHandle();
    BufferHandle* outBuffer = outHandle->GetBufferHandle();
    EXPECT_EQ(outBuffer->width, inBuffer->width);
    EXPECT_EQ(outBuffer->height, inBuffer->height);
    EXPECT_EQ(outBuffer->format, inBuffer->format);
}
```

### 6.3 mapper_interface_test.cpp

#### test_MapperFreeMem001~002 — 直接调用 IMapper::FreeMem

```cpp
sptr<NativeBuffer> handle = AllocViaMapper(info);  // 通过 V1_3 IMapper 分配
int32_t ret = mapper_->FreeMem(handle);            // 通过 IMapper 释放
EXPECT_EQ(ret, HDF_SUCCESS);
```

#### test_MapperMmap001~003 — 直接调用 IMapper::Mmap

```cpp
int32_t ret = mapper_->Mmap(handle);               // 映射，返回值表示成功/失败
EXPECT_EQ(ret, HDF_SUCCESS);
BufferHandle* buffer = handle->GetBufferHandle();
EXPECT_NE(buffer->virAddr, nullptr);               // 验证虚拟地址非空
```

#### test_MapperFlushCache001~003 — 直接调用 IMapper::FlushCache

```cpp
mapper_->Mmap(handle);                              // 先映射
BufferHandle* buffer = handle->GetBufferHandle();
memset_s(buffer->virAddr, buffer->size, 0x77, buffer->size);  // 写入数据
int32_t ret = mapper_->FlushCache(handle);         // 刷缓存
EXPECT_EQ(ret, HDF_SUCCESS);
```

#### test_MapperGetImageLayout001~003 — 直接调用 IMapper::GetImageLayout

```cpp
ImageLayout layout;
int32_t ret = mapper_->GetImageLayout(handle, layout);  // 获取布局
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
if (ret == HDF_SUCCESS) {
    EXPECT_GE(layout.planes.size(), 0);            // 平面数 >= 0
}
```

#### test_MapperAllocMem001~003 — V1_3 Passthrough 分配

```cpp
mapperV1_3_ = V1_3::IMapper::Get(true);           // 获取 V1_3 IMapper
sptr<NativeBuffer> handle = nullptr;
int32_t ret = mapperV1_3_->AllocMem(info, handle);  // 通过 Mapper 直调分配
EXPECT_EQ(ret, HDF_SUCCESS);
```

#### test_MapperReAllocMem001~003 — V1_3 重新分配

```cpp
sptr<NativeBuffer> inHandle = AllocViaMapper(allocInfo);  // 先分配
sptr<NativeBuffer> outHandle = nullptr;
int32_t ret = mapperV1_3_->ReAllocMem(reallocInfo, inHandle, outHandle);  // 重新分配
EXPECT_EQ(ret, HDF_SUCCESS);
mapper_->FreeMem(outHandle);                       // 释放新 Buffer
mapper_->FreeMem(inHandle);                        // 释放原始 Buffer
```

#### test_MapperIsSupportAllocPassthrough001~003 — 检查 Passthrough 支持

```cpp
int32_t ret = mapperV1_3_->IsSupportAllocPassthrough(info);  // 检查是否支持
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
// HDF_SUCCESS: 支持 Passthrough
// HDF_ERR_NOT_SUPPORT: 不支持
```

#### test_MapperFullWorkflow001 — Mapper 完整工作流

```cpp
sptr<NativeBuffer> handle = AllocViaMapper(info);  // 分配
mapper_->Mmap(handle);                              // 映射
mapper_->InvalidateCache(handle);                   // 失效缓存
memset_s(buffer->virAddr, buffer->size, 0x99, buffer->size);  // 写 0x99
mapper_->FlushCache(handle);                        // 刷缓存
mapper_->InvalidateCache(handle);                   // 再次失效
// 验证每个字节是否为 0x99
mapper_->Unmap(handle);                             // 取消映射
mapper_->FreeMem(handle);                           // 释放
```

### 6.4 metadata_interface_test.cpp

#### test_MetadataRegisterBuffer001~003 — 直接调用 IMetadata::RegisterBuffer

```cpp
sptr<NativeBuffer> handle = AllocBufferForMetadata(info);  // 通过 IAllocator 分配
int32_t ret = metadata_->RegisterBuffer(handle);   // 直接通过 IMetadata 注册
EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT);
```

#### test_MetadataSetMetadata001~005 — 直接调用 IMetadata::SetMetadata

每个用例设置不同大小的 value：
- 001: 4 字节 `{0x01, 0x02, 0x03, 0x04}`
- 002: 256 字节（全 0x55）
- 003: 空数组 `{}`
- 004: 1 字节 `{0xFF}`
- 005: 16 字节数组

#### test_MetadataGetMetadata001~002 — 直接调用 IMetadata::GetMetadata

```cpp
metadata_->SetMetadata(handle, key, setValue);     // 先设置
std::vector<uint8_t> getValue;
int32_t ret = metadata_->GetMetadata(handle, key, getValue);  // 获取
if (ret == HDF_SUCCESS) {
    EXPECT_EQ(getValue, setValue);                 // 验证值一致
}
```

#### test_MetadataListMetadataKeys001~003 — 列出元数据键

- 001: 无元数据时列出（期望空列表）
- 002: 设置1个键后列出
- 003: 设置3个键后列出并验证

#### test_MetadataEraseMetadataKey001~003 — 擦除元数据键

- 001: 擦除已存在的键
- 002: 擦除不存在的键
- 003: 擦除后验证键不存在

#### test_MetadataFullWorkflow001~003 — 元数据完整流程

```cpp
metadata_->RegisterBuffer(handle);                 // 1. 注册
metadata_->SetMetadata(handle, key, setValue);     // 2. 设置
metadata_->GetMetadata(handle, key, getValue);     // 3. 获取并验证
metadata_->ListMetadataKeys(handle, keys);         // 4. 列键
metadata_->EraseMetadataKey(handle, key);          // 5. 擦除
metadata_->GetMetadata(handle, key, getValue);     // 6. 验证已擦除
```

#### test_MetadataOverwriteKey — 覆盖写入

```cpp
metadata_->SetMetadata(handle, key, value1);       // 第一次设置
metadata_->SetMetadata(handle, key, value2);       // 覆盖
metadata_->GetMetadata(handle, key, getValue);     // 获取
EXPECT_EQ(getValue, value2);                       // 验证是第二次的值
```

## 7. 测试覆盖矩阵

| 接口方法 | 统一接口测试 | 直调接口测试 | 边界/异常测试 | 重复/压力测试 |
|---------|:---:|:---:|:---:|:---:|
| AllocMem (v1_0) | 14 用例 | 10 用例 | - | 2 用例 |
| FreeMem (v1_0) | 1 用例 | 2 用例 | - | - |
| Mmap (v1_0) | 2 用例 | 3 用例 | - | - |
| Unmap (v1_0) | 1 用例 | 2 用例 | 1 用例 | - |
| FlushCache (v1_0) | 2 用例 | 3 用例 | - | - |
| InvalidateCache (v1_0) | 2 用例 | 2 用例 | - | - |
| IsSupportedAlloc (v1_0) | 2 用例 | - | - | - |
| RegisterBuffer (v1_1) | 1 用例 | 3 用例 | - | 1 用例 |
| SetMetadata (v1_1) | 2 用例 | 5 用例 | 1 用例 | - |
| GetMetadata (v1_1) | 2 用例 | 2 用例 | 2 用例 | - |
| ListMetadataKeys (v1_1) | 2 用例 | 3 用例 | - | - |
| EraseMetadataKey (v1_1) | 2 用例 | 3 用例 | - | - |
| GetImageLayout (v1_2) | 3 用例 | 3 用例 | - | 1 用例 |
| ReAllocMem (v1_3) | 3 用例 | 3 用例 | 1 用例 | - |
| IsSupportAllocPassthrough (v1_3) | - | 3 用例 | - | - |
| AllocMem (v1_3 Passthrough) | - | 3 用例 | - | - |
| CloneDmaBufferHandle (v1_4) | 3 用例 | 3 用例 | - | 1 用例 |

## 8. 构建与运行

### 构建

```bash
./build.sh --product-name <product_name> --build-target hdf_disp_buffer_hdi_test
```

### 运行

```bash
hdc shell
# 测试二进制路径
/system/bin/hdf_disp_buffer_hdi_test
```

### 查看结果

测试使用 GTest 框架，输出格式为标准 GTest 输出：
```
[==========] Running N tests from M test suites.
[----------] Global test environment set-up.
[----------] N tests from DisplayBufferHdiTest
[ RUN      ] DisplayBufferHdiTest.test_AllocMem001
[       OK ] DisplayBufferHdiTest.test_AllocMem001 (123 ms)
...
[==========] N tests from M test suites ran. (12345 ms total)
[  PASSED  ] N tests.
[  FAILED  ] 0 tests.
```

## 9. 注意事项

1. **服务依赖**：测试需要 `allocator_host`、`mapper_host`、`metadata_host` 服务已启动。
2. **硬件依赖**：部分测试依赖 DMA 内存支持，需在真机或支持 DMA 的模拟器上运行。
3. **Passthrough 模式**：`IsSupportAllocPassthrough` 和 V1_3 `AllocMem` 的行为取决于设备是否支持 Passthrough 模式。
4. **Metadata 支持**：`RegisterBuffer` 等元数据接口在某些设备上可能返回 `HDF_ERR_NOT_SUPPORT`，测试用 `EXPECT_TRUE(ret == HDF_SUCCESS || ret == HDF_ERR_NOT_SUPPORT)` 兼容此情况。
5. **CloneDmaBufferHandle**：V1_4 接口需设备支持 DMA Buffer 克隆功能，不支持时返回 `HDF_ERR_NOT_SUPPORT`。
