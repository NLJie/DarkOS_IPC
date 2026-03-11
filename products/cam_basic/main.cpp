#include <csignal>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#include "pg_json.h"
#include "pg_device_config.h"
#include "app_camera.h"
#include "app_mem.h"
#include "com_mem_monitor.h"
#include "log_system.h"

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig) { (void)sig; g_running = 0; }

int main(int argc, char *argv[])
{
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    /* ---- 解析 --config 参数 ---- */
    const char *config_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "Usage: %s [--config <device_json>]\n", argv[0]);
            return 0;
        }
    }

    /* ---- 加载设备配置 ---- */
    if (pg_json_load(config_path) != 0) {
        fprintf(stderr, "[main] config load failed, using defaults\n");
    }

    /* ---- 初始化日志 (配置从 JSON 的 log.* 读取) ---- */
    LogConfig log_cfg;
    pg_dev_cfg_fill_log_config(&log_cfg);
    if (log_system_init(&log_cfg) != 0) {
        fprintf(stderr, "[main] log system init failed\n");
        return 1;
    }

    LOG_INFO("Device: %s", pg_json_get_string("device_name", "unknown"));
    if (config_path) {
        LOG_INFO("Config: %s", config_path);
    }

    /* ---- 摄像头: 按数组动态处理 ---- */
    int cam_count = pg_json_array_len("cameras");
    LOG_INFO("Camera count: %d", cam_count);

    void *cam = NULL;
    if (cam_count > 0) {
        HalCamConfig cam_cfg;
        pg_dev_cfg_fill_cam_config(0, &cam_cfg);   /* 当前只用第 0 路 */
        cam = open_camera(cam_cfg);
        if (cam) read_frame_once(cam);
    } else {
        LOG_INFO("No camera configured, skipping camera init");
    }

    /* ---- 内存监控 ---- */
    if (com_mem_monitor_start(5000, app_mem_report_cb, NULL) != 0) {
        LOG_WARN("com_mem_monitor_start failed");
    }

    /* ---- 主循环 ---- */
    LOG_INFO("Running. Press Ctrl+C to stop.");
    while (g_running) {
        sleep(1);
    }
    LOG_INFO("Shutting down...");

    /* ---- 清理 ---- */
    com_mem_monitor_stop();
    if (cam) hal_cam_close(cam);
    pg_json_unload();
    log_system_shutdown();

    return 0;
}
