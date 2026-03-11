/**********************************************************************************
*	Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*	Desc:		占位文件，可以保证接口不依赖平台编译成功，也可以演示接口调用
*	FileName:	hal_camera_stub.cpp
*	Author:		NLJie
*	Date:		09-03-09
**********************************************************************************/

#include <cstring>

#include "hal_camera.h"

struct HalCamStubCtx {
    HalCamConfig cfg;
    int opened;
};

void* hal_cam_open(const HalCamConfig* cfg)
{
    if (cfg == nullptr) {
        return nullptr;
    }

    HalCamStubCtx* ctx = new HalCamStubCtx();
    std::memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = *cfg;
    ctx->opened = 1;
    return static_cast<void*>(ctx);
}

int hal_cam_read_frame(void* handle, HalVideoFrame* frame)
{
    HalCamStubCtx* ctx = static_cast<HalCamStubCtx*>(handle);
    if (ctx == nullptr || frame == nullptr || ctx->opened == 0) {
        return -1;
    }

    std::memset(frame, 0, sizeof(*frame));
    frame->width = ctx->cfg.width;
    frame->height = ctx->cfg.height;
    return -1;
}

void hal_cam_close(void* handle)
{
    HalCamStubCtx* ctx = static_cast<HalCamStubCtx*>(handle);
    if (ctx != nullptr) {
        ctx->opened = 0;
        delete ctx;
    }
}

int hal_cam_set_mirror(void* handle, bool enable)
{
    (void)enable;
    return (handle != nullptr) ? 0 : -1;
}
