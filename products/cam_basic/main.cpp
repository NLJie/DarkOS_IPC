#include <csignal>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <chrono>
#include <thread>

// OpenCV 和 FFmpeg 是可选依赖
#ifdef PG_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

#ifdef PG_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#endif

#include "pg_json.h"
#include "pg_device_config.h"
#include "app_camera.h"
#include "app_mem.h"
#include "com_mem_monitor.h"
#include "log_system.h"
#include "video_recorder.h"

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
#ifdef PG_HAS_OPENCV
    LOG_INFO("OpenCV: %s", cv::getVersionString().c_str());
#else
    LOG_INFO("OpenCV: not available");
#endif

#ifdef PG_HAS_FFMPEG
    LOG_INFO("FFmpeg avcodec:  %s", av_version_info());
    LOG_INFO("FFmpeg avformat: %s", AV_STRINGIFY(LIBAVFORMAT_VERSION));
#else
    LOG_INFO("FFmpeg: not available");
#endif

    if (config_path) {
        LOG_INFO("Config: %s", config_path);
    }

    /* ---- 摄像头: 按数组动态处理 ---- */
    int cam_count = pg_json_array_len("cameras");
    LOG_INFO("Camera count: %d", cam_count);

    void *cam = NULL;
    HalCamConfig cam_cfg;
    if (cam_count > 0) {
        pg_dev_cfg_fill_cam_config(0, &cam_cfg);   /* 当前只用第 0 路 */
        cam = open_camera(cam_cfg);
    } else {
        LOG_INFO("No camera configured, skipping camera init");
    }

    /* ---- 开始录制 ---- */
    if (cam) {
        LOG_INFO("Starting video recording...");
        video_recorder_start(cam_cfg.width, cam_cfg.height, (int)cam_cfg.fps);
    }

    /* ---- 内存监控 ---- */
    if (com_mem_monitor_start(5000, app_mem_report_cb, NULL) != 0) {
        LOG_WARN("com_mem_monitor_start failed");
    }

    /* ---- 录制循环 ---- */
    LOG_INFO("Recording... Press Ctrl+C to stop.");
    int frame_count = 0;
    auto last_time = std::chrono::steady_clock::now();
    
    while (g_running && video_recorder_is_recording()) {
        HalVideoFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        if (hal_cam_read_frame(cam, &frame) == 0) {
            video_recorder_encode_frame(&frame);
            frame_count++;
            
            // 控制帧率，避免 CPU 占用过高
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
            int target_interval = 1000 / (int)cam_cfg.fps;  // 目标帧间隔 ms
            int sleep_ms = target_interval - (int)elapsed;
            if (sleep_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
            last_time = std::chrono::steady_clock::now();
        } else {
            // 读取失败时短暂休眠
            usleep(10000);  // 10ms
        }
    }
    
    LOG_INFO("Shutting down...");
    LOG_INFO("Total frames recorded: %d", frame_count);

    /* ---- 清理 ---- */
    com_mem_monitor_stop();
    if (cam) hal_cam_close(cam);
    pg_json_unload();
    log_system_shutdown();

    return 0;
}
