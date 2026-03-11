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

// 定义摄像头配置
typedef struct {
    int width;
    int height;
    float fps;
    int pixel_format; // 可以定义为枚举，如 HAL_PIX_FMT_YUV420SP
    char dev_path[64]; // 可选，用于兼容 V4L2
} HalCamConfig;

// 定义捕获到的帧数据 (与之前定义的 VideoFrame 类似，可以考虑合并或继承)
typedef struct {
    void* vir_addr;       // 虚拟地址 (可能指向 MPP Buffer)
    void* phy_addr;       // 物理地址 (对于零拷贝很重要)
    size_t size;         
    int64_t timestamp_us; 
    int width;
    int height;
    int stride;
} HalVideoFrame; // 可以重用之前定义的 VideoFrame

/**
 * @brief 初始化摄像头设备 (统一接口)
 * @param cfg 摄像头配置
 * @return 设备句柄，失败返回 NULL
 */
void* hal_cam_open(const HalCamConfig* cfg);

/**
 * @brief 读取一帧数据 (统一接口)
 * @param handle 设备句柄
 * @param frame 输出的帧数据结构
 * @return 0 on success, -1 on error
 */
int hal_cam_read_frame(void* handle, HalVideoFrame* frame);

/**
 * @brief 释放/关闭摄像头设备 (统一接口)
 * @param handle 设备句柄
 */
void hal_cam_close(void* handle);

/**
 * @brief 设置摄像头镜像 (统一接口)
 * @param handle 设备句柄
 * @param enable 是否启用镜像
 * @return 0 on success, -1 on error
 */
int hal_cam_set_mirror(void* handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif // HAL_CAMERA_H