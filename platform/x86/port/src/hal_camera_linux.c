/**********************************************************************************
*   Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*   Desc:       Linux 平台 HAL 摄像头实现，基于 V4L2，支持自动搜索可用设备
*   FileName:   hal_camera_linux.c
*   Author:     NLJie
*   Date:       2026-03-10
**********************************************************************************/
// 确保开启 POSIX 特性
#define _POSIX_C_SOURCE 200809L 
// 如果是较新的 glibc 且遇到 2038 问题，可能需要这个：
// #define _TIME_BITS 64 

#include "hal_camera.h"
#include "log_system.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/videodev2.h>

#define V4L2_BUF_COUNT      4
#define READ_TIMEOUT_SEC    2
#define VIDEO_DEV_MAX_IDX   20   // 扫描 /dev/video0 ~ /dev/video20

typedef struct {
    void*  start;
    size_t length;
} V4l2Buffer;

typedef struct {
    int          fd;
    HalCamConfig cfg;
    V4l2Buffer   buffers[V4L2_BUF_COUNT];
    unsigned int n_buffers;
    void*        frame_buf;
    size_t       frame_buf_size;
} HalCamLinuxCtx;

static int xioctl(int fd, unsigned long request, void* arg)
{
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

// 尝试打开并验证一个节点是否为可用的视频采集设备，成功返回 fd，失败返回 -1
static int try_open_video_device(const char* path)
{
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) return -1;

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 ||
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        close(fd);
        return -1;
    }
    return fd;
}

// 确定要使用的摄像头并返回已打开的 fd
// dev_path 非空时直接使用；否则依次尝试 /dev/video0 ~ /dev/video9
static int resolve_camera_fd(const char* dev_path, char* out_path, size_t out_size)
{
    if (dev_path != NULL && dev_path[0] != '\0') {
        int fd = try_open_video_device(dev_path);
        if (fd >= 0) {
            strncpy(out_path, dev_path, out_size - 1);
            out_path[out_size - 1] = '\0';
            return fd;
        }
        LOG_ERROR("Specified device %s not available", dev_path);
        return -1;
    }

    LOG_INFO("No device specified, scanning /dev/video0 ~ /dev/video%d ...", VIDEO_DEV_MAX_IDX);
    for (int i = 0; i <= VIDEO_DEV_MAX_IDX; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/video%d", i);
        int fd = try_open_video_device(path);
        if (fd >= 0) {
            LOG_INFO("Found camera: %s", path);
            strncpy(out_path, path, out_size - 1);
            out_path[out_size - 1] = '\0';
            return fd;
        }
    }

    LOG_ERROR("No available camera found in /dev/video0 ~ /dev/video%d", VIDEO_DEV_MAX_IDX);
    return -1;
}

static void cleanup_ctx(HalCamLinuxCtx* ctx, unsigned int mapped_count)
{
    for (unsigned int i = 0; i < mapped_count; i++) {
        munmap(ctx->buffers[i].start, ctx->buffers[i].length);
    }
    free(ctx->frame_buf);
    close(ctx->fd);
    free(ctx);
}

void* hal_cam_open(const HalCamConfig* cfg)
{
    if (cfg == NULL) return NULL;

    char dev[64] = {0};
    int fd = resolve_camera_fd(cfg->dev_path, dev, sizeof(dev));
    if (fd < 0) return NULL;

    // 设置采集格式（YUYV 兼容性最广）
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = (unsigned int)cfg->width;
    fmt.fmt.pix.height      = (unsigned int)cfg->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("VIDIOC_S_FMT failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    LOG_INFO("Format: %dx%d YUYV (driver adjusted: %dx%d)",
             cfg->width, cfg->height, fmt.fmt.pix.width, fmt.fmt.pix.height);

    // 设置帧率（不支持时忽略）
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = (unsigned int)cfg->fps;
    if (xioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
        LOG_WARN("Frame rate setting not supported, ignored");
    }

    // 申请 mmap 缓冲区
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = V4L2_BUF_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        LOG_ERROR("VIDIOC_REQBUFS failed (got %u buffers)", req.count);
        close(fd);
        return NULL;
    }

    HalCamLinuxCtx* ctx = (HalCamLinuxCtx*)calloc(1, sizeof(HalCamLinuxCtx));
    if (!ctx) { close(fd); return NULL; }

    ctx->fd        = fd;
    ctx->cfg       = *cfg;
    ctx->n_buffers = req.count;
    strncpy(ctx->cfg.dev_path, dev, sizeof(ctx->cfg.dev_path) - 1);
    ctx->cfg.width  = (int)fmt.fmt.pix.width;
    ctx->cfg.height = (int)fmt.fmt.pix.height;

    ctx->frame_buf_size = (size_t)(ctx->cfg.width * ctx->cfg.height * 2);
    ctx->frame_buf = malloc(ctx->frame_buf_size);
    if (!ctx->frame_buf) { free(ctx); close(fd); return NULL; }

    // mmap 各缓冲区
    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QUERYBUF failed for buffer %u", i);
            cleanup_ctx(ctx, i);
            return NULL;
        }
        ctx->buffers[i].length = buf.length;
        ctx->buffers[i].start  = mmap(NULL, buf.length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, buf.m.offset);
        if (ctx->buffers[i].start == MAP_FAILED) {
            LOG_ERROR("mmap failed for buffer %u: %s", i, strerror(errno));
            cleanup_ctx(ctx, i);
            return NULL;
        }
    }

    // 所有缓冲区入队
    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QBUF failed for buffer %u", i);
            cleanup_ctx(ctx, ctx->n_buffers);
            return NULL;
        }
    }

    // 开始采集
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("VIDIOC_STREAMON failed: %s", strerror(errno));
        cleanup_ctx(ctx, ctx->n_buffers);
        return NULL;
    }

    LOG_INFO("Camera opened: %s %dx%d @%.0ffps", dev, ctx->cfg.width, ctx->cfg.height, cfg->fps);
    return ctx;
}

int hal_cam_read_frame(void* handle, HalVideoFrame* frame)
{
    if (!handle || !frame) return -1;

    HalCamLinuxCtx* ctx = (HalCamLinuxCtx*)handle;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);
    struct timeval tv;
    tv.tv_sec  = READ_TIMEOUT_SEC;
    tv.tv_usec = 0;

    int ret = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
    if (ret == 0) { LOG_WARN("Read frame timeout"); return -1; }
    if (ret < 0)  { LOG_ERROR("select error: %s", strerror(errno)); return -1; }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf) < 0) {
        LOG_ERROR("VIDIOC_DQBUF failed: %s", strerror(errno));
        return -1;
    }

    size_t copy_size = (buf.bytesused < ctx->frame_buf_size)
                       ? buf.bytesused : ctx->frame_buf_size;
    memcpy(ctx->frame_buf, ctx->buffers[buf.index].start, copy_size);

    if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) < 0) {
        LOG_ERROR("VIDIOC_QBUF failed after read: %s", strerror(errno));
    }

    frame->vir_addr     = ctx->frame_buf;
    frame->phy_addr     = NULL;
    frame->size         = copy_size;
    frame->width        = ctx->cfg.width;
    frame->height       = ctx->cfg.height;
    frame->stride       = ctx->cfg.width * 2;
    frame->timestamp_us = (int64_t)buf.timestamp.tv_sec * 1000000LL
                          + (int64_t)buf.timestamp.tv_usec;
    return 0;
}

void hal_cam_close(void* handle)
{
    if (!handle) return;
    HalCamLinuxCtx* ctx = (HalCamLinuxCtx*)handle;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(ctx->fd, VIDIOC_STREAMOFF, &type);
    cleanup_ctx(ctx, ctx->n_buffers);
    LOG_INFO("Camera closed");
}

int hal_cam_set_mirror(void* handle, bool enable)
{
    if (!handle) return -1;
    HalCamLinuxCtx* ctx = (HalCamLinuxCtx*)handle;
    struct v4l2_control ctrl;
    ctrl.id    = V4L2_CID_HFLIP;
    ctrl.value = enable ? 1 : 0;
    if (xioctl(ctx->fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        LOG_WARN("hal_cam_set_mirror: V4L2_CID_HFLIP not supported");
        return -1;
    }
    return 0;
}
