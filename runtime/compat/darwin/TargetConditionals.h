/*
 * TargetConditionals.h — Android shim for Apple's platform detection header.
 *
 * Upstream selects between Apple platform implementations with
 * "#if TARGET_OS_IPHONE" and "#if TARGET_OS_OSX". On Android every Apple
 * platform has to be false, which makes those conditionals pick the portable
 * path — the same code that runs when neither Apple platform is targeted. The
 * Apple-only bodies they guard (iOS code embedding, iCloud sync, the macOS
 * sampler) are replaced by this project's own platform layer, not emulated.
 *
 * Defining them as literal 0 is what upstream's "#if" form needs; it also keeps
 * the values usable in expressions.
 */
#ifndef DM_COMPAT_TARGETCONDITIONALS_H
#define DM_COMPAT_TARGETCONDITIONALS_H

#define TARGET_OS_MAC            0
#define TARGET_OS_OSX            0
#define TARGET_OS_IPHONE         0
#define TARGET_OS_IOS            0
#define TARGET_OS_TV             0
#define TARGET_OS_WATCH          0
#define TARGET_OS_VISION         0
#define TARGET_OS_SIMULATOR      0
#define TARGET_OS_EMBEDDED       0
#define TARGET_OS_DRIVERKIT      0

/* Android is not an Apple platform: state it explicitly for code that tests it. */
#define DM_TARGET_ANDROID        1

#endif /* DM_COMPAT_TARGETCONDITIONALS_H */
