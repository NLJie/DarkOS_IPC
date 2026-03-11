/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     通用 JSON 读取器实现
 * FileName: pg_json.c
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include "pg_json.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cJSON *s_root = NULL;

/* ============================================================
 * 点路径导航
 * 支持: "log.dir" / "cameras[0].width" / "cameras"
 * ============================================================ */

static cJSON *navigate(const char *key)
{
    if (!s_root || !key || *key == '\0') return NULL;

    char  buf[256];
    strncpy(buf, key, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    cJSON *node = s_root;
    char  *p    = buf;

    while (p && *p && node) {
        char *dot = strchr(p, '.');
        if (dot) *dot = '\0';

        char *bracket = strchr(p, '[');
        int   idx     = -1;
        if (bracket) {
            *bracket = '\0';
            idx = atoi(bracket + 1);
        }

        if (*p) {
            node = cJSON_GetObjectItemCaseSensitive(node, p);
        }
        if (idx >= 0) {
            node = cJSON_IsArray(node) ? cJSON_GetArrayItem(node, idx) : NULL;
        }

        p = dot ? dot + 1 : NULL;
    }

    return node;
}

/* ============================================================
 * 生命周期
 * ============================================================ */

int pg_json_load(const char *json_path)
{
    pg_json_unload();

    if (!json_path) return 0;

    FILE *fp = fopen(json_path, "r");
    if (!fp) {
        fprintf(stderr, "[pg_json] cannot open: %s\n", json_path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long  fsize = ftell(fp);
    rewind(fp);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(fp); return -1; }

    size_t n = fread(buf, 1, (size_t)fsize, fp);
    buf[n] = '\0';
    fclose(fp);

    s_root = cJSON_Parse(buf);
    free(buf);

    if (!s_root) {
        fprintf(stderr, "[pg_json] parse error: %s\n",
                cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "?");
        return -1;
    }
    return 0;
}

void pg_json_unload(void)
{
    if (s_root) {
        cJSON_Delete(s_root);
        s_root = NULL;
    }
}

/* ============================================================
 * Key-value 读取
 * ============================================================ */

int pg_json_get_int(const char *key, int def)
{
    cJSON *n = navigate(key);
    return cJSON_IsNumber(n) ? n->valueint : def;
}

double pg_json_get_double(const char *key, double def)
{
    cJSON *n = navigate(key);
    return cJSON_IsNumber(n) ? n->valuedouble : def;
}

bool pg_json_get_bool(const char *key, bool def)
{
    cJSON *n = navigate(key);
    if (cJSON_IsTrue(n))  return true;
    if (cJSON_IsFalse(n)) return false;
    return def;
}

const char *pg_json_get_string(const char *key, const char *def)
{
    cJSON *n = navigate(key);
    return (cJSON_IsString(n) && n->valuestring) ? n->valuestring : def;
}

int pg_json_array_len(const char *key)
{
    cJSON *n = navigate(key);
    return cJSON_IsArray(n) ? cJSON_GetArraySize(n) : 0;
}
