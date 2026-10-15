/*
 * pthread/qos.h — Android/POSIX shim for Darwin QoS classes.
 *
 * Upstream uses QoS only to tag threads (interactive vs background) and to read
 * the current class back for logging. Android has no QoS API; setpriority via
 * nice levels is the closest equivalent, so classes map to nice values and
 * "get" reports what we set. This keeps upstream call sites compiling and keeps
 * thread priority intent preserved, without pretending to reproduce Darwin
 * scheduler semantics.
 */
#ifndef DM_COMPAT_PTHREAD_QOS_H
#define DM_COMPAT_PTHREAD_QOS_H

#include <pthread.h>
#include <sys/resource.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QOS_CLASS_USER_INTERACTIVE = 0x21,
    QOS_CLASS_USER_INITIATED   = 0x19,
    QOS_CLASS_DEFAULT          = 0x15,
    QOS_CLASS_UTILITY          = 0x11,
    QOS_CLASS_BACKGROUND       = 0x09,
    QOS_CLASS_UNSPECIFIED      = 0x00,
} qos_class_t;

/*
 * Upstream includes <pthread/qos.h> wherever it needs Darwin pthread
 * extensions, so pulling them in here keeps every one of those call sites
 * compiling without touching upstream source.
 */
#include <pthread_compat.h>

static inline int dm_qos_to_nice(qos_class_t q) {
    switch (q) {
        case QOS_CLASS_USER_INTERACTIVE: return -16;
        case QOS_CLASS_USER_INITIATED:   return -8;
        case QOS_CLASS_DEFAULT:          return 0;
        case QOS_CLASS_UTILITY:          return 8;
        case QOS_CLASS_BACKGROUND:       return 16;
        default:                         return 0;
    }
}

static inline qos_class_t dm_nice_to_qos(int nice) {
    if (nice <= -12) return QOS_CLASS_USER_INTERACTIVE;
    if (nice <= -4)  return QOS_CLASS_USER_INITIATED;
    if (nice < 4)    return QOS_CLASS_DEFAULT;
    if (nice < 12)   return QOS_CLASS_UTILITY;
    return QOS_CLASS_BACKGROUND;
}

static inline int pthread_set_qos_class_self_np(qos_class_t q, int rel) {
    (void)rel;
    return setpriority(PRIO_PROCESS, 0, dm_qos_to_nice(q));
}

static inline int pthread_get_qos_class_np(pthread_t t, qos_class_t *q, int *rel) {
    (void)t;
    if (rel) *rel = 0;
    if (q) {
        int nice = getpriority(PRIO_PROCESS, 0);
        *q = dm_nice_to_qos(nice > 19 ? 19 : (nice < -20 ? -20 : nice));
    }
    return 0;
}

static inline int pthread_attr_set_qos_class_np(pthread_attr_t *a, qos_class_t q, int rel) {
    (void)a; (void)q; (void)rel;
    return 0;
}

static inline int pthread_attr_get_qos_class_np(pthread_attr_t *a, qos_class_t *q, int *rel) {
    (void)a;
    if (rel) *rel = 0;
    if (q) *q = QOS_CLASS_UNSPECIFIED;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_PTHREAD_QOS_H */
