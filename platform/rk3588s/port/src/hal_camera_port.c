// platforms/rk3576/port/src/hal_camera_port.c
#include <stdlib.h>

#include "hal_camera.h"
#include "rk_mpi_vi.h"  // 引入 Rockchip 的 VI API
#include "rk_mpi_sys.h" // 引入 MPP Buffer 管理 API
#include "rk_mpi_mb.h"  // MB handle helpers for vir/phys addr and size
#if defined(__has_include)
#if __has_include("rk_comm_vb.h")
#include "rk_comm_vb.h"
#elif __has_include("rk_comm_mb.h")
#include "rk_comm_mb.h"
#endif
#endif
#include "log_system.h"

// 定义内部上下文，存储平台特定的状态
typedef struct {
    RK_U32 dev_id;
    RK_U32 pipe_id;
    RK_U32 channel_id;
    // ... 其他必要状态 ...
} RkCamCtx;

void* hal_cam_open(const HalCamConfig* cfg) {
    RkCamCtx* ctx = malloc(sizeof(RkCamCtx));
    if (!ctx) return NULL;

    RK_S32 s32Ret = RK_FAILURE;

    // --- 核心：使用 Rockchip MPI API 进行初始化 ---
    ctx->dev_id = cfg->dev_path[10] - '0'; // 从 "/dev/videoX" 解析出 X 作为 dev_id
    ctx->pipe_id = ctx->dev_id;
    ctx->channel_id = 0;

    // 1. 配置并使能设备
    VI_DEV_ATTR_S stDevAttr;
    // ... (从默认值或配置中填充 stDevAttr) ...
    s32Ret = RK_MPI_VI_SetDevAttr(ctx->dev_id, &stDevAttr);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetDevAttr failed: 0x%x\n", s32Ret);
        free(ctx);
        return NULL;
    }
    RK_MPI_VI_EnableDev(ctx->dev_id);

    // 2. 配置并使能通道
    VI_CHN_ATTR_S stChnAttr;
    stChnAttr.stSize.u32Width = cfg->width;
    stChnAttr.stSize.u32Height = cfg->height;
    stChnAttr.enPixelFormat = RK_FMT_YUV420SP; // 假设 cfg->pixel_format 会被转换
    // ... 设置其他属性 ...
    s32Ret = RK_MPI_VI_SetChnAttr(ctx->pipe_id, ctx->channel_id, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetChnAttr failed: 0x%x\n", s32Ret);
        // 清理 dev
        RK_MPI_VI_DisableDev(ctx->dev_id);
        free(ctx);
        return NULL;
    }
    RK_MPI_VI_EnableChn(ctx->pipe_id, ctx->channel_id);

    return (void*)ctx;
}

int hal_cam_read_frame(void* handle, HalVideoFrame* frame) {
    if (!handle || !frame) return -1;

    RkCamCtx* ctx = (RkCamCtx*)handle;
    VIDEO_FRAME_INFO_S stViFrame; // Rockchip 特有的帧信息结构

    // --- 核心：使用 Rockchip MPI API 获取帧 ---
    RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(ctx->pipe_id, ctx->channel_id, &stViFrame, 1000); // 1秒超时
    if (s32Ret == RK_SUCCESS) {
        // 将 Rockchip 特有的 VIDEO_FRAME_INFO_S 转换为统一的 HalVideoFrame
        frame->vir_addr = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk); // 从 MPP Block Handle 获取虚拟地址
        frame->phy_addr = (void*)RK_MPI_MB_Handle2PhysAddr(stViFrame.stVFrame.pMbBlk); // 获取物理地址
        frame->size = (size_t)RK_MPI_MB_GetSize(stViFrame.stVFrame.pMbBlk);
        frame->width = stViFrame.stVFrame.u32Width;
        frame->height = stViFrame.stVFrame.u32Height;
        frame->stride = stViFrame.stVFrame.u32VirWidth;
        frame->timestamp_us = stViFrame.stVFrame.u64PTS; // 假设 PTS 代表时间戳

        return 0; // 成功
    }
    return -1; // 失败
}

void hal_cam_close(void* handle) {
    if (handle) {
        RkCamCtx* ctx = (RkCamCtx*)handle;
        // --- 核心：使用 Rockchip MPI API 清理资源 ---
        RK_MPI_VI_DisableChn(ctx->pipe_id, ctx->channel_id);
        RK_MPI_VI_DisableDev(ctx->dev_id);
        free(ctx);
    }
}

int hal_cam_set_mirror(void* handle, bool enable) {
    // 调用你代码中的 pg_VisualSetMirror 或类似功能
    // pg_VisualSetMirror(enable ? PG_TRUE : PG_FALSE);
    // 或者直接使用 RK_MPI 调用 RGA 进行处理
    return 0; // 示例返回
}