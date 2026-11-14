#include "n64_report.h"

#include <coreinit/mutex.h>
#include <coreinit/time.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static OSMutex s_lock;
static int s_ready;
static FILE *s_file;
static char s_root[256];

int n64_report_open(const char *root)
{
    if (!root || !root[0] || strlen(root) + sizeof("/retrou_n64.log") > sizeof(s_root))
        return -1;
    if (!s_ready) {
        OSInitMutex(&s_lock);
        s_ready = 1;
    }
    OSLockMutex(&s_lock);
    if (s_file) {
        fclose(s_file);
        s_file = NULL;
    }
    snprintf(s_root, sizeof(s_root), "%s", root);
    char path[sizeof(s_root) + sizeof("/retrou_n64.log")];
    snprintf(path, sizeof(path), "%s/retrou_n64.log", s_root);
    s_file = fopen(path, "a");
    if (s_file)
        setvbuf(s_file, NULL, _IONBF, 0);
    OSUnlockMutex(&s_lock);
    return s_file ? 0 : -1;
}

void n64_report_close(void)
{
    if (!s_ready)
        return;
    OSLockMutex(&s_lock);
    if (s_file) {
        fclose(s_file);
        s_file = NULL;
    }
    s_root[0] = 0;
    OSUnlockMutex(&s_lock);
}

void report_line(const char *fmt, ...)
{
    if (!s_ready || !fmt)
        return;
    char line[768];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    OSLockMutex(&s_lock);
    if (s_file) {
        fputs(line, s_file);
        fputc('\n', s_file);
    }
    OSUnlockMutex(&s_lock);
}

bool report_save(void)
{
    if (!s_ready)
        return false;
    OSLockMutex(&s_lock);
    const bool saved = s_file && fflush(s_file) == 0;
    OSUnlockMutex(&s_lock);
    return saved;
}

const char *report_sd_root(void)
{
    return s_root[0] ? s_root : NULL;
}

double report_now_us(void)
{
    return (double)OSTicksToMicroseconds(OSGetSystemTime());
}
