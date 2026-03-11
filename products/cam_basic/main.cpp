#include <csignal>
#include <cstring>
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

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [--config <device_json_path>]\n", prog);
    fprintf(stderr, "  --config   设备配置文件路径 (默认: 使用编译期默认值)\n");
    fprintf(stderr, "  Example: %s --config configs/device_x86_usb_cam_v1.json\n", prog);
}

int main(int argc, char *argv[])
{
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    // 解析命令行参数
    const char *config_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // 加载配置 (JSON 覆盖默认值)
    if (app_runtime_config_load(config_path) != 0) {
        fprintf(stderr, "Warning: config load failed, using defaults\n");
    }
    const AppRuntimeConfig &cfg = app_runtime_config_get();

    if (log_system_init(&cfg.log_cfg) != 0) {
        std::cerr << "Failed to initialize log system" << std::endl;
        return 1;
    }

    if (config_path) {
        LOG_INFO("Loaded device config: %s", config_path);
    } else {
        LOG_INFO("No config file specified, using defaults");
    }
    LOG_INFO("Camera: %dx%d @ %.1ffps", cfg.cam_cfg.width, cfg.cam_cfg.height, cfg.cam_cfg.fps);
    LOG_INFO("Application started");

    void *cam = open_camera(cfg.cam_cfg);
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

    LOG_INFO("Application finished. Logs: %s / %s",
             cfg.log_cfg.log_file_path, cfg.log_cfg.error_file_path);

    log_system_shutdown();
    return 0;
}
