/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     设备配置模块 — 基于 pg_config 的硬件 fill 函数
 *
 *   职责: 只负责"把设备 JSON 里的硬件参数填充到具体结构体".
 *         通用 JSON 读取由 pg_config 模块负责.
 *
 *   典型用法:
 *     pg_cfg_load("configs/device_rk3576_cam_v1.json");  // pg_config 模块
 *     pg_dev_cfg_fill_log_config(&log_cfg);               // 本模块
 *     pg_dev_cfg_fill_cam_config(0, &cam_cfg);
 *
 * FileName: pg_device_config.h
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#ifndef PG_DEVICE_CONFIG_H
#define PG_DEVICE_CONFIG_H

#include "hal_camera.h"
#include "log_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从 "log.*" 填充 LogConfig
 * @note  log_file_path / error_file_path 指向模块内部静态 buffer,
 *        生命周期与进程相同.
 * @return 0 成功
 */
int pg_dev_cfg_fill_log_config(LogConfig *out);

/**
 * @brief 从 "cameras[idx].*" 填充 HalCamConfig
 * @param idx  cameras 数组下标
 * @return 0 成功; -1 越界
 */
int pg_dev_cfg_fill_cam_config(int idx, HalCamConfig *out);

#ifdef __cplusplus
}
#endif

#endif /* PG_DEVICE_CONFIG_H */
