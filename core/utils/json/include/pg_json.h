/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     通用 JSON 读取器
 *           带点路径导航的 key-value 访问接口, 与业务无关.
 *           可被任意模块复用 (device_config / network / OTA ...).
 *
 *   Key 路径语法:
 *     "log.dir"           → 对象嵌套
 *     "cameras[0].width"  → 数组下标 + 嵌套
 *     "cameras"           → 数组本身 (用于 array_len)
 *
 * FileName: pg_json.h
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#ifndef PG_JSON_H
#define PG_JSON_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 加载 JSON 文件 (全局单例, 覆盖上次加载的内容)
 * @param json_path  文件路径; NULL = 清空当前内容 (所有 get 返回默认值)
 * @return 0 成功; -1 文件读取或解析失败
 */
int  pg_json_load  (const char *json_path);

/** @brief 释放 JSON 树. 之后 get_string 返回值不再有效. */
void pg_json_unload(void);

/* ============================================================
 * Key-value 读取
 * ============================================================ */

int         pg_json_get_int   (const char *key, int         def);
double      pg_json_get_double(const char *key, double      def);
bool        pg_json_get_bool  (const char *key, bool        def);

/**
 * @brief 读取字符串
 * @return 指向 cJSON 内部缓冲区的指针 (unload 前有效); 未找到返回 def
 */
const char *pg_json_get_string(const char *key, const char *def);

/**
 * @brief 获取数组长度; key 不存在或不是数组时返回 0
 */
int         pg_json_array_len (const char *key);

#ifdef __cplusplus
}
#endif

#endif /* PG_JSON_H */
