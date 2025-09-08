#include "thread.h"
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct RtThread { HANDLE h; } RtThread;

static DWORD WINAPI rt_thread_tramp(LPVOID p) {
  RtThreadFn fn = ((RtThreadFn*)p)[0];
  void *arg = ((void**)p)[1];
  free(p);
  fn(arg);
  return 0;
}

RtThread *rt_thread_spawn(RtThreadFn fn, void *arg) {
  void **pack = (void**)malloc(sizeof(void*)*2);
  pack[0] = (void*)fn; pack[1] = arg;
  HANDLE h = CreateThread(NULL, 0, rt_thread_tramp, pack, 0, NULL);
  if (!h) { free(pack); return NULL; }
  RtThread *t = (RtThread*)malloc(sizeof(RtThread)); t->h = h; return t;
}
void rt_thread_join(RtThread *t) { WaitForSingleObject(t->h, INFINITE); CloseHandle(t->h); free(t); }

#else

#include <pthread.h>

typedef struct RtThread { pthread_t th; } RtThread;

typedef struct { RtThreadFn fn; void *arg; } TrampArg;

static void *tramp(void *p){ TrampArg *ta=(TrampArg*)p; RtThreadFn f=ta->fn; void* a=ta->arg; free(ta); f(a); return NULL; }

RtThread *rt_thread_spawn(RtThreadFn fn, void *arg) {
  RtThread *t = (RtThread*)malloc(sizeof(RtThread));
  TrampArg *ta = (TrampArg*)malloc(sizeof(TrampArg)); ta->fn=fn; ta->arg=arg;
  if (pthread_create(&t->th, NULL, tramp, ta)!=0) { free(ta); free(t); return NULL; }
  return t;
}
void rt_thread_join(RtThread *t) { pthread_join(t->th, NULL); free(t); }

#endif

