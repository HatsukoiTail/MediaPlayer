# 实现计划：GPU 硬件解码 + Avalonia 渲染

## 目标

在 Avalonia 12.0 / .NET 10 中实现 4K60fps 视频播放，采用 `d3d11va-copy` 路径（GPU 解码 + DMA 下载 + GPU shader YUV→RGB）。

## 已验证的性能基准

- **路径**：mpv `d3d11va-copy`（D3D11VA 硬解 → `av_hwframe_transfer_data` → GL 双平面上传 → shader YUV→RGB）
- **4K60 实测**：58.1 FPS，零掉帧
- **单帧渲染**：平均 13.4ms（预算 16.7ms）
- **ANGLE 版本**：Edge Chromium 147 的 `libGLESv2.dll` / `libEGL.dll`（2.1.45125，比 Avalonia 自带的功能更完整）

## 架构

```
┌─ 解码线程 ─────────────────────┐    ┌─ 渲染线程 ───────────────────────┐
│                                │    │                                  │
│  FFmpeg D3D11VA 硬解           │    │  glTexSubImage2D (Y 纹理)        │
│       ↓                        │    │  glTexSubImage2D (UV 纹理)       │
│  av_hwframe_transfer_data      │    │       ↓                          │
│  (同步 DMA: NV12→CPU)          │    │  glUseProgram (YUV→RGB shader)   │
│       ↓                        │    │       ↓                          │
│  原始 NV12 数据 → [帧队列] →   │    │  glDrawArrays (全屏四边形)       │
│                                │    │                                  │
└────────────────────────────────┘    └──────────────────────────────────┘
```

- 解码线程 DMA 阻塞被帧队列缓冲吸收
- 渲染线程从队列取 NV12 帧，上传双平面纹理，shader 做 YUV→RGB
- CPU 零格式转换！`av_hwframe_transfer_data` 只做原始 NV12 搬运

## 项目结构

```
Texture/
├── FFmpeg-CPP/          # C++ FFmpeg 封装（DLL）
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── Demux/           # 解复用
│   ├── Native/          # AVFrame/AVPacket/CodecContext 包装
│   ├── FrameLoader/     # 帧加载器
│   ├── Queue/           # 线程安全帧队列 + 包队列
│   └── Transcode/       # 转码/格式转换
│
├── Avalonia-UI/         # C# UI 项目
│   ├── Controls/
│   │   └── VideoView.cs # OpenGL shader + 纹理上传（双平面需改造）
│   ├── Views/           # MainWindow
│   └── Program.cs
│
├── build_fc/            # FFmpeg-CPP cmake 构建目录
├── .vscode/             # launch.json, tasks.json
├── IMPLEMENTATION_PLAN.md
└── GPU_RENDERING_FINDINGS.md  # 详细技术调研记录
```

## 待做工作

### FFmpeg-CPP 侧

1. **修复编译错误**：`Demuxer.cpp` 中成员名不匹配
   - `audio_packets` → `audio_packet_queue`
   - `video_packets` → `video_packet_queue`
   - `subtitle_packets` → `subtitle_packet_queue`
   - `.push()` → `.enqueue()`
   - 声明 `audio_idx`、`video_idx` 成员

2. **补充缺失文件**：检查 `Native/` 下的 `.h`/`.cpp` 是否完整

3. **暴露 C API**：FFmpeg-CPP 编译为 DLL，提供 P/Invoke 接口给 C#：
   ```c
   // 核心函数
   FFmpeg_OpenFile(const char* path, int* width, int* height);
   FFmpeg_DecodeFrame(AVFrame** outFrame);  // 硬件解码一帧
   FFmpeg_TransferFrame(AVFrame* hwFrame, uint8_t** outNV12Data, int* outStride);
   FFmpeg_Close();
   ```

4. **ANGLE 设备传递**：C++ 侧需接收 ANGLE 的 `ID3D11Device*` 来初始化 `AVD3D11VADeviceContext`（参考之前 HwDecodeTest Phase B 的做法）

### Avalonia-UI 侧

1. **改造 VideoView.cs 为双平面渲染**：
   - 创建 2 个 GL 纹理（Y + UV）
   - `glTexSubImage2D` 分别上传
   - YUV→RGB shader（参考 HwDecodeTest Phase D）
   ```
   // YUV→RGB (BT.601 limited range)
   float y = 1.164 * (texture2D(uTexY, vTc).r - 0.0625);
   float u = texture2D(uTexUV, vTc).r - 0.5;
   float v = texture2D(uTexUV, vTc).g - 0.5;
   ```

2. **获取 ANGLE D3D11 设备**：
   ```csharp
   var eglDisplay = eglGetCurrentDisplay();
   eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, &devicePtr);
   eglQueryDeviceAttribEXT(devicePtr, EGL_D3D11_DEVICE_ANGLE, &d3dDevice);
   ```
   传递给 C++ 侧用于创建 D3D11VA 设备上下文

3. **帧队列 + 解码线程**：
   - 解码线程：循环 `FFmpeg_DecodeFrame` → `FFmpeg_TransferFrame` → 入队
   - 渲染线程：`OnOpenGlRender` 时从队列取帧，上传 Y+UV，渲染
   - 丢帧策略：队列满时跳过渲染（参考旧项目 `VideoDecoder.cs`）

4. **替换 ANGLE DLL**：
   - 将 Edge Chromium 的 `libGLESv2.dll` → `av_libglesv2.dll`
   - 将 `libEGL.dll` 和 `d3dcompiler_47.dll` 放入运行时目录
   - 位置：`Avalonia-UI/bin/Debug/net10.0/runtimes/win-x64/native/`

## 关键技术细节

### D3D11VA 初始化（C++ 侧）
```cpp
// 用 ANGLE 的 D3D11 设备创建 hw_device_ctx
AVBufferRef* hw_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
AVD3D11VADeviceContext* d3d = (AVD3D11VADeviceContext*)
    ((AVHWDeviceContext*)hw_ctx->data)->hwctx;
ID3D11Device_AddRef(angle_device);  // 传给 FFmpeg 前增加引用
d3d->device = angle_device;
av_hwdevice_ctx_init(hw_ctx);

// 设置到解码器
codecCtx->hw_device_ctx = av_buffer_ref(hw_ctx);
codecCtx->pix_fmt = AV_PIX_FMT_D3D11;  // 必须设置！
avcodec_open2(codecCtx, codec, NULL);
```

### 帧格式
- 解码后：`frame->format == AV_PIX_FMT_D3D11 (171)`
- `data[0]` 是 D3D11 纹理指针，`data[1]` 编码了子资源索引
- `av_hwframe_transfer_data` 后：CPU 拿到 NV12 (`fmt=23`)，`data[0]`=Y 平面，`data[1]`=UV 平面

### GL 纹理尺寸
- Y 纹理：`width × height`，GL_R8 或 GL_LUMINANCE
- UV 纹理：`width/2 × height/2`，GL_RG8 或 GL_LUMINANCE_ALPHA（交错 UV）

## 不可行路径（备忘）

| 路径 | 根因 |
|------|------|
| EGL NV YUV consumer | BAD_ATTRIBUTE — ANGLE build 限制 |
| D3D11 VideoProcessor | 解码器表面 DECODER 资源状态封锁 GPU 读取 |
| EGLStream + KHR consumer + NV12 | KHR consumer 不支持 NV12 平面格式 |
| ICompositionGpuInterop | `UpdateWithKeyedMutexAsync` → "Unable to consume" |
| Vulkan 后端 | 最终仍走 ANGLE，同样限制 |
