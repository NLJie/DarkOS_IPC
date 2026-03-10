/**********************************************************************************
*   Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*   Desc:       Windows 平台 HAL 摄像头实现，基于 Media Foundation，支持自动枚举设备
*   FileName:   hal_camera_win.cpp
*   Author:     NLJie
*   Date:       2026-03-10
**********************************************************************************/

// 必须在 windows.h 之前定义，避免引入过多 Win32 宏
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <strsafe.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include "hal_camera.h"
#include "log_system.h"
}

// CMakeLists 中通过 target_link_libraries 链接，此处仅作说明
// mf.lib  mfplat.lib  mfreadwrite.lib  mfuuid.lib  ole32.lib

struct HalCamWinCtx {
    HalCamConfig     cfg;
    IMFSourceReader* pReader;
    BYTE*            frame_buf;
    size_t           frame_buf_size;
    bool             did_coinit;    // 记录是否由本实例初始化了 COM
};

// dev_path 为空或 "0"/"1"/... 时表示设备索引；返回对应索引
static int parse_device_index(const char* dev_path)
{
    if (dev_path == NULL || dev_path[0] == '\0') return 0;
    return atoi(dev_path);
}

// 枚举所有视频采集设备，返回设备数量和设备列表（调用方负责 Release + CoTaskMemFree）
static UINT32 enum_video_devices(IMFActivate*** pppDevices)
{
    *pppDevices = NULL;

    IMFAttributes* pAttr = NULL;
    if (FAILED(MFCreateAttributes(&pAttr, 1))) return 0;

    pAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    UINT32 count = 0;
    HRESULT hr = MFEnumDeviceSources(pAttr, pppDevices, &count);
    pAttr->Release();

    return SUCCEEDED(hr) ? count : 0;
}

extern "C" void* hal_cam_open(const HalCamConfig* cfg)
{
    if (!cfg) return NULL;

    // 初始化 COM（多线程模式）
    bool did_coinit = false;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr == S_OK || hr == S_FALSE) {
        did_coinit = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        // RPC_E_CHANGED_MODE 表示调用方已用其他模式初始化，可以继续使用
        LOG_ERROR("CoInitializeEx failed: 0x%08lx", hr);
        return NULL;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        LOG_ERROR("MFStartup failed: 0x%08lx", hr);
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    // 枚举设备
    IMFActivate** ppDevices = NULL;
    UINT32 count = enum_video_devices(&ppDevices);
    if (count == 0) {
        LOG_ERROR("No video capture devices found");
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    // 打印所有可用设备
    LOG_INFO("Found %u video capture device(s):", count);
    for (UINT32 i = 0; i < count; i++) {
        WCHAR* name = NULL;
        UINT32 len  = 0;
        if (SUCCEEDED(ppDevices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len))) {
            LOG_INFO("  [%u] %ls", i, name);
            CoTaskMemFree(name);
        }
    }

    // 选择设备
    int idx = parse_device_index(cfg->dev_path);
    if (idx < 0 || (UINT32)idx >= count) {
        LOG_ERROR("Device index %d out of range (found %u)", idx, count);
        for (UINT32 i = 0; i < count; i++) ppDevices[i]->Release();
        CoTaskMemFree(ppDevices);
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    // 激活选中的设备
    IMFMediaSource* pSource = NULL;
    hr = ppDevices[idx]->ActivateObject(IID_PPV_ARGS(&pSource));
    for (UINT32 i = 0; i < count; i++) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);

    if (FAILED(hr)) {
        LOG_ERROR("ActivateObject failed: 0x%08lx", hr);
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    // 创建 SourceReader
    IMFSourceReader* pReader = NULL;
    hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
    pSource->Release();
    if (FAILED(hr)) {
        LOG_ERROR("MFCreateSourceReaderFromMediaSource failed: 0x%08lx", hr);
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    // 设置输出格式：优先 YUY2，回退 NV12
    IMFMediaType* pType = NULL;
    if (SUCCEEDED(MFCreateMediaType(&pType))) {
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        MFSetAttributeSize(pType, MF_MT_FRAME_SIZE,
                           (UINT32)cfg->width, (UINT32)cfg->height);

        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (FAILED(pReader->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType))) {
            pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            if (FAILED(pReader->SetCurrentMediaType(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType))) {
                LOG_WARN("Could not set preferred format, using device default");
            }
        }
        pType->Release();
    }

    HalCamWinCtx* ctx = (HalCamWinCtx*)calloc(1, sizeof(HalCamWinCtx));
    if (!ctx) {
        pReader->Release();
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    ctx->cfg            = *cfg;
    ctx->pReader        = pReader;
    ctx->did_coinit     = did_coinit;
    ctx->frame_buf_size = (size_t)(cfg->width * cfg->height * 2);
    ctx->frame_buf      = (BYTE*)malloc(ctx->frame_buf_size);
    if (!ctx->frame_buf) {
        pReader->Release();
        free(ctx);
        MFShutdown();
        if (did_coinit) CoUninitialize();
        return NULL;
    }

    LOG_INFO("Camera opened: device[%d] %dx%d @%.0ffps",
             idx, cfg->width, cfg->height, cfg->fps);
    return ctx;
}

extern "C" int hal_cam_read_frame(void* handle, HalVideoFrame* frame)
{
    if (!handle || !frame) return -1;

    HalCamWinCtx* ctx = (HalCamWinCtx*)handle;

    DWORD      dwFlags    = 0;
    LONGLONG   llTimestamp = 0;
    IMFSample* pSample    = NULL;

    HRESULT hr = ctx->pReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0, NULL, &dwFlags, &llTimestamp, &pSample);

    if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ERROR)) {
        LOG_ERROR("ReadSample failed: 0x%08lx", hr);
        return -1;
    }
    if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
        LOG_WARN("ReadSample: end of stream");
        return -1;
    }
    if (!pSample) return -1;   // 可能是格式变更通知，重试即可

    // 获取连续内存 buffer
    IMFMediaBuffer* pBuffer = NULL;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    pSample->Release();
    if (FAILED(hr)) {
        LOG_ERROR("ConvertToContiguousBuffer failed: 0x%08lx", hr);
        return -1;
    }

    BYTE*  pData   = NULL;
    DWORD  cbLen   = 0;
    hr = pBuffer->Lock(&pData, NULL, &cbLen);
    if (SUCCEEDED(hr)) {
        size_t copy_size = (cbLen < (DWORD)ctx->frame_buf_size)
                           ? cbLen : ctx->frame_buf_size;
        memcpy(ctx->frame_buf, pData, copy_size);
        pBuffer->Unlock();

        frame->vir_addr     = ctx->frame_buf;
        frame->phy_addr     = NULL;
        frame->size         = copy_size;
        frame->width        = ctx->cfg.width;
        frame->height       = ctx->cfg.height;
        frame->stride       = ctx->cfg.width * 2;
        frame->timestamp_us = (int64_t)(llTimestamp / 10); // 100ns → us
    }

    pBuffer->Release();
    return SUCCEEDED(hr) ? 0 : -1;
}

extern "C" void hal_cam_close(void* handle)
{
    if (!handle) return;
    HalCamWinCtx* ctx = (HalCamWinCtx*)handle;
    ctx->pReader->Release();
    free(ctx->frame_buf);
    free(ctx);
    MFShutdown();
    if (ctx->did_coinit) CoUninitialize();
    LOG_INFO("Camera closed");
}

extern "C" int hal_cam_set_mirror(void* handle, bool enable)
{
    (void)handle;
    (void)enable;
    // Media Foundation 不直接提供镜像控制，需通过 IMFVideoProcessorControl
    // 或驱动私有 KSPROPERTY，此处暂不支持
    LOG_WARN("hal_cam_set_mirror: not supported on Windows");
    return -1;
}
