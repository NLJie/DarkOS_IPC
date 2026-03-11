/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     事件总线公共接口
 *           支持:
 *             - 进程内多线程安全分发 (SYNC / ASYNC 两种模式)
 *             - 进程间通信 (UNIX Domain Socket, 可选)
 *             - 主题通配符匹配 ("camera/*", "#" 匹配全部)
 * FileName: pg_event_bus.h
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#ifndef PG_EVENT_BUS_H
#define PG_EVENT_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量
 * ============================================================ */

/** 主题字符串最大长度 (含 '\0') */
#define PG_EB_TOPIC_MAX_LEN     64

/**
 * 事件 inline payload 最大字节数.
 * 对于视频帧等大数据, 应只在 payload 中传递句柄/引用, 而非原始数据.
 */
#define PG_EB_DATA_INLINE_MAX   256

/** 最大订阅者数量 */
#define PG_EB_MAX_SUBSCRIBERS   32

/* ============================================================
 * 事件结构
 * ============================================================ */

typedef struct {
    char     topic[PG_EB_TOPIC_MAX_LEN]; /**< 路由主题, 如 "camera/frame"       */
    uint64_t timestamp_us;               /**< 发布时刻 (CLOCK_MONOTONIC 微秒)    */
    uint32_t size;                       /**< payload 实际字节数                 */
    uint8_t  data[PG_EB_DATA_INLINE_MAX];/**< inline payload                     */
} PgEvent;

/* ============================================================
 * 订阅回调
 * ============================================================ */

/**
 * @brief 事件回调函数类型
 * @note  ASYNC 模式下在订阅者自己的工作线程中调用;
 *        SYNC  模式下直接在发布者线程中调用 (阻塞发布者, 慎用).
 */
typedef void (*PgEbHandler)(const PgEvent *event, void *user_data);

/* ============================================================
 * 分发模式
 * ============================================================ */

typedef enum {
    /**
     * 异步模式 (推荐): 事件写入订阅者专属环形队列,
     * 由其独立工作线程消费. 发布者不阻塞.
     * 队列满时丢弃最新事件并记录警告.
     */
    PG_EB_DISPATCH_ASYNC = 0,

    /**
     * 同步模式: 在发布者线程直接调用回调后返回.
     * 适合需要低延迟但回调必须极短的场景.
     * 注意: 回调内不可再调用 pg_eb_publish() (会死锁).
     */
    PG_EB_DISPATCH_SYNC = 1,
} PgEbDispatchMode;

/* ============================================================
 * 初始化配置
 * ============================================================ */

typedef struct {
    /**
     * 每个 ASYNC 订阅者的环形队列深度 (事件个数).
     * 0 = 使用默认值 64.
     */
    uint32_t queue_depth;

    /** 是否启用 IPC (跨进程通信). 默认 false. */
    bool enable_ipc;

    /**
     * UNIX Domain Socket 路径, 如 "/tmp/pg_eventbus.sock".
     * enable_ipc = true 时必须填写.
     */
    char ipc_socket_path[108]; /* 108 = UNIX_PATH_MAX */

    /**
     * true  = 本进程作为 IPC 路由服务端 (通常是主进程).
     * false = 本进程作为 IPC 客户端 (子进程/外部进程).
     */
    bool ipc_is_server;

    /**
     * Server 模式下最大同时连接的客户端数.
     * 0 = 使用默认值 8.
     */
    uint32_t ipc_max_clients;
} PgEbConfig;

/* ============================================================
 * 核心 API
 * ============================================================ */

/**
 * @brief  初始化事件总线
 * @param  cfg  配置指针; NULL = 使用默认配置 (仅本地, 不启用 IPC)
 * @return 0 成功, 负值失败
 */
int pg_eb_init(const PgEbConfig *cfg);

/**
 * @brief  关闭事件总线, 等待所有工作线程退出, 释放所有资源.
 *         调用后需重新 pg_eb_init() 才能继续使用.
 */
void pg_eb_deinit(void);

/**
 * @brief  订阅指定主题的事件
 *
 * 主题匹配规则:
 *   - 精确匹配: "camera/frame"
 *   - 单级通配: "camera/*"   匹配 "camera/frame", "camera/error" 等
 *   - 全局通配: "#"          匹配所有主题
 *
 * @param  topic      主题模式字符串
 * @param  mode       分发模式
 * @param  handler    回调函数
 * @param  user_data  透传给回调的用户指针
 * @return subscription_id (>= 0), 失败返回负值
 */
int pg_eb_subscribe(const char *topic,
                    PgEbDispatchMode mode,
                    PgEbHandler handler,
                    void *user_data);

/**
 * @brief  取消订阅
 * @param  sub_id  pg_eb_subscribe() 返回的 ID
 * @note   建议仅在组件 deinit 阶段调用, 不要在回调中调用.
 */
void pg_eb_unsubscribe(int sub_id);

/**
 * @brief  发布事件
 *
 * 发布流程:
 *   1. 本地分发: 遍历所有匹配订阅者, SYNC 直接调用 / ASYNC 入队
 *   2. 若 IPC 已启用: 序列化后通过 socket 发送给其他进程
 *
 * @param  topic  主题字符串 (不含通配符)
 * @param  data   payload 指针 (可为 NULL, size = 0)
 * @param  size   payload 字节数 (最大 PG_EB_DATA_INLINE_MAX)
 * @return 0 成功, 负值失败 (size 超限 / 总线未初始化)
 */
int pg_eb_publish(const char *topic, const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* PG_EVENT_BUS_H */
