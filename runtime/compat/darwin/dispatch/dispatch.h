/*
 * dispatch/dispatch.h — GCD shim for Android.
 *
 * Upstream uses a small slice of GCD, and only in two host files:
 *
 *   hle_lifecycle.c  dispatch_after (a 1.5 s watchdog), dispatch_source_*
 *                    (a signal handler), dispatch_get_global_queue
 *   hle_audio.c      dispatch_semaphore_* (producer/consumer signalling)
 *
 * This is a platform capability, not a missing declaration — on iOS the
 * scheduler runs this and on Android pthreads do. It is implemented here
 * because upstream includes <dispatch/dispatch.h> by name and rewiring the call
 * sites would mean editing upstream; the implementation is deliberately thin
 * and does not pretend to reproduce GCD's queue semantics.
 *
 * Differences worth knowing:
 *   - "global queues" are not real queues here; work runs on a freshly created
 *     detached thread. There is no ordering guarantee between blocks.
 *   - dispatch_after uses a timer thread, which is accurate enough for a
 *     watchdog timeout.
 *   - dispatch_semaphore is a counting semaphore with the same contract.
 */
#ifndef DM_COMPAT_DISPATCH_H
#define DM_COMPAT_DISPATCH_H

#include <Block.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *dispatch_queue_t;
typedef void *dispatch_block_t;
typedef void *dispatch_source_t;
typedef void *dispatch_semaphore_t;
typedef int64_t dispatch_time_t;

typedef uintptr_t dispatch_source_type_t;

#define DISPATCH_TIME_NOW 0
#define DISPATCH_TIME_FOREVER (~0ull)
#define NSEC_PER_SEC 1000000000ull
#define NSEC_PER_MSEC 1000000ull
#define NSEC_PER_USEC 1000ull
#define USEC_PER_SEC 1000000ull

#define DISPATCH_SOURCE_TYPE_SIGNAL ((dispatch_source_type_t)1)

/* QoS values are shared with pthread/qos.h; re-declared here only if absent. */
#ifndef QOS_CLASS_USER_INTERACTIVE
#define QOS_CLASS_USER_INTERACTIVE 0x21
#define QOS_CLASS_USER_INITIATED 0x19
#define QOS_CLASS_DEFAULT 0x15
#define QOS_CLASS_UTILITY 0x11
#define QOS_CLASS_BACKGROUND 0x09
#define QOS_CLASS_UNSPECIFIED 0x00
#endif

static inline dispatch_time_t dispatch_time(dispatch_time_t base, int64_t delta_ns) {
    if (base == DISPATCH_TIME_NOW) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        base = (dispatch_time_t)ts.tv_sec * (int64_t)NSEC_PER_SEC + ts.tv_nsec;
    }
    return base + delta_ns;
}

/* A handle is enough: this shim runs each block on its own thread. */
static inline dispatch_queue_t dispatch_get_global_queue(long id, unsigned long flags) {
    (void)id; (void)flags;
    return (dispatch_queue_t)&dispatch_get_global_queue;
}

typedef struct {
    int64_t delay_ns;
    dispatch_block_t block;
} dm_dispatch_job;

static void *dm_dispatch_thread(void *arg) {
    dm_dispatch_job *j = (dm_dispatch_job *)arg;
    if (j->delay_ns > 0) {
        struct timespec ts;
        ts.tv_sec = j->delay_ns / (int64_t)NSEC_PER_SEC;
        ts.tv_nsec = j->delay_ns % (int64_t)NSEC_PER_SEC;
        nanosleep(&ts, NULL);
    }
    void (*invoke)(void *) = (void (*)(void *))((const struct Block_literal *)j->block)->invoke;
    if (invoke) invoke(j->block);
    _Block_release(j->block);
    free(j);
    return NULL;
}

static inline void dispatch_async(dispatch_queue_t q, dispatch_block_t block) {
    (void)q;
    dm_dispatch_job *j = (dm_dispatch_job *)malloc(sizeof *j);
    if (!j) return;
    j->delay_ns = 0;
    j->block = _Block_copy(block);
    pthread_t t;
    if (pthread_create(&t, NULL, dm_dispatch_thread, j) != 0) { free(j); return; }
    pthread_detach(t);
}

static inline void dispatch_after(dispatch_time_t when, dispatch_queue_t q,
                                  dispatch_block_t block) {
    (void)q;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = (int64_t)ts.tv_sec * (int64_t)NSEC_PER_SEC + ts.tv_nsec;

    dm_dispatch_job *j = (dm_dispatch_job *)malloc(sizeof *j);
    if (!j) return;
    j->delay_ns = (int64_t)when - now;
    if (j->delay_ns < 0) j->delay_ns = 0;
    j->block = _Block_copy(block);
    pthread_t t;
    if (pthread_create(&t, NULL, dm_dispatch_thread, j) != 0) { free(j); return; }
    pthread_detach(t);
}

/* Signal sources: deliver the signal to a thread that then runs the handler. */
typedef struct {
    int signo;
    dispatch_block_t handler;
    volatile int resumed;
} dm_dispatch_source;

static void *dm_signal_watcher(void *arg) {
    dm_dispatch_source *s = (dm_dispatch_source *)arg;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, s->signo);
    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) != 0) continue;
        if (s->handler) {
            void (*invoke)(void *) = (void (*)(void *))((const struct Block_literal *)s->handler)->invoke;
            if (invoke) invoke(s->handler);
        }
    }
    return NULL;
}

static inline dispatch_source_t dispatch_source_create(dispatch_source_type_t type,
                                                       uintptr_t handle,
                                                       unsigned long mask,
                                                       dispatch_queue_t q) {
    if (type != DISPATCH_SOURCE_TYPE_SIGNAL) return NULL;
    (void)mask; (void)q;
    dm_dispatch_source *s = (dm_dispatch_source *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->signo = (int)handle;
    /* Block the signal process-wide so sigwait can take it. */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, s->signo);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    return (dispatch_source_t)s;
}

static inline void dispatch_source_set_event_handler(dispatch_source_t src,
                                                     dispatch_block_t handler) {
    dm_dispatch_source *s = (dm_dispatch_source *)src;
    if (!s) return;
    s->handler = _Block_copy(handler);
}

static inline void dispatch_source_set_cancel_handler(dispatch_source_t src,
                                                      dispatch_block_t handler) {
    (void)src; (void)handler;
}

static inline void dispatch_resume(dispatch_source_t src) {
    dm_dispatch_source *s = (dm_dispatch_source *)src;
    if (!s || s->resumed) return;
    s->resumed = 1;
    pthread_t t;
    if (pthread_create(&t, NULL, dm_signal_watcher, s) == 0) pthread_detach(t);
}

static inline void dispatch_suspend(dispatch_source_t src) { (void)src; }
static inline void dispatch_cancel(dispatch_source_t src) { (void)src; }

/* Counting semaphore, same contract as GCD's. */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cv;
    long value;
} dm_semaphore;

static inline dispatch_semaphore_t dispatch_semaphore_create(long value) {
    dm_semaphore *s = (dm_semaphore *)malloc(sizeof *s);
    if (!s) return NULL;
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cv, NULL);
    s->value = value;
    return (dispatch_semaphore_t)s;
}

static inline long dispatch_semaphore_signal(dispatch_semaphore_t dsema) {
    dm_semaphore *s = (dm_semaphore *)dsema;
    if (!s) return 0;
    pthread_mutex_lock(&s->lock);
    long prev = s->value++;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->lock);
    return prev;
}

static inline long dispatch_semaphore_wait(dispatch_semaphore_t dsema,
                                           dispatch_time_t timeout) {
    dm_semaphore *s = (dm_semaphore *)dsema;
    if (!s) return -1;

    pthread_mutex_lock(&s->lock);
    if (timeout == DISPATCH_TIME_FOREVER) {
        while (s->value <= 0) pthread_cond_wait(&s->cv, &s->lock);
        s->value--;
        pthread_mutex_unlock(&s->lock);
        return 0;
    }

    /* Timed wait, in absolute CLOCK_REALTIME terms as GCD specifies. */
    struct timespec ts;
    ts.tv_sec = (time_t)(timeout / (int64_t)NSEC_PER_SEC);
    ts.tv_nsec = (long)(timeout % (int64_t)NSEC_PER_SEC);
    int rc = 0;
    while (s->value <= 0 && rc == 0) rc = pthread_cond_timedwait(&s->cv, &s->lock, &ts);
    if (s->value > 0) { s->value--; pthread_mutex_unlock(&s->lock); return 0; }
    pthread_mutex_unlock(&s->lock);
    return 1;   /* timed out */
}

/* dispatch_release on objects: only semaphores are heap-allocated here. */
static inline void dispatch_release(dispatch_semaphore_t dsema) {
    dm_semaphore *s = (dm_semaphore *)dsema;
    if (!s) return;
    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cv);
    free(s);
}

static inline void dispatch_retain(void *o) { (void)o; }

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_DISPATCH_H */
