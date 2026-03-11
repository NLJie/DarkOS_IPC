#include <csignal>
#include <iostream>
#include <unistd.h>

#include "app_camera.h"
#include "app_mem.h"
#include "app_runtime_config.h"
#include "com_mem_monitor.h"
#include "log_system.h"

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    const AppRuntimeConfig& cfg = app_runtime_config();

    if (log_system_init(&cfg.log_cfg) != 0) {
        std::cerr << "Failed to initialize log system" << std::endl;
        return 1;
    }
    LOG_INFO("Application started");

    void* cam = open_camera(cfg.cam_cfg);
    if (cam == nullptr) {
        log_system_shutdown();
        return 1;
    }

    read_frame_once(cam);

    if (com_mem_monitor_start(5000, app_mem_report_cb, nullptr) != 0) {
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
             cfg.log_cfg.log_file_path, cfg.log_cfg.error_file_path);

    log_system_shutdown();

    return 0;
}
