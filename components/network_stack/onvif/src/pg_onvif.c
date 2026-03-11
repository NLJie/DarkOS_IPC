/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     ONVIF 客户端实现
 *
 *   当前状态: 接口框架已定义, 实现待集成 gsoap 后补全.
 *
 *   集成 gsoap 步骤:
 *     1. 下载 gsoap: https://sourceforge.net/projects/gsoap2/
 *     2. 放到 third_party/gsoap/
 *     3. 用 wsdl2h + soapcpp2 从 ONVIF WSDL 生成桩代码
 *        $ wsdl2h -o onvif.h device.wsdl media.wsdl ptz.wsdl
 *        $ soapcpp2 -I gsoap/import -C onvif.h
 *     4. 在下方 TODO 处填入 gsoap 调用
 *
 *   ONVIF 规范文档: https://www.onvif.org/specs/
 *
 * FileName: pg_onvif.c
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#include "pg_onvif.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * 内部工具
 * ============================================================ */

/**
 * 生成 WS-Security UsernameToken 摘要鉴权头.
 * ONVIF 要求密码用 Base64(SHA1(nonce + created + password)) 方式签名.
 *
 * TODO: 实现或复用 gsoap 生成的 soap_wsse_add_UsernameTokenDigest()
 */
static int build_auth_header(const char *username, const char *password,
                             char *out_header, int header_size)
{
    /* TODO: gsoap WS-Security 鉴权 */
    (void)username; (void)password; (void)out_header; (void)header_size;
    return 0;
}

/* ============================================================
 * 设备发现 — WS-Discovery
 * ============================================================ */

int pg_onvif_discover(PgOnvifDevice *out_devices, int max_count, int timeout_ms)
{
    if (!out_devices || max_count <= 0) return -1;

    /**
     * WS-Discovery 流程:
     *   1. 创建 UDP socket 绑定到 239.255.255.250:3702
     *   2. 发送 Probe SOAP 消息 (搜索 NetworkVideoTransmitter 类型设备)
     *   3. 等待 timeout_ms 毫秒, 收集 ProbeMatch 响应
     *   4. 解析响应中的 XAddrs (设备服务端点 URL) 和 Scopes (设备名称)
     *
     * TODO: 实现 UDP 组播 + XML 解析, 或使用 gsoap wsdd 插件:
     *   struct soap *soap = soap_new();
     *   soap_register_plugin(soap, wsdd);
     *   wsdd_Probe(soap, ...);
     */

    fprintf(stderr, "[pg_onvif] discover: not implemented (gsoap required)\n");
    return 0;
}

/* ============================================================
 * 获取媒体 Profile 列表
 * ============================================================ */

int pg_onvif_get_profiles(const PgOnvifDevice *device,
                          const char *username,
                          const char *password,
                          PgOnvifProfile *out_profiles,
                          int max_count)
{
    if (!device || !out_profiles || max_count <= 0) return -1;

    /**
     * ONVIF Media Service: GetProfiles
     *
     * HTTP POST 到 device->xaddr (Media 服务端点)
     * 请求体: SOAP GetProfiles
     * 响应: Profile 列表, 每个含 token / Name / VideoEncoderConfiguration
     *
     * TODO:
     *   struct soap *soap = soap_new();
     *   build_auth_header(username, password, ...);
     *   soap_call___trt__GetProfiles(soap, device->xaddr, NULL, &req, &resp);
     *   for each resp.Profiles: 填入 out_profiles[i]
     */

    (void)username; (void)password;
    fprintf(stderr, "[pg_onvif] get_profiles: not implemented (gsoap required)\n");
    return -1;
}

/* ============================================================
 * 获取 RTSP 流地址
 * ============================================================ */

int pg_onvif_get_stream_uri(const PgOnvifDevice *device,
                            const char *username,
                            const char *password,
                            const PgOnvifProfile *profile,
                            char *out_url,
                            int url_size)
{
    if (!device || !profile || !out_url || url_size <= 0) return -1;

    /**
     * ONVIF Media Service: GetStreamUri
     *
     * 请求体: SOAP GetStreamUri, 携带 profile->token
     *         Transport.Protocol = "RTSP"
     * 响应:   MediaUri.Uri = "rtsp://ip:554/..."
     *
     * TODO:
     *   struct _trt__GetStreamUri req = {0};
     *   req.ProfileToken = profile->token;
     *   req.StreamSetup->Stream = tt__StreamType__RTP_Unicast;
     *   req.StreamSetup->Transport->Protocol = tt__TransportProtocol__RTSP;
     *   soap_call___trt__GetStreamUri(soap, device->xaddr, NULL, &req, &resp);
     *   strncpy(out_url, resp.MediaUri->Uri, url_size - 1);
     */

    (void)username; (void)password;
    fprintf(stderr, "[pg_onvif] get_stream_uri: not implemented (gsoap required)\n");
    return -1;
}

/* ============================================================
 * 便捷接口: 一步完成 → 填充 HalCamConfig
 * ============================================================ */

int pg_onvif_fill_cam_config(const PgOnvifDevice *device,
                             const char *username,
                             const char *password,
                             int profile_idx,
                             HalCamConfig *out_cfg)
{
    if (!device || !out_cfg) return -1;

    /* 1. 获取 profile 列表 */
    PgOnvifProfile profiles[PG_ONVIF_MAX_PROFILES];
    int count = pg_onvif_get_profiles(device, username, password,
                                      profiles, PG_ONVIF_MAX_PROFILES);
    if (count <= 0 || profile_idx >= count) {
        fprintf(stderr, "[pg_onvif] no profiles or index out of range\n");
        return -1;
    }

    /* 2. 获取 RTSP URL */
    char rtsp_url[256] = {0};
    if (pg_onvif_get_stream_uri(device, username, password,
                                &profiles[profile_idx],
                                rtsp_url, sizeof(rtsp_url)) != 0) {
        return -1;
    }

    /* 3. 填充 HalCamConfig — 后续直接传入 hal_cam_open() */
    memset(out_cfg, 0, sizeof(*out_cfg));
    out_cfg->interface = HAL_CAM_IF_RTSP;
    out_cfg->width     = profiles[profile_idx].width;
    out_cfg->height    = profiles[profile_idx].height;
    out_cfg->fps       = profiles[profile_idx].fps;
    strncpy(out_cfg->rtsp.url,      rtsp_url, sizeof(out_cfg->rtsp.url)     - 1);
    strncpy(out_cfg->rtsp.username, username, sizeof(out_cfg->rtsp.username) - 1);
    strncpy(out_cfg->rtsp.password, password, sizeof(out_cfg->rtsp.password) - 1);

    return 0;
}

/* ============================================================
 * PTZ 云台控制
 * ============================================================ */

int pg_onvif_ptz_absolute(const PgOnvifDevice *device,
                          const char *username,
                          const char *password,
                          const char *profile_token,
                          float pan, float tilt, float zoom)
{
    /**
     * ONVIF PTZ Service: AbsoluteMove
     *
     * TODO:
     *   struct _tptz__AbsoluteMove req = {0};
     *   req.ProfileToken = profile_token;
     *   req.Position->PanTilt->x = pan;
     *   req.Position->PanTilt->y = tilt;
     *   req.Position->Zoom->x    = zoom;
     *   soap_call___tptz__AbsoluteMove(soap, ptz_xaddr, NULL, &req, &resp);
     */
    (void)device; (void)username; (void)password; (void)profile_token;
    (void)pan; (void)tilt; (void)zoom;
    fprintf(stderr, "[pg_onvif] ptz_absolute: not implemented\n");
    return -1;
}

int pg_onvif_ptz_stop(const PgOnvifDevice *device,
                      const char *username,
                      const char *password,
                      const char *profile_token)
{
    /**
     * ONVIF PTZ Service: Stop
     *
     * TODO:
     *   struct _tptz__Stop req = {0};
     *   req.ProfileToken = profile_token;
     *   soap_call___tptz__Stop(soap, ptz_xaddr, NULL, &req, &resp);
     */
    (void)device; (void)username; (void)password; (void)profile_token;
    fprintf(stderr, "[pg_onvif] ptz_stop: not implemented\n");
    return -1;
}
