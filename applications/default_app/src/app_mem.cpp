#include "app_mem.h"

#include <cstdio>

#include "log_system.h"

static const char* fmt_mem(size_t kb, char* buf, size_t buf_size)
{
    if (kb >= 1024) {
        snprintf(buf, buf_size, "%.1fMB", kb / 1024.0);
    } else {
        snprintf(buf, buf_size, "%zuKB", kb);
    }
    return buf;
}

void app_mem_report_cb(const ComMemUsage* usage, void* user_data)
{
    (void)user_data;
    if (usage == nullptr) return;

    char rss[16], size[16], hwm[16], peak[16], data[16], stk[16];
    LOG_INFO("[APP_MEM] VmRSS=%s VmSize=%s VmHWM=%s VmPeak=%s "
             "VmData=%s VmStk=%s Threads=%u",
             fmt_mem(usage->vm_rss_kb,  rss,  sizeof(rss)),
             fmt_mem(usage->vm_size_kb, size, sizeof(size)),
             fmt_mem(usage->vm_hwm_kb,  hwm,  sizeof(hwm)),
             fmt_mem(usage->vm_peak_kb, peak, sizeof(peak)),
             fmt_mem(usage->vm_data_kb, data, sizeof(data)),
             fmt_mem(usage->vm_stk_kb,  stk,  sizeof(stk)),
             usage->threads);
}
