/**********************************************************************************
*   Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*   Desc:       x86 平台 HAL 摄像头实现，基于 OpenCV
*               支持 USB / MIPI / RTSP / 虚拟(文件回放/合成帧)
*   FileName:   hal_camera_opencv.cpp
*   Author:     NLJie
*   Date:       2026-03-16
**********************************************************************************/

#include "hal_camera.h"
#include "log_system.h"

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <string>

/* ============================================================
 * 内部常量
 * ============================================================ */
#define AUTO_SCAN_MAX_IDX   9        /* 自动扫描 /dev/video0 ~ /dev/video9 */

/* ============================================================
 * 内部上下文
 * ============================================================ */
struct HalCamCvCtx {
    cv::VideoCapture cap;
    HalCamConfig     cfg;
    cv::Mat          frame_bgr;   /* 最近一帧 BGR 数据，生命周期与 ctx 绑定 */
    bool             mirror;      /* 水平镜像标志 */
    bool             synthetic;   /* 合成帧模式（虚拟摄像头无文件时） */
    std::string      open_uri;    /* 设备路径/URL，用于日志和 RTSP 重连 */
    uint8_t          synth_color; /* 合成帧色调偏移，用于制造动态效果 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/* 获取当前时间戳（微秒） */
static int64_t now_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* 将 RTSP 鉴权信息嵌入 URL
 * rtsp://host/path + user/pass → rtsp://user:pass@host/path */
static std::string build_rtsp_url(const HalCamConfig* cfg)
{
    std::string url(cfg->rtsp.url);
    if (cfg->rtsp.username[0] == '\0') return url;

    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        std::string auth = std::string(cfg->rtsp.username)
                         + ":" + std::string(cfg->rtsp.password) + "@";
        url.insert(pos + 3, auth);
    }
    return url;
}

/* 尝试用 V4L2 打开指定索引，返回是否成功 */
static bool try_open_v4l2(cv::VideoCapture& cap, int idx, std::string& out_uri)
{
    if (!cap.open(idx, cv::CAP_V4L2)) return false;
    out_uri = "/dev/video" + std::to_string(idx);
    return true;
}

/* ============================================================
 * HAL 接口实现
 * ============================================================ */

void* hal_cam_open(const HalCamConfig* cfg)
{
    if (!cfg) return nullptr;

    HalCamCvCtx* ctx = new HalCamCvCtx();
    ctx->mirror     = false;
    ctx->synthetic  = false;
    ctx->synth_color = 0;
    ctx->cfg        = *cfg;

    switch (cfg->interface) {

    /* ---- USB / MIPI / DVP: 统一走 V4L2 ---- */
    case HAL_CAM_IF_USB:
    case HAL_CAM_IF_MIPI:
    case HAL_CAM_IF_DVP: {
        bool opened = false;
        if (cfg->usb.dev_path[0] != '\0') {
            /* 用指定的设备节点 */
            opened = ctx->cap.open(cfg->usb.dev_path, cv::CAP_V4L2);
            if (opened) {
                ctx->open_uri = cfg->usb.dev_path;
            } else {
                LOG_ERROR("无法打开指定设备: %s", cfg->usb.dev_path);
            }
        } else {
            /* 自动扫描 /dev/video0 ~ /dev/video{MAX} */
            LOG_INFO("未指定设备节点，自动扫描 /dev/video0 ~ /dev/video%d ...",
                     AUTO_SCAN_MAX_IDX);
            for (int i = 0; i <= AUTO_SCAN_MAX_IDX; i++) {
                if (try_open_v4l2(ctx->cap, i, ctx->open_uri)) {
                    LOG_INFO("找到摄像头: /dev/video%d", i);
                    opened = true;
                    break;
                }
            }
            if (!opened)
                LOG_ERROR("未找到可用摄像头 (/dev/video0 ~ /dev/video%d)",
                          AUTO_SCAN_MAX_IDX);
        }
        if (!opened) { delete ctx; return nullptr; }
        break;
    }

    /* ---- RTSP: 通过 FFmpeg 后端拉流 ---- */
    case HAL_CAM_IF_RTSP: {
        /* 强制 TCP 传输，避免 UDP 丢包导致花屏；超时 5s */
        setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS",
               "rtsp_transport;tcp|timeout;5000000", 0 /* 不覆盖已有设置 */);

        ctx->open_uri = build_rtsp_url(cfg);
        if (!ctx->cap.open(ctx->open_uri, cv::CAP_FFMPEG)) {
            LOG_ERROR("RTSP 连接失败: %s", cfg->rtsp.url);
            delete ctx;
            return nullptr;
        }
        break;
    }

    /* ---- VIRTUAL: 文件回放 或 合成帧 ---- */
    case HAL_CAM_IF_VIRTUAL: {
        if (cfg->virt.file_path[0] != '\0') {
            ctx->open_uri = cfg->virt.file_path;
            if (!ctx->cap.open(cfg->virt.file_path, cv::CAP_FFMPEG)) {
                LOG_ERROR("无法打开视频文件: %s", cfg->virt.file_path);
                delete ctx;
                return nullptr;
            }
            LOG_INFO("虚拟摄像头: 文件回放 %s", cfg->virt.file_path);
        } else {
            /* 无文件：生成纯色合成帧（用于无摄像头的测试场景） */
            ctx->synthetic = true;
            ctx->open_uri  = "<synthetic>";
            ctx->cfg.width  = (cfg->width  > 0) ? cfg->width  : 640;
            ctx->cfg.height = (cfg->height > 0) ? cfg->height : 480;
            ctx->cfg.fps    = (cfg->fps    > 0) ? cfg->fps    : 25.0f;
            LOG_INFO("虚拟摄像头: 合成帧模式 %dx%d @%.0ffps",
                     ctx->cfg.width, ctx->cfg.height, ctx->cfg.fps);
            return ctx;   /* 合成模式不需要 cap，直接返回 */
        }
        break;
    }

    default:
        LOG_ERROR("不支持的接口类型: %d", cfg->interface);
        delete ctx;
        return nullptr;
    }

    /* ---- 设置分辨率/帧率（RTSP/文件不设置，由流本身决定） ---- */
    if (cfg->interface != HAL_CAM_IF_RTSP &&
        cfg->interface != HAL_CAM_IF_VIRTUAL) {
        if (cfg->width  > 0) ctx->cap.set(cv::CAP_PROP_FRAME_WIDTH,  cfg->width);
        if (cfg->height > 0) ctx->cap.set(cv::CAP_PROP_FRAME_HEIGHT, cfg->height);
        if (cfg->fps    > 0) ctx->cap.set(cv::CAP_PROP_FPS,          cfg->fps);
    }

    /* ---- 读回驱动实际生效的参数 ---- */
    ctx->cfg.width  = (int)ctx->cap.get(cv::CAP_PROP_FRAME_WIDTH);
    ctx->cfg.height = (int)ctx->cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    ctx->cfg.fps    = (float)ctx->cap.get(cv::CAP_PROP_FPS);

    LOG_INFO("摄像头已打开: %s  %dx%d @%.0ffps",
             ctx->open_uri.c_str(), ctx->cfg.width, ctx->cfg.height, ctx->cfg.fps);
    return ctx;
}

/* ------------------------------------------------------------ */

int hal_cam_read_frame(void* handle, HalVideoFrame* frame)
{
    if (!handle || !frame) return -1;
    HalCamCvCtx* ctx = static_cast<HalCamCvCtx*>(handle);

    /* ---- 合成帧：生成动态彩色图像 ---- */
    if (ctx->synthetic) {
        ctx->frame_bgr.create(ctx->cfg.height, ctx->cfg.width, CV_8UC3);
        ctx->frame_bgr.setTo(cv::Scalar(
            ctx->synth_color,
            128,
            255 - ctx->synth_color));
        ctx->synth_color += 2;   /* 色调缓慢变化，方便肉眼判断帧是否在更新 */
        goto fill_frame;
    }

    /* ---- 抓取一帧 ---- */
    ctx->cap >> ctx->frame_bgr;

    /* ---- 文件回放：到达结尾自动循环 ---- */
    if (ctx->frame_bgr.empty() && ctx->cfg.interface == HAL_CAM_IF_VIRTUAL) {
        LOG_DEBUG("视频文件播放完毕，循环重播");
        ctx->cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        ctx->cap >> ctx->frame_bgr;
    }

    /* ---- RTSP：掉线后自动重连一次 ---- */
    if (ctx->frame_bgr.empty() && ctx->cfg.interface == HAL_CAM_IF_RTSP) {
        LOG_WARN("RTSP 收到空帧，尝试重连: %s", ctx->open_uri.c_str());
        ctx->cap.release();
        if (ctx->cap.open(ctx->open_uri, cv::CAP_FFMPEG)) {
            LOG_INFO("RTSP 重连成功: %s", ctx->open_uri.c_str());
            ctx->cap >> ctx->frame_bgr;
        } else {
            LOG_ERROR("RTSP 重连失败: %s", ctx->open_uri.c_str());
        }
    }

    if (ctx->frame_bgr.empty()) {
        LOG_WARN("读帧失败: %s", ctx->open_uri.c_str());
        return -1;
    }

fill_frame:
    /* ---- 水平镜像 ---- */
    if (ctx->mirror)
        cv::flip(ctx->frame_bgr, ctx->frame_bgr, 1 /* flipCode=1: 水平翻转 */);

    /* ---- 填充 HalVideoFrame ---- */
    frame->vir_addr     = ctx->frame_bgr.data;
    frame->phy_addr     = nullptr;             /* x86 不提供物理地址 */
    frame->size         = ctx->frame_bgr.total() * ctx->frame_bgr.elemSize();
    frame->width        = ctx->frame_bgr.cols;
    frame->height       = ctx->frame_bgr.rows;
    frame->stride       = static_cast<int>(ctx->frame_bgr.step[0]);
    frame->timestamp_us = now_us();

    return 0;
}

/* ------------------------------------------------------------ */

void hal_cam_close(void* handle)
{
    if (!handle) return;
    HalCamCvCtx* ctx = static_cast<HalCamCvCtx*>(handle);
    if (ctx->cap.isOpened()) ctx->cap.release();
    LOG_INFO("摄像头已关闭: %s", ctx->open_uri.c_str());
    delete ctx;
}

/* ------------------------------------------------------------ */

int hal_cam_set_mirror(void* handle, bool enable)
{
    if (!handle) return -1;
    HalCamCvCtx* ctx = static_cast<HalCamCvCtx*>(handle);
    ctx->mirror = enable;
    LOG_INFO("水平镜像: %s", enable ? "开启" : "关闭");
    return 0;
}
