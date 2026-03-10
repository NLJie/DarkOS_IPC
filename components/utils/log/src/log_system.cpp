/**********************************************************************************
*	Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
*	Desc:		日志系统实现
*	FileName:	log_system.cpp
*	Author:		NLJie
*	Date:		2026-03-09
**********************************************************************************/

#include "log_system.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <mutex>
#include <string>

// 日志系统内部状态
struct LogSystemState {
    LogLevel level;
    bool enable_console;
    bool enable_file;
    bool enable_timestamp;
    LogTimestampPrecision timestamp_precision;
    FILE* log_file;
    FILE* error_file;
    std::string log_file_path;
    std::string error_file_path;
    size_t max_file_size;
    int max_backup_files;
    size_t max_error_file_size;
    bool enable_error_file;
    size_t current_file_size;
    size_t current_error_file_size;
    std::mutex mutex;
    bool initialized;

    LogSystemState() 
        : level(LOG_LEVEL_INFO)
        , enable_console(true)
        , enable_file(false)
        , enable_timestamp(true)
        , timestamp_precision(LOG_TIMESTAMP_SECOND)
        , log_file(nullptr)
        , error_file(nullptr)
        , max_file_size(0)
        , max_backup_files(5)
        , max_error_file_size(0)
        , enable_error_file(false)
        , current_file_size(0)
        , current_error_file_size(0)
        , initialized(false)
    {}
};

static LogSystemState g_log_state;

// 日志级别名称
static const char* log_level_names[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR"
};

// 控制台颜色（仅用于终端输出）
#define LOG_COLOR_RESET  "\033[0m"
#define LOG_COLOR_DEBUG  "\033[36m"    // 青色
#define LOG_COLOR_INFO   "\033[32m"    // 绿色
#define LOG_COLOR_WARN   "\033[33m"    // 黄色
#define LOG_COLOR_ERROR  "\033[31m"    // 红色

static const char* log_level_colors[] = {
    LOG_COLOR_DEBUG,
    LOG_COLOR_INFO,
    LOG_COLOR_WARN,
    LOG_COLOR_ERROR,
};

// 获取时间戳字符串
static void get_timestamp(char* buffer, size_t size, LogTimestampPrecision precision)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    
    time_t now = tv.tv_sec;
    struct tm* tm_info = localtime(&now);
    
    if (precision == LOG_TIMESTAMP_MILLISECOND) {
        // 毫秒级时间戳
        int millisec = tv.tv_usec / 1000;
        char time_base[64];
        strftime(time_base, sizeof(time_base), "%Y-%m-%d %H:%M:%S", tm_info);
        snprintf(buffer, size, "%s.%03d", time_base, millisec);
    } else {
        // 秒级时间戳
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    }
}

// 提取文件名（去掉路径）
static const char* extract_filename(const char* path)
{
    const char* filename = strrchr(path, '/');
    if (filename == nullptr) {
        filename = strrchr(path, '\\');
    }
    return filename ? filename + 1 : path;
}

// 获取文件大小
static size_t get_file_size(const char* filepath)
{
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

static std::string get_parent_dir(const std::string& filepath)
{
    size_t pos = filepath.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string();
    }
    if (pos == 0) {
        return "/";
    }
    return filepath.substr(0, pos);
}

static bool ensure_dir_exists(const std::string& dir)
{
    if (dir.empty()) {
        return true;
    }

    size_t start = 0;
    if (dir[0] == '/') {
        start = 1;
    }

    while (start <= dir.size()) {
        size_t slash = dir.find('/', start);
        std::string part = (slash == std::string::npos)
                             ? dir
                             : dir.substr(0, slash);

        if (!part.empty()) {
            if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
                return false;
            }
        }

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    return true;
}

// 日志文件轮转
static void rotate_log_file(const std::string& filepath, int max_backups)
{
    // 删除最旧的备份
    if (max_backups > 0) {
        std::string oldest = filepath + "." + std::to_string(max_backups);
        remove(oldest.c_str());
    }

    // 轮转备份文件
    for (int i = max_backups - 1; i >= 1; i--) {
        std::string old_name = filepath + "." + std::to_string(i);
        std::string new_name = filepath + "." + std::to_string(i + 1);
        rename(old_name.c_str(), new_name.c_str());
    }

    // 重命名当前日志文件
    if (max_backups > 0) {
        std::string backup = filepath + ".1";
        rename(filepath.c_str(), backup.c_str());
    }
}

// 检查并轮转日志文件
static void check_and_rotate(FILE** file_ptr, std::string& filepath, 
                             size_t& current_size, size_t max_size, 
                             int max_backups)
{
    if (max_size == 0 || current_size < max_size) {
        return;
    }

    // 关闭当前文件
    if (*file_ptr != nullptr) {
        fclose(*file_ptr);
        *file_ptr = nullptr;
    }

    // 执行轮转
    rotate_log_file(filepath, max_backups);

    // 重新打开文件
    *file_ptr = fopen(filepath.c_str(), "a");
    current_size = 0;
}

int log_system_init(const LogConfig* config)
{
    std::lock_guard<std::mutex> lock(g_log_state.mutex);

    if (g_log_state.initialized) {
        return 0; // 已经初始化
    }

    if (config != nullptr) {
        g_log_state.level = config->level;
        g_log_state.enable_console = config->enable_console;
        g_log_state.enable_file = config->enable_file;
        g_log_state.enable_timestamp = config->enable_timestamp;
        g_log_state.timestamp_precision = config->timestamp_precision;
        g_log_state.max_file_size = config->max_file_size;
        g_log_state.max_backup_files = config->max_backup_files;
        g_log_state.enable_error_file = config->enable_error_file;
        g_log_state.max_error_file_size = config->max_error_file_size;

        if (g_log_state.level == LOG_LEVEL_NONE) {
            g_log_state.enable_console = false;
            g_log_state.enable_file = false;
            g_log_state.enable_error_file = false;
            fprintf(stderr, "Log system disabled (LOG_LEVEL_NONE)\n");
        }

        // 打开主日志文件
        if (g_log_state.enable_file && config->log_file_path != nullptr) {
            g_log_state.log_file_path = config->log_file_path;
            std::string parent_dir = get_parent_dir(g_log_state.log_file_path);
            if (!ensure_dir_exists(parent_dir)) {
                fprintf(stderr, "Failed to create log dir: %s\n", parent_dir.c_str());
                return -1;
            }
            g_log_state.log_file = fopen(config->log_file_path, "a");
            if (g_log_state.log_file == nullptr) {
                fprintf(stderr, "Failed to open log file: %s\n", config->log_file_path);
                return -1;
            }
            g_log_state.current_file_size = get_file_size(config->log_file_path);
        }

        // 打开错误日志文件
        if (g_log_state.enable_error_file && config->error_file_path != nullptr) {
            g_log_state.error_file_path = config->error_file_path;
            std::string error_dir = get_parent_dir(g_log_state.error_file_path);
            if (!ensure_dir_exists(error_dir)) {
                fprintf(stderr, "Failed to create error log dir: %s\n", error_dir.c_str());
            } else {
                g_log_state.error_file = fopen(config->error_file_path, "a");
            }
            if (g_log_state.error_file == nullptr) {
                fprintf(stderr, "Failed to open error log file: %s\n", config->error_file_path);
                // 继续运行，只是错误日志无法单独输出
            } else {
                g_log_state.current_error_file_size = get_file_size(config->error_file_path);
            }
        }
    }

    g_log_state.initialized = true;
    return 0;
}

void log_system_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_log_state.mutex);

    const bool allow_separator = (g_log_state.level != LOG_LEVEL_NONE);

    // 在关闭前写入分隔符，标记本次会话结束
    if (g_log_state.log_file != nullptr) {
        if (allow_separator) {
            fprintf(g_log_state.log_file, "\n");
            fflush(g_log_state.log_file);
        }
        fclose(g_log_state.log_file);
        g_log_state.log_file = nullptr;
    }

    if (g_log_state.error_file != nullptr) {
        if (allow_separator) {
            fprintf(g_log_state.error_file, "\n");
            fflush(g_log_state.error_file);
        }
        fclose(g_log_state.error_file);
        g_log_state.error_file = nullptr;
    }

    g_log_state.initialized = false;
}

void log_system_set_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_log_state.mutex);
    g_log_state.level = level;
}

LogLevel log_system_get_level(void)
{
    std::lock_guard<std::mutex> lock(g_log_state.mutex);
    return g_log_state.level;
}

void log_system_output(LogLevel level, const char* file, int line, 
                        const char* func, const char* fmt, ...)
{
    // 快速级别过滤（无锁）
    if (level < g_log_state.level) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_log_state.mutex);

    // 双重检查
    if (level < g_log_state.level) {
        return;
    }

    char timestamp[64] = {0};
    if (g_log_state.enable_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp), g_log_state.timestamp_precision);
    }

    const char* filename = extract_filename(file);
    const char* level_name = (level >= LOG_LEVEL_DEBUG && level <= LOG_LEVEL_ERROR) 
                              ? log_level_names[level] 
                              : "UNKN ";

    // 格式化用户消息
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // 输出到控制台
    if (g_log_state.enable_console) {
        FILE* output = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        const char* color = (level >= LOG_LEVEL_DEBUG && level <= LOG_LEVEL_ERROR)
                             ? log_level_colors[level] : LOG_COLOR_RESET;

        if (g_log_state.enable_timestamp) {
            fprintf(output, "%s[%s] [%s] [%s:%d %s]%s %s\n",
                    color, timestamp, level_name, filename, line, func,
                    LOG_COLOR_RESET, message);
        } else {
            fprintf(output, "%s[%s] [%s:%d %s]%s %s\n",
                    color, level_name, filename, line, func,
                    LOG_COLOR_RESET, message);
        }
        fflush(output);
    }

    // 输出到主日志文件
    if (g_log_state.enable_file && g_log_state.log_file != nullptr) {
        char log_line[4096];
        int written = 0;
        if (g_log_state.enable_timestamp) {
            written = snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s:%d %s] %s\n", 
                              timestamp, level_name, filename, line, func, message);
        } else {
            written = snprintf(log_line, sizeof(log_line), "[%s] [%s:%d %s] %s\n", 
                              level_name, filename, line, func, message);
        }
        
        if (written > 0) {
            fprintf(g_log_state.log_file, "%s", log_line);
            fflush(g_log_state.log_file);
            g_log_state.current_file_size += written;
            
            // 检查是否需要轮转
            check_and_rotate(&g_log_state.log_file, g_log_state.log_file_path,
                           g_log_state.current_file_size, g_log_state.max_file_size,
                           g_log_state.max_backup_files);
        }
    }

    // 输出错误/警告到单独文件
    if (g_log_state.enable_error_file && g_log_state.error_file != nullptr &&
        (level == LOG_LEVEL_ERROR || level == LOG_LEVEL_WARN)) {
        char log_line[4096];
        int written = 0;
        if (g_log_state.enable_timestamp) {
            written = snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s:%d %s] %s\n", 
                              timestamp, level_name, filename, line, func, message);
        } else {
            written = snprintf(log_line, sizeof(log_line), "[%s] [%s:%d %s] %s\n", 
                              level_name, filename, line, func, message);
        }
        
        if (written > 0) {
            fprintf(g_log_state.error_file, "%s", log_line);
            fflush(g_log_state.error_file);
            g_log_state.current_error_file_size += written;
            
            // 检查是否需要轮转
            check_and_rotate(&g_log_state.error_file, g_log_state.error_file_path,
                           g_log_state.current_error_file_size, g_log_state.max_error_file_size,
                           g_log_state.max_backup_files);
        }
    }
}
