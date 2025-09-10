#include "dbg_log.h"

#if DBG_LOG_ENABLE
#include <stdio.h>
#include <stdarg.h>

static FILE *g_log = 0;
static int   g_enabled = 1;

void dbg_log_open(const char *path)
{
    if (!g_log) {
        if (!path) path = "app_dbg.txt";
        g_log = fopen(path, "w");
        if (g_log) {
            fputs("-- log start --\n", g_log);
            fflush(g_log);
        }
    }
}

void dbg_log_set_enabled(int on)
{
    g_enabled = on ? 1 : 0;
}

void dbg_log_printf(const char *section, const char *fmt, ...)
{
    va_list ap;
    if (!g_log || !g_enabled) return;
    fputs(section ? section : "LOG", g_log);
    fputs(": ", g_log);
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void dbg_log_flush(void)
{
    if (g_log) fflush(g_log);
}

void dbg_log_close(void)
{
    if (g_log) {
        fputs("-- log end --\n", g_log);
        fclose(g_log);
        g_log = 0;
    }
}
#endif /* DBG_LOG_ENABLE */

