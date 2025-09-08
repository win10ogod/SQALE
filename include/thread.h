#ifndef THREAD_H
#define THREAD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Cross-platform tiny thread + channel abstraction

typedef struct RtThread RtThread;

typedef void *(*RtThreadFn)(void *arg);

RtThread *rt_thread_spawn(RtThreadFn fn, void *arg);
void rt_thread_join(RtThread *t);

typedef struct Channel Channel;

Channel *rt_channel_new(size_t capacity);
void rt_channel_free(Channel *c);
bool rt_channel_send(Channel *c, void *msg, int64_t timeout_ms);
void *rt_channel_recv(Channel *c, int64_t timeout_ms);

#endif // THREAD_H

