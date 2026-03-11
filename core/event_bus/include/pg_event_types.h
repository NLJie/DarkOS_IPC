/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     系统预定义事件主题常量
 *           各组件应使用此处定义的常量, 避免魔法字符串.
 * FileName: pg_event_types.h
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#ifndef PG_EVENT_TYPES_H
#define PG_EVENT_TYPES_H

#include <stdint.h>

/* ============================================================
 * 主题常量
 * 命名规范: "<模块>/<事件名>"
 * ============================================================ */

/* --- 摄像头 --- */
/** payload: PgEvtCameraFrame  — 新帧就绪通知 (只携带句柄, 不含像素数据) */
#define PG_EVT_CAMERA_FRAME     "camera/frame"
/** payload: PgEvtCameraError  — 摄像头错误 */
#define PG_EVT_CAMERA_ERROR     "camera/error"
/** payload: 无                — 摄像头已打开 */
#define PG_EVT_CAMERA_OPENED    "camera/opened"
/** payload: 无                — 摄像头已关闭 */
#define PG_EVT_CAMERA_CLOSED    "camera/closed"

/* --- AI 推理 --- */
/** payload: PgEvtAiDetection  — 目标检测结果 */
#define PG_EVT_AI_DETECTION     "ai/detection"
/** payload: 无                — 推理模块就绪 */
#define PG_EVT_AI_READY         "ai/ready"

/* --- 告警 --- */
/** payload: PgEvtAlarm        — 告警触发 */
#define PG_EVT_ALARM_TRIGGER    "alarm/trigger"
/** payload: PgEvtAlarm        — 告警恢复 */
#define PG_EVT_ALARM_CLEAR      "alarm/clear"

/* --- 存储 --- */
/** payload: 无                — 存储空间不足 */
#define PG_EVT_STORAGE_LOW      "storage/low"
/** payload: 无                — 存储已满, 停止录制 */
#define PG_EVT_STORAGE_FULL     "storage/full"

/* --- 网络 --- */
/** payload: 无                — 网络已连接 */
#define PG_EVT_NET_CONNECTED    "network/connected"
/** payload: 无                — 网络已断开 */
#define PG_EVT_NET_DISCONNECTED "network/disconnected"

/* --- 系统 --- */
/** payload: 无                — 请求优雅关闭, 各组件收到后执行 stop */
#define PG_EVT_SYS_SHUTDOWN     "system/shutdown"
/** payload: 无                — OTA 升级触发 */
#define PG_EVT_SYS_OTA_START    "system/ota_start"

/* ============================================================
 * 各事件 payload 结构体定义
 * ============================================================ */

/**
 * PG_EVT_CAMERA_FRAME payload.
 * 注意: 不含像素数据! 只含引用句柄.
 * 订阅者通过 hal_cam_read_frame() 或共享内存访问实际帧.
 */
typedef struct {
    void    *cam_handle;     /**< hal_cam_open() 返回的摄像头句柄 */
    int64_t  timestamp_us;   /**< 帧时间戳 (微秒)                 */
    int      width;
    int      height;
    int      frame_seq;      /**< 帧序号, 单调递增               */
} PgEvtCameraFrame;

/** PG_EVT_CAMERA_ERROR payload */
typedef struct {
    int  error_code;
    char message[64];
} PgEvtCameraError;

/** PG_EVT_AI_DETECTION payload — 单帧检测结果 */
#define PG_AI_MAX_OBJECTS 32

typedef struct {
    float x, y, w, h;   /**< 归一化 [0,1] 的边界框   */
    float confidence;
    int   class_id;
    char  class_name[32];
} PgAiObject;

typedef struct {
    int64_t    frame_timestamp_us;
    int        object_count;
    PgAiObject objects[PG_AI_MAX_OBJECTS];
} PgEvtAiDetection;

/** PG_EVT_ALARM_TRIGGER / PG_EVT_ALARM_CLEAR payload */
typedef struct {
    int  alarm_id;
    int  alarm_type;     /**< 业务层自定义枚举 */
    char description[64];
    int64_t triggered_at_us;
} PgEvtAlarm;

#endif /* PG_EVENT_TYPES_H */
