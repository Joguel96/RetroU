#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void report_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
bool report_save(void);
const char *report_sd_root(void);
double report_now_us(void);

#ifdef __cplusplus
}
#endif
