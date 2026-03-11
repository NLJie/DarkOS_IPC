/**********************************************************************************
 * Copyright (c) PanGu Tech. Co., Ltd. 2026-2026+3. All rights reserved.
 * Desc:     事件总线实现
 *
 * 架构:
 *   1. 本地分发 (进程内)
 *      - SYNC:  在发布者线程直接调用回调
 *      - ASYNC: 写入订阅者专属环形队列, 由其工作线程消费
 *
 *   2. IPC 分发 (进程间, 可选)
 *      - Server: 监听 UNIX Domain Socket, 接受客户端连接;
 *                本地发布时广播给所有客户端;
 *                收到客户端事件时本地分发并转发给其他客户端.
 *      - Client: 连接服务端;
 *                本地发布时发给服务端;
 *                收到服务端事件时本地分发.
 *
 *   IPC 传输层使用 SOCK_SEQPACKET (保证消息边界, 无需分包).
 *   每条消息 = 一个完整 PgEvent 结构体 (固定大小).
 *
 * FileName: pg_event_bus.c
 * Author:   NLJie
 * Date:     2026-03-11
 **********************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include "pg_event_bus.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <fnmatch.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define DEFAULT_QUEUE_DEPTH     64
#define DEFAULT_IPC_MAX_CLIENTS  8
#define IPC_INVALID_FD          (-1)

/* ============================================================
 * 环形队列 (有锁, 多生产者单消费者)
 * ============================================================ */

typedef struct {
    PgEvent         *buf;
    uint32_t         capacity;
    uint32_t         head;     /* 消费者读位置 */
    uint32_t         tail;     /* 生产者写位置 */
    uint32_t         count;
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;
} RingBuf;

static int rb_init(RingBuf *rb, uint32_t capacity)
{
    rb->buf = malloc(sizeof(PgEvent) * capacity);
    if (!rb->buf) return -1;
    rb->capacity = capacity;
    rb->head = rb->tail = rb->count = 0;
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    return 0;
}

static void rb_deinit(RingBuf *rb)
{
    free(rb->buf);
    rb->buf = NULL;
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
}

/** 非阻塞入队. 队满返回 -1 (丢弃). */
static int rb_push(RingBuf *rb, const PgEvent *ev)
{
    pthread_mutex_lock(&rb->mutex);
    if (rb->count >= rb->capacity) {
        pthread_mutex_unlock(&rb->mutex);
        return -1;
    }
    rb->buf[rb->tail] = *ev;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

/**
 * 阻塞出队. 最多等待 timeout_ms 毫秒.
 * 返回 0 成功, -1 超时或 *stop 为非 0.
 */
static int rb_pop(RingBuf *rb, PgEvent *out, volatile int *stop, int timeout_ms)
{
    pthread_mutex_lock(&rb->mutex);
    while (rb->count == 0 && !*stop) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += (long)timeout_ms * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec  += ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;
        }
        pthread_cond_timedwait(&rb->not_empty, &rb->mutex, &ts);
    }
    if (rb->count == 0) {
        pthread_mutex_unlock(&rb->mutex);
        return -1;
    }
    *out  = rb->buf[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

/* ============================================================
 * 订阅者
 * ============================================================ */

typedef struct {
    bool             active;
    char             pattern[PG_EB_TOPIC_MAX_LEN];
    PgEbDispatchMode mode;
    PgEbHandler      handler;
    void            *user_data;
    /* ASYNC 专用 */
    RingBuf          queue;
    pthread_t        thread;
    volatile int     stop_flag;
} Subscriber;

/* ============================================================
 * IPC 状态
 * ============================================================ */

#define IPC_MAX_CLIENTS_HARD 16

typedef struct {
    bool         enabled;
    bool         is_server;

    /* server 模式: 监听 fd */
    int          listen_fd;
    /* server 模式: 客户端连接池 */
    int          client_fds[IPC_MAX_CLIENTS_HARD];
    int          client_count;
    int          max_clients;

    /* client 模式: 连接到 server 的 fd */
    int          conn_fd;

    pthread_t    thread;
    volatile int stop_flag;

    pthread_mutex_t send_mutex; /* 保护 ipc 写操作 */
} IpcState;

/* ============================================================
 * 全局总线上下文
 * ============================================================ */

typedef struct {
    bool            initialized;
    uint32_t        queue_depth;
    Subscriber      subs[PG_EB_MAX_SUBSCRIBERS];
    pthread_mutex_t subs_mutex;
    IpcState        ipc;
} EbCtx;

static EbCtx g_eb = {0};

/* ============================================================
 * 工具函数
 * ============================================================ */

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

static bool topic_matches(const char *pattern, const char *topic)
{
    if (strcmp(pattern, "#") == 0) return true;
    return fnmatch(pattern, topic, 0) == 0;
}

/* ============================================================
 * ASYNC 工作线程
 * ============================================================ */

static void *async_worker(void *arg)
{
    Subscriber *sub = (Subscriber *)arg;
    PgEvent ev;
    while (!sub->stop_flag) {
        if (rb_pop(&sub->queue, &ev, &sub->stop_flag, 100) == 0) {
            sub->handler(&ev, sub->user_data);
        }
    }
    return NULL;
}

/* ============================================================
 * 本地分发 (进程内, 不含 IPC 转发)
 * ============================================================ */

static void local_dispatch(const PgEvent *ev)
{
    /* 快照匹配订阅者 (持锁期间仅读, 回调在锁外执行) */
    Subscriber *matched[PG_EB_MAX_SUBSCRIBERS];
    int count = 0;

    pthread_mutex_lock(&g_eb.subs_mutex);
    for (int i = 0; i < PG_EB_MAX_SUBSCRIBERS; i++) {
        if (g_eb.subs[i].active &&
            topic_matches(g_eb.subs[i].pattern, ev->topic)) {
            matched[count++] = &g_eb.subs[i];
        }
    }
    pthread_mutex_unlock(&g_eb.subs_mutex);

    for (int i = 0; i < count; i++) {
        Subscriber *sub = matched[i];
        if (!sub->active) continue; /* 分发期间被取消订阅则跳过 */

        if (sub->mode == PG_EB_DISPATCH_SYNC) {
            sub->handler(ev, sub->user_data);
        } else {
            if (rb_push(&sub->queue, ev) != 0) {
                fprintf(stderr, "[pg_eb] WARN: async queue full, topic=%s dropped\n",
                        ev->topic);
            }
        }
    }
}

/* ============================================================
 * IPC — 发送一条事件到指定 fd
 * ============================================================ */

static int ipc_send_event(int fd, const PgEvent *ev)
{
    ssize_t sent = send(fd, ev, sizeof(PgEvent), MSG_NOSIGNAL);
    if (sent != (ssize_t)sizeof(PgEvent)) {
        return -1;
    }
    return 0;
}

/* ============================================================
 * IPC Server 线程
 * 使用 select() 多路复用:
 *   - 监听 listen_fd 的新连接
 *   - 监听每个 client_fd 的消息
 * ============================================================ */

static void ipc_server_remove_client(IpcState *ipc, int fd)
{
    close(fd);
    for (int i = 0; i < ipc->client_count; i++) {
        if (ipc->client_fds[i] == fd) {
            ipc->client_fds[i] = ipc->client_fds[--ipc->client_count];
            break;
        }
    }
}

static void *ipc_server_thread(void *arg)
{
    IpcState *ipc = (IpcState *)arg;

    while (!ipc->stop_flag) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ipc->listen_fd, &rfds);
        int maxfd = ipc->listen_fd;

        pthread_mutex_lock(&ipc->send_mutex);
        for (int i = 0; i < ipc->client_count; i++) {
            FD_SET(ipc->client_fds[i], &rfds);
            if (ipc->client_fds[i] > maxfd) maxfd = ipc->client_fds[i];
        }
        pthread_mutex_unlock(&ipc->send_mutex);

        struct timeval tv = {0, 100000}; /* 100ms 超时, 检查 stop_flag */
        int ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        /* 新客户端连接 */
        if (FD_ISSET(ipc->listen_fd, &rfds)) {
            int cfd = accept(ipc->listen_fd, NULL, NULL);
            if (cfd >= 0) {
                pthread_mutex_lock(&ipc->send_mutex);
                if (ipc->client_count < ipc->max_clients) {
                    ipc->client_fds[ipc->client_count++] = cfd;
                } else {
                    close(cfd); /* 超出上限, 拒绝连接 */
                }
                pthread_mutex_unlock(&ipc->send_mutex);
            }
        }

        /* 收到客户端消息 */
        pthread_mutex_lock(&ipc->send_mutex);
        int snapshot_fds[IPC_MAX_CLIENTS_HARD];
        int snapshot_count = ipc->client_count;
        memcpy(snapshot_fds, ipc->client_fds, sizeof(int) * snapshot_count);
        pthread_mutex_unlock(&ipc->send_mutex);

        for (int i = 0; i < snapshot_count; i++) {
            int cfd = snapshot_fds[i];
            if (!FD_ISSET(cfd, &rfds)) continue;

            PgEvent ev;
            ssize_t n = recv(cfd, &ev, sizeof(PgEvent), 0);
            if (n != (ssize_t)sizeof(PgEvent)) {
                /* 客户端断开 */
                pthread_mutex_lock(&ipc->send_mutex);
                ipc_server_remove_client(ipc, cfd);
                pthread_mutex_unlock(&ipc->send_mutex);
                continue;
            }

            /* 本地分发 */
            local_dispatch(&ev);

            /* 转发给其他客户端 (不含发送方) */
            pthread_mutex_lock(&ipc->send_mutex);
            for (int j = 0; j < ipc->client_count; j++) {
                if (ipc->client_fds[j] != cfd) {
                    ipc_send_event(ipc->client_fds[j], &ev);
                }
            }
            pthread_mutex_unlock(&ipc->send_mutex);
        }
    }
    return NULL;
}

/* ============================================================
 * IPC Client 线程
 * 阻塞接收来自 server 的事件, 本地分发.
 * ============================================================ */

static void *ipc_client_thread(void *arg)
{
    IpcState *ipc = (IpcState *)arg;

    while (!ipc->stop_flag) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ipc->conn_fd, &rfds);

        struct timeval tv = {0, 100000}; /* 100ms */
        int ret = select(ipc->conn_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        PgEvent ev;
        ssize_t n = recv(ipc->conn_fd, &ev, sizeof(PgEvent), 0);
        if (n != (ssize_t)sizeof(PgEvent)) {
            fprintf(stderr, "[pg_eb] IPC client: server disconnected\n");
            break;
        }
        local_dispatch(&ev);
    }
    return NULL;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

int pg_eb_init(const PgEbConfig *cfg)
{
    if (g_eb.initialized) return -1;

    memset(&g_eb, 0, sizeof(g_eb));

    g_eb.queue_depth = (cfg && cfg->queue_depth) ? cfg->queue_depth : DEFAULT_QUEUE_DEPTH;

    pthread_mutex_init(&g_eb.subs_mutex, NULL);

    /* IPC 初始化 */
    IpcState *ipc = &g_eb.ipc;
    ipc->listen_fd = IPC_INVALID_FD;
    ipc->conn_fd   = IPC_INVALID_FD;
    for (int i = 0; i < IPC_MAX_CLIENTS_HARD; i++) {
        ipc->client_fds[i] = IPC_INVALID_FD;
    }
    pthread_mutex_init(&ipc->send_mutex, NULL);

    if (cfg && cfg->enable_ipc) {
        ipc->enabled    = true;
        ipc->is_server  = cfg->ipc_is_server;
        ipc->max_clients = (cfg->ipc_max_clients > 0)
                           ? (int)cfg->ipc_max_clients
                           : DEFAULT_IPC_MAX_CLIENTS;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, cfg->ipc_socket_path, sizeof(addr.sun_path) - 1);

        if (ipc->is_server) {
            /* 创建监听 socket */
            ipc->listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if (ipc->listen_fd < 0) goto ipc_fail;

            unlink(cfg->ipc_socket_path); /* 清除残留 */
            if (bind(ipc->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) goto ipc_fail;
            if (listen(ipc->listen_fd, ipc->max_clients) < 0) goto ipc_fail;

            pthread_create(&ipc->thread, NULL, ipc_server_thread, ipc);

        } else {
            /* 连接到 server */
            ipc->conn_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if (ipc->conn_fd < 0) goto ipc_fail;

            if (connect(ipc->conn_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                fprintf(stderr, "[pg_eb] IPC client: connect failed: %s\n", strerror(errno));
                goto ipc_fail;
            }
            pthread_create(&ipc->thread, NULL, ipc_client_thread, ipc);
        }
    }

    g_eb.initialized = true;
    return 0;

ipc_fail:
    fprintf(stderr, "[pg_eb] IPC init failed: %s\n", strerror(errno));
    if (ipc->listen_fd != IPC_INVALID_FD) { close(ipc->listen_fd); ipc->listen_fd = IPC_INVALID_FD; }
    if (ipc->conn_fd   != IPC_INVALID_FD) { close(ipc->conn_fd);   ipc->conn_fd   = IPC_INVALID_FD; }
    ipc->enabled = false;
    /* 降级为纯本地模式继续运行 */
    g_eb.initialized = true;
    return 0;
}

void pg_eb_deinit(void)
{
    if (!g_eb.initialized) return;

    /* 停止 IPC 线程 */
    IpcState *ipc = &g_eb.ipc;
    if (ipc->enabled) {
        ipc->stop_flag = 1;
        pthread_join(ipc->thread, NULL);

        if (ipc->listen_fd != IPC_INVALID_FD) close(ipc->listen_fd);
        if (ipc->conn_fd   != IPC_INVALID_FD) close(ipc->conn_fd);
        for (int i = 0; i < ipc->client_count; i++) {
            if (ipc->client_fds[i] != IPC_INVALID_FD) close(ipc->client_fds[i]);
        }
    }
    pthread_mutex_destroy(&ipc->send_mutex);

    /* 停止所有 ASYNC 工作线程并释放队列 */
    pthread_mutex_lock(&g_eb.subs_mutex);
    for (int i = 0; i < PG_EB_MAX_SUBSCRIBERS; i++) {
        if (g_eb.subs[i].active && g_eb.subs[i].mode == PG_EB_DISPATCH_ASYNC) {
            g_eb.subs[i].stop_flag = 1;
            pthread_cond_signal(&g_eb.subs[i].queue.not_empty);
        }
    }
    pthread_mutex_unlock(&g_eb.subs_mutex);

    for (int i = 0; i < PG_EB_MAX_SUBSCRIBERS; i++) {
        if (g_eb.subs[i].active && g_eb.subs[i].mode == PG_EB_DISPATCH_ASYNC) {
            pthread_join(g_eb.subs[i].thread, NULL);
            rb_deinit(&g_eb.subs[i].queue);
        }
        g_eb.subs[i].active = false;
    }

    pthread_mutex_destroy(&g_eb.subs_mutex);
    memset(&g_eb, 0, sizeof(g_eb));
}

int pg_eb_subscribe(const char *topic,
                    PgEbDispatchMode mode,
                    PgEbHandler handler,
                    void *user_data)
{
    if (!g_eb.initialized || !topic || !handler) return -1;

    pthread_mutex_lock(&g_eb.subs_mutex);

    int slot = -1;
    for (int i = 0; i < PG_EB_MAX_SUBSCRIBERS; i++) {
        if (!g_eb.subs[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&g_eb.subs_mutex);
        fprintf(stderr, "[pg_eb] subscribe failed: max subscribers (%d) reached\n",
                PG_EB_MAX_SUBSCRIBERS);
        return -1;
    }

    Subscriber *sub = &g_eb.subs[slot];
    memset(sub, 0, sizeof(*sub));
    strncpy(sub->pattern, topic, PG_EB_TOPIC_MAX_LEN - 1);
    sub->mode      = mode;
    sub->handler   = handler;
    sub->user_data = user_data;

    if (mode == PG_EB_DISPATCH_ASYNC) {
        if (rb_init(&sub->queue, g_eb.queue_depth) != 0) {
            pthread_mutex_unlock(&g_eb.subs_mutex);
            return -1;
        }
        sub->stop_flag = 0;
        pthread_create(&sub->thread, NULL, async_worker, sub);
    }

    sub->active = true;
    pthread_mutex_unlock(&g_eb.subs_mutex);
    return slot;
}

void pg_eb_unsubscribe(int sub_id)
{
    if (!g_eb.initialized) return;
    if (sub_id < 0 || sub_id >= PG_EB_MAX_SUBSCRIBERS) return;

    pthread_mutex_lock(&g_eb.subs_mutex);
    Subscriber *sub = &g_eb.subs[sub_id];
    if (!sub->active) {
        pthread_mutex_unlock(&g_eb.subs_mutex);
        return;
    }
    sub->active = false; /* 标记无效, local_dispatch 会跳过 */
    pthread_mutex_unlock(&g_eb.subs_mutex);

    if (sub->mode == PG_EB_DISPATCH_ASYNC) {
        sub->stop_flag = 1;
        pthread_cond_signal(&sub->queue.not_empty);
        pthread_join(sub->thread, NULL);
        rb_deinit(&sub->queue);
    }
    memset(sub, 0, sizeof(*sub));
}

int pg_eb_publish(const char *topic, const void *data, size_t size)
{
    if (!g_eb.initialized || !topic) return -1;
    if (size > PG_EB_DATA_INLINE_MAX) {
        fprintf(stderr, "[pg_eb] publish: size %zu > max %d, topic=%s\n",
                size, PG_EB_DATA_INLINE_MAX, topic);
        return -1;
    }

    PgEvent ev;
    memset(&ev, 0, sizeof(ev));
    strncpy(ev.topic, topic, PG_EB_TOPIC_MAX_LEN - 1);
    ev.timestamp_us = now_us();
    ev.size = (uint32_t)size;
    if (data && size > 0) {
        memcpy(ev.data, data, size);
    }

    /* 1. 本地分发 */
    local_dispatch(&ev);

    /* 2. IPC 转发 */
    IpcState *ipc = &g_eb.ipc;
    if (ipc->enabled && !ipc->stop_flag) {
        pthread_mutex_lock(&ipc->send_mutex);
        if (ipc->is_server) {
            /* 广播给所有客户端 */
            for (int i = 0; i < ipc->client_count; i++) {
                ipc_send_event(ipc->client_fds[i], &ev);
            }
        } else if (ipc->conn_fd != IPC_INVALID_FD) {
            /* 发给服务端 */
            ipc_send_event(ipc->conn_fd, &ev);
        }
        pthread_mutex_unlock(&ipc->send_mutex);
    }

    return 0;
}
