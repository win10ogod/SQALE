#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "thread.h"
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct Channel {
  CRITICAL_SECTION mu;
  CONDITION_VARIABLE cv_send;
  CONDITION_VARIABLE cv_recv;
  void **buf;
  size_t cap, head, tail, count;
} Channel;

Channel *rt_channel_new(size_t capacity) {
  Channel *c = (Channel*)malloc(sizeof(Channel));
  InitializeCriticalSection(&c->mu);
  InitializeConditionVariable(&c->cv_send);
  InitializeConditionVariable(&c->cv_recv);
  c->buf = (void**)malloc(sizeof(void*)*capacity);
  c->cap=capacity; c->head=c->tail=c->count=0; return c;
}
void rt_channel_free(Channel *c) {
  DeleteCriticalSection(&c->mu);
  free(c->buf); free(c);
}
static BOOL wait_ms(CONDITION_VARIABLE *cv, CRITICAL_SECTION *mu, int64_t ms) {
  if (ms<0) { SleepConditionVariableCS(cv, mu, INFINITE); return TRUE; }
  return SleepConditionVariableCS(cv, mu, (DWORD)ms);
}
bool rt_channel_send(Channel *c, void *msg, int64_t timeout_ms) {
  EnterCriticalSection(&c->mu);
  while (c->count==c->cap) { if (!wait_ms(&c->cv_send, &c->mu, timeout_ms)) { LeaveCriticalSection(&c->mu); return false; } }
  c->buf[c->tail] = msg; c->tail = (c->tail+1)%c->cap; c->count++;
  WakeConditionVariable(&c->cv_recv);
  LeaveCriticalSection(&c->mu); return true;
}
void *rt_channel_recv(Channel *c, int64_t timeout_ms) {
  EnterCriticalSection(&c->mu);
  while (c->count==0) { if (!wait_ms(&c->cv_recv, &c->mu, timeout_ms)) { LeaveCriticalSection(&c->mu); return NULL; } }
  void *msg = c->buf[c->head]; c->head=(c->head+1)%c->cap; c->count--; 
  WakeConditionVariable(&c->cv_send);
  LeaveCriticalSection(&c->mu); return msg;
}

#else

#include <pthread.h>
#include <time.h>

typedef struct Channel {
  pthread_mutex_t mu;
  pthread_cond_t cv_send;
  pthread_cond_t cv_recv;
  void **buf;
  size_t cap, head, tail, count;
} Channel;

Channel *rt_channel_new(size_t capacity) {
  Channel *c = (Channel*)malloc(sizeof(Channel));
  pthread_mutex_init(&c->mu, NULL);
  pthread_cond_init(&c->cv_send, NULL);
  pthread_cond_init(&c->cv_recv, NULL);
  c->buf = (void**)malloc(sizeof(void*)*capacity);
  c->cap=capacity; c->head=c->tail=c->count=0; return c;
}
void rt_channel_free(Channel *c) {
  pthread_mutex_destroy(&c->mu);
  pthread_cond_destroy(&c->cv_send);
  pthread_cond_destroy(&c->cv_recv);
  free(c->buf); free(c);
}

static int wait_ms(pthread_cond_t *cv, pthread_mutex_t *mu, int64_t ms) {
  if (ms<0) return pthread_cond_wait(cv, mu);
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ms/1000; ts.tv_nsec += (ms%1000)*1000000;
  if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
  return pthread_cond_timedwait(cv, mu, &ts);
}

bool rt_channel_send(Channel *c, void *msg, int64_t timeout_ms) {
  pthread_mutex_lock(&c->mu);
  while (c->count==c->cap) { if (wait_ms(&c->cv_send, &c->mu, timeout_ms)!=0) { pthread_mutex_unlock(&c->mu); return false; } }
  c->buf[c->tail] = msg; c->tail = (c->tail+1)%c->cap; c->count++;
  pthread_cond_signal(&c->cv_recv);
  pthread_mutex_unlock(&c->mu); return true;
}
void *rt_channel_recv(Channel *c, int64_t timeout_ms) {
  pthread_mutex_lock(&c->mu);
  while (c->count==0) { if (wait_ms(&c->cv_recv, &c->mu, timeout_ms)!=0) { pthread_mutex_unlock(&c->mu); return NULL; } }
  void *msg = c->buf[c->head]; c->head=(c->head+1)%c->cap; c->count--; 
  pthread_cond_signal(&c->cv_send);
  pthread_mutex_unlock(&c->mu); return msg;
}

#endif
