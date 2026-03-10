#include <cstring>
#include <iostream>
#include <csignal>
#include <unistd.h>

#include "com_mem_monitor.h"
#include "hal_camera.h"
#include "log_system.h"
#include "app_runtime_config.h"

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void app_mem_report_cb(const CommonMemoryUsage* usage, void* user_data) {
    (void)user_data;
    if (usage == nullptr) return;

    LOG_INFO("[APP_MEM] VmRSS=%zuKB VmSize=%zuKB VmHWM=%zuKB VmPeak=%zuKB",
             usage->vm_rss_kb,
             usage->vm_size_kb,
             usage->vm_hwm_kb,
             usage->vm_peak_kb);
}

static bool init_log_system(const LogConfig& log_config)
{
    if (log_system_init(&log_config) != 0) {
        std::cerr << "Failed to initialize log system" << std::endl;
        return false;
    }

    LOG_INFO("Application started");
    LOG_DEBUG("Log system initialized: file=%s, error_file=%s",
              log_config.log_file_path, log_config.error_file_path);
    return true;
}

static void* open_camera(const HalCamConfig& cfg)
{
    LOG_INFO("Opening camera device: %s", cfg.dev_path);
    void* cam = hal_cam_open(&cfg);
    if (cam == nullptr) {
        LOG_ERROR("hal_cam_open failed");
        return nullptr;
    }
    LOG_INFO("Camera opened successfully");
    return cam;
}

static void read_frame_once(void* cam)
{
    HalVideoFrame frame;
    std::memset(&frame, 0, sizeof(frame));
    LOG_DEBUG("Attempting to read frame...");
    if (hal_cam_read_frame(cam, &frame) == 0) {
        LOG_INFO("Frame received: %dx%d, size=%zu, ts=%lld",
                 frame.width, frame.height, frame.size,
                 static_cast<long long>(frame.timestamp_us));
    } else {
        LOG_WARN("hal_cam_read_frame not available in current backend");
    }
}

int main()
{
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    const AppRuntimeConfig& runtime_cfg = app_runtime_config();

    if (!init_log_system(runtime_cfg.log_cfg)) {
        return 1;
    }

    void* cam = open_camera(runtime_cfg.cam_cfg);
    if (cam == nullptr) {
        log_system_shutdown();
        return 1;
    }

    read_frame_once(cam);

    if (com_mem_monitor_start(2000, app_mem_report_cb, nullptr) != 0) {
        LOG_WARN("com_mem_monitor_start failed");
    }

    LOG_INFO("Running. Press Ctrl+C to stop.");
    while (g_running) {
        sleep(1);
    }
    LOG_INFO("Shutting down...");

    (void)com_mem_monitor_stop();

    LOG_INFO("Closing camera");
    hal_cam_close(cam);

    LOG_INFO("Application finished. Check logs: %s and %s",
             runtime_cfg.log_cfg.log_file_path, runtime_cfg.log_cfg.error_file_path);

    log_system_shutdown();

    return 0;
}
