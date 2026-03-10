#ifndef COMM_MEM_MONITOR_H
#define COMM_MEM_MONITOR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t vm_size_kb;      // VmSize - 进程占用的虚拟内存总量
    size_t vm_rss_kb;       // VmRSS  - 进程占用的物理内存总量
    size_t vm_peak_kb;      // VmPeak - 进程历史上占用的最大虚拟内存量
    size_t vm_hwm_kb;       // VmHWM  - 进程历史上占用的最大物理内存量（高水位线）
    size_t vm_data_kb;      // VmData - 堆+全局数据段大小（不含共享库）
    size_t vm_stk_kb;       // VmStk  - 主线程栈大小
    unsigned int threads;   // Threads - 当前线程数
} ComMemUsage;

typedef void (*ComMemReportCallback)(const ComMemUsage* usage, void* user_data);

/**
 * @brief 获取当前进程内存使用快照（单位：KB）
 * @return 0 成功，-1 失败
 */
int com_mem_get_usage(ComMemUsage* usage);

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