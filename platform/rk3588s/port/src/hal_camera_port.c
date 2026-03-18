/**********************************************************************************
*   Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*   Desc:       RK3588S 平台 HAL 摄像头实现 - V4L2 方式
*               直接从 /dev/video11 (ISP 输出) 读取 NV12 数据
*   FileName:   hal_camera_port.c
*   Author:     NLJie
*   Date:       2026-03-18
**********************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include "hal_camera.h"
#include "log_system.h"

#define V4L2_BUF_COUNT  4
#define CLEAR(x)        memset(&(x), 0, sizeof(x))

typedef struct {
    void    *start;
    size_t  length;
} V4l2Buffer;

typedef struct {
    HalCamConfig    cfg;
    int             fd;
    V4l2Buffer      buffers[V4L2_BUF_COUNT];
    unsigned int    n_buffers;
    void           *frame_buf;
    size_t          frame_buf_size;
} RkCamCtx;

static int xioctl(int fd, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

void* hal_cam_open(const HalCamConfig* cfg)
{
    if (cfg == NULL) {
        LOG_ERROR("hal_cam_open: cfg is NULL");
        return NULL;
    }
    
    RkCamCtx* ctx = calloc(1, sizeof(RkCamCtx));
    if (!ctx) {
        LOG_ERROR("Failed to allocate context");
        return NULL;
    }
    
    ctx->cfg = *cfg;
    
    /* 使用 /dev/video11 (rkisp_mainpath) */
    const char* dev_name = "/dev/video11";
    LOG_INFO("Opening V4L2 device: %s (%dx%d @ %.1ffps)", 
             dev_name, cfg->width, cfg->height, cfg->fps);
    
    ctx->fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (ctx->fd < 0) {
        LOG_ERROR("Cannot open '%s': %s", dev_name, strerror(errno));
        free(ctx);
        return NULL;
    }
    
    /* 设置格式 */
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = cfg->width;
    fmt.fmt.pix_mp.height = cfg->height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.num_planes = 2;
    
    if (xioctl(ctx->fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("VIDIOC_S_FMT failed: %s", strerror(errno));
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    
    LOG_INFO("Format set: %dx%d NV12", 
             fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);
    
    /* 申请缓冲区 */
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = V4L2_BUF_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(ctx->fd, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("VIDIOC_REQBUFS failed: %s", strerror(errno));
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    
    ctx->n_buffers = req.count;
    LOG_INFO("Request %d buffers", ctx->n_buffers);
    
    /* 映射缓冲区 */
    for (unsigned int i = 0; i < ctx->n_buffers; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[2];
        
        CLEAR(buf);
        CLEAR(planes);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = 2;
        
        if (xioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            close(ctx->fd);
            free(ctx);
            return NULL;
        }
        
        ctx->buffers[i].length = buf.m.planes[0].length + buf.m.planes[1].length;
        ctx->buffers[i].start = mmap(NULL, ctx->buffers[i].length,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     ctx->fd, buf.m.planes[0].m.mem_offset);
        
        if (ctx->buffers[i].start == MAP_FAILED) {
            LOG_ERROR("mmap failed: %s", strerror(errno));
            close(ctx->fd);
            free(ctx);
            return NULL;
        }
        
        LOG_INFO("Buffer %d: %p, size=%zu", i, ctx->buffers[i].start, ctx->buffers[i].length);
    }
    
    /* 入队缓冲区 */
    for (unsigned int i = 0; i < ctx->n_buffers; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[2];
        
        CLEAR(buf);
        CLEAR(planes);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = 2;
        
        if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("VIDIOC_QBUF failed: %s", strerror(errno));
            close(ctx->fd);
            free(ctx);
            return NULL;
        }
    }
    
    /* 开始采集 */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(ctx->fd, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("VIDIOC_STREAMON failed: %s", strerror(errno));
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    
    /* 分配帧缓冲区 */
    ctx->frame_buf_size = cfg->width * cfg->height * 3 / 2;  /* NV12 */
    ctx->frame_buf = malloc(ctx->frame_buf_size);
    if (!ctx->frame_buf) {
        LOG_ERROR("Failed to allocate frame buffer");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    
    LOG_INFO("Camera opened successfully");
    return ctx;
}

int hal_cam_read_frame(void* handle, HalVideoFrame* frame)
{
    if (!handle || !frame) return -1;
    
    RkCamCtx* ctx = handle;
    struct v4l2_buffer buf;
    struct v4l2_plane planes[2];
    fd_set fds;
    struct timeval tv;
    int r;
    
    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    r = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
    if (r < 0) {
        LOG_ERROR("select error: %s", strerror(errno));
        return -1;
    }
    if (r == 0) {
        LOG_WARN("select timeout");
        return -1;
    }
    
    CLEAR(buf);
    CLEAR(planes);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = 2;
    
    if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            LOG_ERROR("VIDIOC_DQBUF failed: %s", strerror(errno));
        }
        return -1;
    }
    
    /* 填充 HalVideoFrame */
    memset(frame, 0, sizeof(*frame));
    frame->vir_addr = ctx->buffers[buf.index].start;
    frame->phy_addr = NULL;  /* V4L2 不直接提供物理地址 */
    frame->size = ctx->buffers[buf.index].length;
    frame->width = ctx->cfg.width;
    frame->height = ctx->cfg.height;
    frame->stride = ctx->cfg.width;  /* NV12 Y 平面步长 */
    frame->timestamp_us = buf.timestamp.tv_sec * 1000000LL + buf.timestamp.tv_usec;
    
    /* 重新入队 */
    if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) < 0) {
        LOG_ERROR("VIDIOC_QBUF failed: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

void hal_cam_close(void* handle)
{
    if (!handle) return;
    
    RkCamCtx* ctx = handle;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    xioctl(ctx->fd, VIDIOC_STREAMOFF, &type);
    
    for (unsigned int i = 0; i < ctx->n_buffers; ++i) {
        munmap(ctx->buffers[i].start, ctx->buffers[i].length);
    }
    
    free(ctx->frame_buf);
    close(ctx->fd);
    free(ctx);
    
    LOG_INFO("Camera closed");
}

int hal_cam_set_mirror(void* handle, bool enable)
{
    (void)handle;
    (void)enable;
    LOG_WARN("Mirror not supported in V4L2 mode");
    return -1;
}
