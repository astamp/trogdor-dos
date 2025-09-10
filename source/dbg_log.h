/* Tiny shared logger with compile-time disable (C only).
 * Set DBG_LOG_ENABLE to 0 to compile out all logging calls.
 */
#ifndef DBG_LOG_H
#define DBG_LOG_H

#ifndef DBG_LOG_ENABLE
#define DBG_LOG_ENABLE 1
#endif

/* Verbose logging toggle: 0 to silence dbg_vlog_printf, 1 to enable */
#ifndef DBG_LOG_VERBOSE
#define DBG_LOG_VERBOSE 1
#endif

#if DBG_LOG_ENABLE
void dbg_log_open(const char *path);                 /* path==NULL -> "app_dbg.txt" */
void dbg_log_set_enabled(int on);                    /* runtime enable/disable */
void dbg_log_printf(const char *section, const char *fmt, ...);
void dbg_log_flush(void);
void dbg_log_close(void);
/* Verbose logger: compile-time selectable */
#if DBG_LOG_VERBOSE
#define dbg_vlog_printf dbg_log_printf
#else
#define dbg_vlog_printf(...) ((void)0)
#endif
#else
#define dbg_log_open(path)               ((void)0)
#define dbg_log_set_enabled(on)          ((void)0)
#define dbg_log_printf(section, ...)     ((void)0)
#define dbg_log_flush()                  ((void)0)
#define dbg_log_close()                  ((void)0)
/* When logging disabled, verbose also compiles out */
#define dbg_vlog_printf(...)             ((void)0)
#endif

#endif /* DBG_LOG_H */
