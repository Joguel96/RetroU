#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* The caller owns root and must keep it valid until n64_report_close(). */
int n64_report_open(const char *root);
void n64_report_close(void);

#ifdef __cplusplus
}
#endif
