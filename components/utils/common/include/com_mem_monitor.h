#ifndef COMM_MEM_MONITOR_H
#define COMM_MEM_MONITOR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t vm_size_kb;  // VmSize
    size_t vm_rss_kb;   // VmRSS
    size_t vm_peak_kb;  // VmPeak
    size_t vm_hwm_kb;   // VmHWM
} CommonMemoryUsage;

typedef void (*ComMemReportCallback)(const CommonMemoryUsage* usage, void* user_data);

/**
 * @brief 获取当前进程内存使用快照（单位：KB）
 * @return 0 成功，-1 失败
 */
int com_mem_get_usage(CommonMemoryUsage* usage);

/**
 * @brief 启动周期性内存监控
 * @param interval_ms 采样周期，建议 >= 100
 * @param cb 回调函数，不能为空
 * @param user_data 回调透传参数
 * @return 0 成功，-1 失败
 */
int com_mem_monitor_start(unsigned int interval_ms,
                                ComMemReportCallback cb,
                                void* user_data);

/**
 * @brief 停止周期性内存监控
 * @return 0 成功，-1 失败
 */
int com_mem_monitor_stop(void);

/**
 * @brief 查询监控是否运行中
 */
bool com_mem_monitor_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // COMM_MEM_MONITOR_H