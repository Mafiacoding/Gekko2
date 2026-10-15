#ifndef FRONTEND_LOG_H
#define FRONTEND_LOG_H
#include <stdint.h>
#include <stdio.h>
/* One immutable path per cold boot. Resume never creates or truncates a file. */
typedef struct {
 char path[256];
 uint32_t sequence,errors;
 int last_error;
} frontend_log;
int frontend_log_begin(frontend_log *,const char *directory,int disc,int gx);
FILE *frontend_log_open(frontend_log *);
int frontend_log_close(frontend_log *,FILE *);
#endif
