/**********************************************************************************
*	Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*	Desc:		app_runtime_config.h
*	FileName:	config.h
*	Author:		NLJie
*	Date:		2026-03-09
**********************************************************************************/

#ifndef APP_RUNTIME_CONFIG_H
#define APP_RUNTIME_CONFIG_H

#include "hal_camera.h"
#include "log_system.h"

// ==================== 日志文件路径配置 ====================
#define LOG_DIR                 "."     // 日志根目录
#define LOG_FILE_PATH           LOG_DIR "/app.log"      // 主日志文件
#define ERROR_LOG_FILE_PATH     LOG_DIR "/app_err.log"  // 错误日志文件

// 日志文件大小限制
#define LOG_MAX_FILE_SIZE       (1024 * 1024)           // 1MB
#define LOG_MAX_BACKUP_FILES    3                       // 保留3个备份
#define ERROR_LOG_MAX_FILE_SIZE (512 * 1024)            // 512KB

// ==================== 数据文件路径配置 ====================
#define DATA_DIR                "/var/lib/pg_camera"    // 数据根目录
#define CONFIG_FILE_PATH        DATA_DIR "/config.json" // 配置文件
#define DATABASE_FILE_PATH      DATA_DIR "/data.db"     // 数据库文件

// ==================== 临时文件路径配置 ====================
#define TEMP_DIR                "/tmp/pg_camera"        // 临时文件目录
#define TEMP_IMAGE_PATH         TEMP_DIR "/image.jpg"  // 临时图片
#define TEMP_VIDEO_PATH         TEMP_DIR "/video.mp4"  // 临时视频

// ==================== 媒体文件路径配置 ====================
#define MEDIA_DIR               "/opt/pg_camera/media"  // 媒体根目录
#define SNAPSHOT_DIR            MEDIA_DIR "/snapshots"  // 快照目录
#define RECORDING_DIR           MEDIA_DIR "/recordings" // 录像目录

// ==================== 模型文件路径配置 ====================
#define MODEL_DIR               "/opt/pg_camera/models" // AI模型目录
#define RKNN_MODEL_PATH         MODEL_DIR "/yolov5.rknn" // RKNN模型文件

// ==================== 其他配置 ====================
#define PID_FILE_PATH           "/var/run/pg_camera.pid" // PID文件

struct AppRuntimeConfig {
    LogConfig log_cfg;
    HalCamConfig cam_cfg;
};

const AppRuntimeConfig& app_runtime_config();

#endif // APP_RUNTIME_CONFIG_H
