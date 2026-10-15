#ifndef BLUEMSX_LOG_H
#define BLUEMSX_LOG_H

/* spdlog-based logging for the Pi port.
 *
 * Runtime level selection (highest priority first):
 *   1. BLUEMSX_LOG_LEVEL environment variable
 *   2. settings.logLevel key inside bluemsx.ini ([config] section)
 *   3. default "warn"
 *
 * So logging is silent during a normal boot. Set logLevel to
 * "info"/"debug"/"trace" (or the environment variable) to see the
 * startup messages. Error/critical still always print.
 *
 * Numeric levels used by the LOG_* macros:
 *   0 trace, 1 debug, 2 info, 3 warn, 4 error, 5 critical
 */

#if defined(ROM_TESTER_BUILD)
/* rom_tester is a standalone diagnostic C utility, deliberately not linked
 * against the spdlog/C++ runtime: keep its console output simple. */
#include <stdio.h>
#define LOG_TRACE(...)  (void)0
#define LOG_DEBUG(...)  (void)0
#ifdef DEBUG
#undef LOG_DEBUG
#define LOG_DEBUG(...)  do { printf(__VA_ARGS__); fputc('\n', stdout); } while (0)
#endif
#define LOG_INFO(...)   do { printf(__VA_ARGS__); fputc('\n', stdout); } while (0)
#define LOG_WARN(...)   do { printf(__VA_ARGS__); fputc('\n', stdout); } while (0)
#define LOG_ERROR(...)  do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#else
#ifdef __cplusplus
extern "C" {
#endif
void logInit(void);
void logWrite(int level, const char *fmt, ...);
void logSetLevelByName(const char *name);
#ifdef __cplusplus
}
#endif
#define LOG_TRACE(...)  logWrite(0, __VA_ARGS__)
#define LOG_DEBUG(...)  logWrite(1, __VA_ARGS__)
#define LOG_INFO(...)   logWrite(2, __VA_ARGS__)
#define LOG_WARN(...)   logWrite(3, __VA_ARGS__)
#define LOG_ERROR(...)  logWrite(4, __VA_ARGS__)
#endif

#endif