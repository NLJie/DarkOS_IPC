/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     ONVIF 客户端接口
 *
 *   职责: 局域网设备发现 + 获取 RTSP 流地址.
 *         完成后交给 HAL 层 (hal_cam_open) 处理, 本模块不做流媒体传输.
 *
 *   依赖: gsoap (需集成到 third_party/gsoap/)
 *         集成方法: https://www.genivia.com/doc/soapdoc2.html
 *
 *   典型用法:
 *     // 1. 发现设备
 *     PgOnvifDevice devs[8];
 *     int n = pg_onvif_discover(devs, 8, 2000);
 *
 *     // 2. 获取 RTSP 地址并填入 HalCamConfig
 *     HalCamConfig cam_cfg;
 *     pg_onvif_fill_cam_config(&devs[0], "admin", "12345", &cam_cfg);
 *
 *     // 3. 走统一 HAL 接口
 *     void *cam = hal_cam_open(&cam_cfg);
 *
 * FileName: pg_onvif.h
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#ifndef PG_ONVIF_H
#define PG_ONVIF_H

#include "hal_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 发现到的设备描述
 * ============================================================ */

#define PG_ONVIF_MAX_PROFILES  8   /* 一台摄像头最多返回的媒体 profile 数 */

typedef struct {
    char device_ip[64];            /**< 设备 IP 地址, 如 "192.168.1.100"   */
    char device_name[128];         /**< 厂商/型号, 如 "Hikvision DS-2CD2T" */
    char xaddr[256];               /**< ONVIF 服务端点 URL                  */
} PgOnvifDevice;

/** 媒体 profile (一个摄像头可能有主码流/子码流等多个 profile) */
typedef struct {
    char token[64];                /**< profile token, GetStreamUri 时用    */
    char name[64];                 /**< "MainStream" / "SubStream" ...      */
    int  width;
    int  height;
    float fps;
} PgOnvifProfile;

/* ============================================================
 * 设备发现
 * ============================================================ */

/**
 * @brief 扫描局域网, 发现所有 ONVIF 摄像头 (WS-Discovery)
 *
 * 发送 UDP 组播探测包到 239.255.255.250:3702, 收集响应.
 *
 * @param out_devices  输出设备数组
 * @param max_count    数组容量
 * @param timeout_ms   等待响应超时 (推荐 2000ms)
 * @return 发现的设备数量; 负值表示错误
 */
int pg_onvif_discover(PgOnvifDevice *out_devices, int max_count, int timeout_ms);

/* ============================================================
 * 设备连接与信息获取
 * ============================================================ */

/**
 * @brief 获取设备支持的媒体 profile 列表 (GetProfiles)
 *
 * @param device      目标设备
 * @param username    登录账号
 * @param password    登录密码
 * @param out_profiles  输出 profile 数组
 * @param max_count   数组容量 (推荐 PG_ONVIF_MAX_PROFILES)
 * @return profile 数量; 负值表示错误 (鉴权失败/网络超时)
 */
int pg_onvif_get_profiles(const PgOnvifDevice *device,
                          const char *username,
                          const char *password,
                          PgOnvifProfile *out_profiles,
                          int max_count);

/**
 * @brief 获取指定 profile 的 RTSP 流地址 (GetStreamUri)
 *
 * @param device       目标设备
 * @param username     登录账号
 * @param password     登录密码
 * @param profile      目标 profile (来自 pg_onvif_get_profiles)
 * @param out_url      输出 RTSP URL 缓冲区
 * @param url_size     缓冲区大小
 * @return 0 成功; -1 失败
 */
int pg_onvif_get_stream_uri(const PgOnvifDevice *device,
                            const char *username,
                            const char *password,
                            const PgOnvifProfile *profile,
                            char *out_url,
                            int url_size);

/* ============================================================
 * 便捷接口 (发现 → 鉴权 → 获取 URL → 填充 HalCamConfig 一步完成)
 * ============================================================ */

/**
 * @brief 从 ONVIF 设备获取第一路主码流并填充 HalCamConfig
 *
 * 内部依次执行: GetProfiles → GetStreamUri, 选取第 profile_idx 路码流.
 * 填充完成后 out_cfg->interface = HAL_CAM_IF_RTSP, 可直接传入 hal_cam_open().
 *
 * @param device       目标设备
 * @param username     登录账号
 * @param password     登录密码
 * @param profile_idx  选取第几路 profile (0 = 主码流)
 * @param out_cfg      输出 HalCamConfig
 * @return 0 成功; -1 失败
 */
int pg_onvif_fill_cam_config(const PgOnvifDevice *device,
                             const char *username,
                             const char *password,
                             int profile_idx,
                             HalCamConfig *out_cfg);

/* ============================================================
 * PTZ 云台控制 (可选, 摄像头需支持)
 * ============================================================ */

/**
 * @brief 云台绝对移动
 * @param pan   水平角度 [-1.0, 1.0]
 * @param tilt  垂直角度 [-1.0, 1.0]
 * @param zoom  变焦     [ 0.0, 1.0]
 */
int pg_onvif_ptz_absolute(const PgOnvifDevice *device,
                          const char *username,
                          const char *password,
                          const char *profile_token,
                          float pan, float tilt, float zoom);

/** @brief 停止云台运动 */
int pg_onvif_ptz_stop(const PgOnvifDevice *device,
                      const char *username,
                      const char *password,
                      const char *profile_token);

#ifdef __cplusplus
}
#endif

#endif /* PG_ONVIF_H */
