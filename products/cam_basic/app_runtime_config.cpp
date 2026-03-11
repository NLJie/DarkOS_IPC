#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "app_runtime_config.h"
#include "cJSON.h"

/* ============================================================
 * 内部状态
 * ============================================================ */

static AppRuntimeConfig g_cfg;
static bool             g_loaded = false;

/* ============================================================
 * 工具: 日志级别字符串 → 枚举
 * ============================================================ */

static LogLevel parse_log_level(const char *s)
{
    if (!s)                          return LOG_LEVEL_DEBUG;
    if (strcmp(s, "debug") == 0)     return LOG_LEVEL_DEBUG;
    if (strcmp(s, "info")  == 0)     return LOG_LEVEL_INFO;
    if (strcmp(s, "warn")  == 0)     return LOG_LEVEL_WARN;
    if (strcmp(s, "error") == 0)     return LOG_LEVEL_ERROR;
    if (strcmp(s, "none")  == 0)     return LOG_LEVEL_NONE;
    fprintf(stderr, "[app_cfg] unknown log level '%s', using debug\n", s);
    return LOG_LEVEL_DEBUG;
}

/* ============================================================
 * 兜底默认值填充
 * ============================================================ */

static void fill_defaults(AppRuntimeConfig &c)
{
    /* 日志路径 */
    snprintf(c.log_file_path,   sizeof(c.log_file_path),
             "%s/app.log",     PG_FALLBACK_LOG_DIR);
    snprintf(c.error_file_path, sizeof(c.error_file_path),
             "%s/app_err.log", PG_FALLBACK_LOG_DIR);

    /* LogConfig (指针指向上面的数组) */
    c.log_cfg.level               = LOG_LEVEL_DEBUG;
    c.log_cfg.enable_console      = true;
    c.log_cfg.enable_file         = true;
    c.log_cfg.enable_timestamp    = true;
    c.log_cfg.timestamp_precision = LOG_TIMESTAMP_SECOND;
    c.log_cfg.log_file_path       = c.log_file_path;
    c.log_cfg.max_file_size       = PG_FALLBACK_LOG_SIZE;
    c.log_cfg.max_backup_files    = PG_FALLBACK_LOG_BACKUPS;
    c.log_cfg.enable_error_file   = true;
    c.log_cfg.error_file_path     = c.error_file_path;
    c.log_cfg.max_error_file_size = PG_FALLBACK_ERROR_LOG_SIZE;

    /* 摄像头 */
    c.cam_cfg.width        = 1920;
    c.cam_cfg.height       = 1080;
    c.cam_cfg.fps          = 25.0f;
    c.cam_cfg.pixel_format = 0;
    c.cam_cfg.dev_path[0]  = '\0';

    /* 路径 */
    strncpy(c.data_dir,  "/var/lib/pg_camera",    sizeof(c.data_dir)  - 1);
    strncpy(c.temp_dir,  "/tmp/pg_camera",         sizeof(c.temp_dir)  - 1);
    strncpy(c.media_dir, "/opt/pg_camera/media",   sizeof(c.media_dir) - 1);

    /* AI */
    strncpy(c.ai_model_path, "/opt/pg_camera/models/model.rknn",
            sizeof(c.ai_model_path) - 1);
    c.ai_threshold = 0.5f;
}

/* ============================================================
 * JSON 覆盖 (缺失字段保持默认值不变)
 * ============================================================ */

static void apply_json(const char *json_str, AppRuntimeConfig &c)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "[app_cfg] JSON parse error near: %s\n",
                cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        return;
    }

    /* --- log --- */
    cJSON *log = cJSON_GetObjectItemCaseSensitive(root, "log");
    if (cJSON_IsObject(log)) {
        cJSON *item;

        item = cJSON_GetObjectItemCaseSensitive(log, "dir");
        if (cJSON_IsString(item) && item->valuestring) {
            snprintf(c.log_file_path,   sizeof(c.log_file_path),
                     "%s/app.log",     item->valuestring);
            snprintf(c.error_file_path, sizeof(c.error_file_path),
                     "%s/app_err.log", item->valuestring);
            /* 指针跟随数组, 无需额外赋值 (fill_defaults 已经指向数组) */
        }

        item = cJSON_GetObjectItemCaseSensitive(log, "max_size_mb");
        if (cJSON_IsNumber(item))
            c.log_cfg.max_file_size = (size_t)(item->valuedouble * 1024 * 1024);

        item = cJSON_GetObjectItemCaseSensitive(log, "max_backups");
        if (cJSON_IsNumber(item))
            c.log_cfg.max_backup_files = item->valueint;

        item = cJSON_GetObjectItemCaseSensitive(log, "level");
        if (cJSON_IsString(item))
            c.log_cfg.level = parse_log_level(item->valuestring);

        item = cJSON_GetObjectItemCaseSensitive(log, "console");
        if (cJSON_IsBool(item))
            c.log_cfg.enable_console = cJSON_IsTrue(item);

        item = cJSON_GetObjectItemCaseSensitive(log, "timestamp_ms");
        if (cJSON_IsBool(item))
            c.log_cfg.timestamp_precision =
                cJSON_IsTrue(item) ? LOG_TIMESTAMP_MILLISECOND : LOG_TIMESTAMP_SECOND;
    }

    /* --- camera --- */
    cJSON *cam = cJSON_GetObjectItemCaseSensitive(root, "camera");
    if (cJSON_IsObject(cam)) {
        cJSON *item;

        item = cJSON_GetObjectItemCaseSensitive(cam, "width");
        if (cJSON_IsNumber(item)) c.cam_cfg.width = item->valueint;

        item = cJSON_GetObjectItemCaseSensitive(cam, "height");
        if (cJSON_IsNumber(item)) c.cam_cfg.height = item->valueint;

        item = cJSON_GetObjectItemCaseSensitive(cam, "fps");
        if (cJSON_IsNumber(item)) c.cam_cfg.fps = (float)item->valuedouble;

        item = cJSON_GetObjectItemCaseSensitive(cam, "dev_path");
        if (cJSON_IsString(item) && item->valuestring)
            strncpy(c.cam_cfg.dev_path, item->valuestring,
                    sizeof(c.cam_cfg.dev_path) - 1);
    }

    /* --- paths --- */
    cJSON *paths = cJSON_GetObjectItemCaseSensitive(root, "paths");
    if (cJSON_IsObject(paths)) {
        cJSON *item;

        item = cJSON_GetObjectItemCaseSensitive(paths, "data_dir");
        if (cJSON_IsString(item) && item->valuestring)
            strncpy(c.data_dir, item->valuestring, sizeof(c.data_dir) - 1);

        item = cJSON_GetObjectItemCaseSensitive(paths, "temp_dir");
        if (cJSON_IsString(item) && item->valuestring)
            strncpy(c.temp_dir, item->valuestring, sizeof(c.temp_dir) - 1);

        item = cJSON_GetObjectItemCaseSensitive(paths, "media_dir");
        if (cJSON_IsString(item) && item->valuestring)
            strncpy(c.media_dir, item->valuestring, sizeof(c.media_dir) - 1);
    }

    /* --- ai --- */
    cJSON *ai = cJSON_GetObjectItemCaseSensitive(root, "ai");
    if (cJSON_IsObject(ai)) {
        cJSON *item;

        item = cJSON_GetObjectItemCaseSensitive(ai, "model_path");
        if (cJSON_IsString(item) && item->valuestring)
            strncpy(c.ai_model_path, item->valuestring,
                    sizeof(c.ai_model_path) - 1);

        item = cJSON_GetObjectItemCaseSensitive(ai, "threshold");
        if (cJSON_IsNumber(item)) c.ai_threshold = (float)item->valuedouble;
    }

    cJSON_Delete(root);
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int app_runtime_config_load(const char *json_path)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    fill_defaults(g_cfg);
    g_loaded = true;

    if (!json_path) return 0;

    FILE *fp = fopen(json_path, "r");
    if (!fp) {
        fprintf(stderr, "[app_cfg] cannot open: %s\n", json_path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(fp); return -1; }

    size_t ret = fread(buf, 1, (size_t)fsize, fp);
    if (ret != (size_t)fsize) {
        printf("fread failed: expected %zu, got %zu\n", (size_t)fsize, ret);
    }
    buf[fsize] = '\0';
    fclose(fp);

    apply_json(buf, g_cfg);
    free(buf);

    /* fill_defaults 已经让 log_cfg 指针指向 g_cfg 内部数组,
       apply_json 修改的是数组内容, 指针值不变, 无需重新赋值. */

    return 0;
}

const AppRuntimeConfig &app_runtime_config_get()
{
    if (!g_loaded) {
        memset(&g_cfg, 0, sizeof(g_cfg));
        fill_defaults(g_cfg);
        g_loaded = true;
    }
    return g_cfg;
}
