/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     设备配置 fill 函数实现 (调用 pg_json 通用接口)
 * FileName: pg_device_config.c
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#include "pg_device_config.h"
#include "pg_json.h"

#include <stdio.h>
#include <string.h>

#define DEV_PATH_MAX 256

static char s_log_file_path[DEV_PATH_MAX];
static char s_error_file_path[DEV_PATH_MAX];

static LogLevel parse_level(const char *s)
{
    if (!s)                       return LOG_LEVEL_DEBUG;
    if (strcmp(s, "debug") == 0)  return LOG_LEVEL_DEBUG;
    if (strcmp(s, "info")  == 0)  return LOG_LEVEL_INFO;
    if (strcmp(s, "warn")  == 0)  return LOG_LEVEL_WARN;
    if (strcmp(s, "error") == 0)  return LOG_LEVEL_ERROR;
    if (strcmp(s, "none")  == 0)  return LOG_LEVEL_NONE;
    return LOG_LEVEL_DEBUG;
}

int pg_dev_cfg_fill_log_config(LogConfig *out)
{
    if (!out) return -1;

    const char *log_dir = pg_json_get_string("log.dir", "./log");
    snprintf(s_log_file_path,   sizeof(s_log_file_path),   "%s/app.log",     log_dir);
    snprintf(s_error_file_path, sizeof(s_error_file_path), "%s/app_err.log", log_dir);

    out->log_file_path       = s_log_file_path;
    out->error_file_path     = s_error_file_path;
    out->level               = parse_level(pg_json_get_string("log.level", "debug"));
    out->enable_console      = pg_json_get_bool("log.console",      true);
    out->enable_file         = pg_json_get_bool("log.file",         true);
    out->enable_timestamp    = pg_json_get_bool("log.time",         true);
    out->timestamp_precision = pg_json_get_bool("log.timestamp_ms", false)
                               ? LOG_TIMESTAMP_MILLISECOND : LOG_TIMESTAMP_SECOND;

    double max_mb            = pg_json_get_double("log.max_size_mb", 1.0);
    out->max_file_size       = (size_t)(max_mb * 1024.0 * 1024.0);
    out->max_backup_files    = pg_json_get_int("log.max_backups", 3);
    out->enable_error_file   = true;
    out->max_error_file_size = out->max_file_size / 2;

    return 0;
}

static HalCamInterface parse_interface(const char *s)
{
    if (!s || strcmp(s, "usb") == 0)     return HAL_CAM_IF_USB;
    if (strcmp(s, "mipi")    == 0)       return HAL_CAM_IF_MIPI;
    if (strcmp(s, "dvp")     == 0)       return HAL_CAM_IF_DVP;
    if (strcmp(s, "rtsp")    == 0)       return HAL_CAM_IF_RTSP;
    if (strcmp(s, "virtual") == 0)       return HAL_CAM_IF_VIRTUAL;
    return HAL_CAM_IF_USB;
}

int pg_dev_cfg_fill_cam_config(int idx, HalCamConfig *out)
{
    if (!out || idx < 0 || idx >= pg_json_array_len("cameras")) return -1;

    char key[64];
    memset(out, 0, sizeof(*out));

#define CAM(field) (snprintf(key, sizeof(key), "cameras[%d]." field, idx), key)

    /* 通用参数 */
    out->interface    = parse_interface(pg_json_get_string(CAM("interface"), "usb"));
    out->width        = pg_json_get_int   (CAM("width"),        1920);
    out->height       = pg_json_get_int   (CAM("height"),       1080);
    out->fps          = (float)pg_json_get_double(CAM("fps"),   25.0);
    out->pixel_format = pg_json_get_int   (CAM("pixel_format"), 0);

    /* 接口专属参数 */
    switch (out->interface) {
    case HAL_CAM_IF_USB:
    case HAL_CAM_IF_DVP: {
        const char *dev = pg_json_get_string(CAM("dev_path"), "");
        strncpy(out->usb.dev_path, dev, sizeof(out->usb.dev_path) - 1);
        break;
    }
    case HAL_CAM_IF_MIPI: {
        const char *dev = pg_json_get_string(CAM("dev_path"), "");
        strncpy(out->mipi.dev_path, dev, sizeof(out->mipi.dev_path) - 1);
        out->mipi.lanes = pg_json_get_int(CAM("mipi_lanes"), 2);
        const char *sensor = pg_json_get_string(CAM("sensor"), "");
        strncpy(out->mipi.sensor, sensor, sizeof(out->mipi.sensor) - 1);
        break;
    }
    case HAL_CAM_IF_RTSP: {
        const char *url  = pg_json_get_string(CAM("rtsp_url"),  "");
        const char *user = pg_json_get_string(CAM("username"),  "");
        const char *pass = pg_json_get_string(CAM("password"),  "");
        strncpy(out->rtsp.url,      url,  sizeof(out->rtsp.url)      - 1);
        strncpy(out->rtsp.username, user, sizeof(out->rtsp.username)  - 1);
        strncpy(out->rtsp.password, pass, sizeof(out->rtsp.password)  - 1);
        break;
    }
    case HAL_CAM_IF_VIRTUAL: {
        const char *fp = pg_json_get_string(CAM("file_path"), "");
        strncpy(out->virt.file_path, fp, sizeof(out->virt.file_path) - 1);
        break;
    }
    }

#undef CAM
    return 0;
}
