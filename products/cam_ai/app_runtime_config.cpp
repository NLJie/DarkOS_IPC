#include <cstring>

#include "app_runtime_config.h"

const AppRuntimeConfig& app_runtime_config()
{
    static const AppRuntimeConfig cfg = [] {
        AppRuntimeConfig c{};

        c.log_cfg.level = LOG_LEVEL_DEBUG;
        c.log_cfg.enable_console = true;
        c.log_cfg.enable_file = true;
        c.log_cfg.enable_timestamp = true;
        c.log_cfg.timestamp_precision = LOG_TIMESTAMP_SECOND;
        c.log_cfg.log_file_path = LOG_FILE_PATH;
        c.log_cfg.max_file_size = LOG_MAX_FILE_SIZE;
        c.log_cfg.max_backup_files = LOG_MAX_BACKUP_FILES;
        c.log_cfg.enable_error_file = true;
        c.log_cfg.error_file_path = ERROR_LOG_FILE_PATH;
        c.log_cfg.max_error_file_size = ERROR_LOG_MAX_FILE_SIZE;

        c.cam_cfg.width = 1920;
        c.cam_cfg.height = 1080;
        c.cam_cfg.fps = 25.0f;
        c.cam_cfg.pixel_format = 0;
        c.cam_cfg.dev_path[0] = '\0';
        // std::strncpy(c.cam_cfg.dev_path, "/dev/video0", sizeof(c.cam_cfg.dev_path) - 1);

        return c;
    }();

    return cfg;
}
