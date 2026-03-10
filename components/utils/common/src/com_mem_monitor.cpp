#include "com_mem_monitor.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static pthread_t g_monitor_thread;
static pthread_mutex_t g_monitor_lock = PTHREAD_MUTEX_INITIALIZER;

static bool g_started = false;
static bool g_stop_requested = false;
static unsigned int g_interval_ms = 1000;
static ComMemReportCallback g_callback = NULL;
static void* g_user_data = NULL;

static int parse_kb_value(const char* line, const char* key, size_t* out_val) {
    // 例: "VmRSS:	   12345 kB"
    if (strncmp(line, key, strlen(key)) != 0) {
        return -1;
    }

    size_t val = 0;
    if (sscanf(line + strlen(key), "%zu", &val) != 1) {
        return -1;
    }

    *out_val = val;
    return 0;
}

static int parse_uint_value(const char* line, const char* key, unsigned int* out_val) {
    // 例: "Threads:	4"
    if (strncmp(line, key, strlen(key)) != 0) {
        return -1;
    }

    unsigned int val = 0;
    if (sscanf(line + strlen(key), "%u", &val) != 1) {
        return -1;
    }

    *out_val = val;
    return 0;
}

int com_mem_get_usage(ComMemUsage* usage) {
    if (usage == NULL) {
        return -1;
    }

    FILE* fp = fopen("/proc/self/status", "r");
    if (fp == NULL) {
        return -1;
    }

    memset(usage, 0, sizeof(*usage));

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        (void)parse_kb_value(line, "VmSize:", &usage->vm_size_kb);
        (void)parse_kb_value(line, "VmRSS:",  &usage->vm_rss_kb);
        (void)parse_kb_value(line, "VmPeak:", &usage->vm_peak_kb);
        (void)parse_kb_value(line, "VmHWM:",  &usage->vm_hwm_kb);
        (void)parse_kb_value(line, "VmData:", &usage->vm_data_kb);
        (void)parse_kb_value(line, "VmStk:",  &usage->vm_stk_kb);
        (void)parse_uint_value(line, "Threads:", &usage->threads);
    }

    fclose(fp);
    return 0;
}

static void sleep_ms(unsigned int ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

static void* monitor_thread_main(void* arg) {
    (void)arg;

    while (1) {
        ComMemReportCallback cb = NULL;
        void* user = NULL;
        unsigned int interval = 1000;
        bool stop = false;

        pthread_mutex_lock(&g_monitor_lock);
        stop = g_stop_requested;
        cb = g_callback;
        user = g_user_data;
        interval = g_interval_ms;
        pthread_mutex_unlock(&g_monitor_lock);

        if (stop) {
            break;
        }

        ComMemUsage usage;
        if (cb != NULL && com_mem_get_usage(&usage) == 0) {
            cb(&usage, user);
        }

        sleep_ms(interval);
    }

    return NULL;
}

int com_mem_monitor_start(unsigned int interval_ms,
                                ComMemReportCallback cb,
                                void* user_data) {
    if (cb == NULL) {
        return -1;
    }

    if (interval_ms < 100) {
        interval_ms = 100;
    }

    pthread_mutex_lock(&g_monitor_lock);
    if (g_started) {
        pthread_mutex_unlock(&g_monitor_lock);
        return -1;
    }

    g_interval_ms = interval_ms;
    g_callback = cb;
    g_user_data = user_data;
    g_stop_requested = false;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024); // 64KB，远大于实际需求

    int ret = pthread_create(&g_monitor_thread, &attr, monitor_thread_main, NULL);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        g_callback = NULL;
        g_user_data = NULL;
        pthread_mutex_unlock(&g_monitor_lock);
        return -1;
    }

    g_started = true;
    pthread_mutex_unlock(&g_monitor_lock);
    return 0;
}

int com_mem_monitor_stop(void) {
    pthread_t tid;

    pthread_mutex_lock(&g_monitor_lock);
    if (!g_started) {
        pthread_mutex_unlock(&g_monitor_lock);
        return 0;
    }

    g_stop_requested = true;
    tid = g_monitor_thread;
    pthread_mutex_unlock(&g_monitor_lock);

    (void)pthread_join(tid, NULL);

    pthread_mutex_lock(&g_monitor_lock);
    g_started = false;
    g_callback = NULL;
    g_user_data = NULL;
    pthread_mutex_unlock(&g_monitor_lock);

    return 0;
}

bool com_mem_monitor_is_running(void) {
    bool running;
    pthread_mutex_lock(&g_monitor_lock);
    running = g_started && !g_stop_requested;
    pthread_mutex_unlock(&g_monitor_lock);
    return running;
}