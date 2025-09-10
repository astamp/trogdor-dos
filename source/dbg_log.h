/* Tiny shared logger with compile-time disable (C only).
 * Set DBG_LOG_ENABLE to 0 to compile out all logging calls.
 */
#ifndef DBG_LOG_H
#define DBG_LOG_H

#ifndef DBG_LOG_ENABLE
#define DBG_LOG_ENABLE 1
#endif

#if DBG_LOG_ENABLE
void dbg_log_open(const char *path);                 /* path==NULL -> "app_dbg.txt" */
void dbg_log_set_enabled(int on);                    /* runtime enable/disable */
void dbg_log_printf(const char *section, const char *fmt, ...);
void dbg_log_flush(void);
void dbg_log_close(void);
#else
#define dbg_log_open(path)               ((void)0)
#define dbg_log_set_enabled(on)          ((void)0)
#define dbg_log_printf(section, ...)     ((void)0)
#define dbg_log_flush()                  ((void)0)
#define dbg_log_close()                  ((void)0)
#endif

#endif /* DBG_LOG_H */
