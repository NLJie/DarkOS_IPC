/**********************************************************************************
*	Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*	Desc:		摄像头硬件操作头文件
*	FileName:	hal_camera.h
*	Author:		NLJie
*	Date:		09-03-09
**********************************************************************************/

#ifndef HAL_CAMERA_H
#define HAL_CAMERA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 摄像头接口类型
 * ============================================================ */
typedef enum {
    HAL_CAM_IF_USB     = 0,  /**< USB UVC, 走 V4L2 (/dev/videoX)        */
    HAL_CAM_IF_MIPI    = 1,  /**< MIPI CSI-2, 直连 SoC ISP              */
    HAL_CAM_IF_DVP     = 2,  /**< 并行 DVP                               */
    HAL_CAM_IF_RTSP    = 3,  /**< 网络 IP 摄像头 / RTSP 流               */
    HAL_CAM_IF_VIRTUAL = 4,  /**< 虚拟摄像头 (测试/文件回放)             */
} HalCamInterface;

/* ============================================================
 * 摄像头配置
 * 通用字段 + 按接口类型选用的专属字段 (union)
 * ============================================================ */
typedef struct {
    HalCamInterface interface;   /**< 接口类型, 决定下方 union 哪个成员有效 */

    /* --- 通用参数 --- */
    int   width;
    int   height;
    float fps;
    int   pixel_format;          /**< 0 = 由 HAL 自动选择                  */

    /* --- 接口专属参数 --- */
    union {
        /* HAL_CAM_IF_USB / HAL_CAM_IF_DVP */
        struct {
            char dev_path[64];   /**< 设备节点, 如 "/dev/video0"; 空=自动扫描 */
        } usb;

        /* HAL_CAM_IF_MIPI */
        struct {
            char dev_path[64];   /**< 部分平台 MIPI 也映射为 /dev/videoX    */
            int  lanes;          /**< MIPI lane 数: 1 / 2 / 4               */
            char sensor[32];     /**< sensor 型号, 如 "imx415" / "os08a10"  */
        } mipi;

        /* HAL_CAM_IF_RTSP */
        struct {
            char url[256];       /**< "rtsp://ip/stream"                     */
            char username[64];
            char password[64];
        } rtsp;

        /* HAL_CAM_IF_VIRTUAL */
        struct {
            char file_path[256]; /**< 视频文件路径; 空=生成纯色合成帧        */
        } virt;
    };
} HalCamConfig;

/* ============================================================
 * 帧数据
 * ============================================================ */
typedef struct {
    void    *vir_addr;       /**< 虚拟地址                                  */
    void    *phy_addr;       /**< 物理地址 (零拷贝场景)                      */
    size_t   size;
    int64_t  timestamp_us;
    int      width;
    int      height;
    int      stride;
} HalVideoFrame;

/* ============================================================
 * HAL 接口
 * ============================================================ */

/** @brief 打开摄像头, 返回设备句柄; 失败返回 NULL */
void *hal_cam_open       (const HalCamConfig *cfg);

/** @brief 读一帧; 0 成功, -1 失败 */
int   hal_cam_read_frame (void *handle, HalVideoFrame *frame);

/** @brief 关闭摄像头, 释放资源 */
void  hal_cam_close      (void *handle);

/** @brief 设置水平镜像; 0 成功, -1 不支持 */
int   hal_cam_set_mirror (void *handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* HAL_CAMERA_H */
