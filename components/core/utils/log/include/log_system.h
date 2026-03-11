/**********************************************************************************
*	Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*	Desc:		日志系统统一接口
*	FileName:	log_system.h
*	Author:		NLJie
*	Date:		2026-03-09
**********************************************************************************/

#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// 日志级别定义
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE  = 4
} LogLevel;

// 时间戳精度定义
typedef enum {
    LOG_TIMESTAMP_SECOND = 0,      // 秒级时间戳 (YYYY-MM-DD HH:MM:SS)
    LOG_TIMESTAMP_MILLISECOND = 1  // 毫秒级时间戳 (YYYY-MM-DD HH:MM:SS.mmm)
} LogTimestampPrecision;

// 日志配置结构
typedef struct {
    LogLevel level;                      // 当前日志级别
    bool enable_console;                 // 是否输出到控制台
    bool enable_file;                    // 是否输出到文件
    bool enable_timestamp;               // 是否显示时间戳
    LogTimestampPrecision timestamp_precision;  // 时间戳精度（秒/毫秒）
    const char* log_file_path;           // 日志文件路径
    size_t max_file_size;                // 日志文件最大大小（字节），0表示不限制
    int max_backup_files;                // 最大备份文件数量
    bool enable_error_file;              // 是否将错误/警告单独输出到文件
    const char* error_file_path;         // 错误/警告日志文件路径
    size_t max_error_file_size;          // 错误日志文件最大大小（字节）
} LogConfig;

/**
 * @brief 初始化日志系统
 * @param config 日志配置，传 NULL 使用默认配置
 * @return 0 on success, -1 on error
 */
int log_system_init(const LogConfig* config);

/**
 * @brief 关闭日志系统
 */
void log_system_shutdown(void);

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void log_system_set_level(LogLevel level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
LogLevel log_system_get_level(void);

/**
 * @brief 日志输出核心函数
 * @param level 日志级别
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 */
void log_system_output(LogLevel level, const char* file, int line, 
                        const char* func, const char* fmt, ...);

// 日志宏定义
#define LOG_DEBUG(fmt, ...) \
    log_system_output(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    log_system_output(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    log_system_output(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    log_system_output(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOG_SYSTEM_H
