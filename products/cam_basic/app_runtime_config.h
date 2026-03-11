/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     应用运行时配置头文件
 * FileName: app_runtime_config.h
 * Author:   NLJie
 * Date:     2026-03-09
 **********************************************************************************/

#ifndef APP_RUNTIME_CONFIG_H
#define APP_RUNTIME_CONFIG_H

#include "hal_camera.h"
#include "log_system.h"

/* ============================================================
 * 兜底默认值 (仅当 JSON 缺少对应字段时生效)
 * ============================================================ */
#define PG_FALLBACK_LOG_DIR          "./log"
#define PG_FALLBACK_LOG_SIZE         (1 * 1024 * 1024)   /* 1 MB  */
#define PG_FALLBACK_LOG_BACKUPS      3
#define PG_FALLBACK_ERROR_LOG_SIZE   (512 * 1024)         /* 512 KB */

/* 路径字符串最大长度 */
#define PG_PATH_MAX  256

/* ============================================================
 * 运行时配置结构体
 *
 * 所有路径以 char 数组形式存储在此结构体内, log_cfg 中的
 * const char* 指针指向这些数组, 确保生命周期一致.
 * ============================================================ */
struct AppRuntimeConfig {
    /* --- 日志 --- */
    LogConfig log_cfg;                   /* log_file_path/error_file_path 指向下面的数组 */
    char log_file_path[PG_PATH_MAX];     /* 主日志文件路径         */
    char error_file_path[PG_PATH_MAX];   /* 错误日志文件路径       */

    /* --- 摄像头 --- */
    HalCamConfig cam_cfg;

    /* --- 路径 --- */
    char data_dir[PG_PATH_MAX];          /* 持久化数据目录         */
    char temp_dir[PG_PATH_MAX];          /* 临时文件目录           */
    char media_dir[PG_PATH_MAX];         /* 媒体输出目录           */

    /* --- AI --- */
    char  ai_model_path[PG_PATH_MAX];    /* 推理模型文件路径       */
    float ai_threshold;                  /* 检测置信度阈值         */
};

/**
 * @brief 初始化运行时配置
 *
 * 先用兜底默认值填充, 再用 JSON 文件逐字段覆盖.
 * JSON 中缺失的字段保留默认值.
 * json_path 为 NULL 时纯用默认值.
 *
 * @param json_path  设备配置 JSON 文件路径, 可为 NULL
 * @return 0 成功; -1 文件读取或解析失败 (降级为默认值继续运行)
 */
int app_runtime_config_load(const char *json_path);

/** @brief 获取已加载的配置 (只读). 未调用 load 时返回默认值. */
const AppRuntimeConfig &app_runtime_config_get();

#endif /* APP_RUNTIME_CONFIG_H */
