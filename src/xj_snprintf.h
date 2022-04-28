#ifndef XJ_SNPRINTF_H
#define XJ_SNPRINTF_H
#include <stdarg.h>
#include "xjson.h"
int      xj_vsnprintf(char *buf, int count, const char *fmt, va_list va);
xj_value *xj_vdecodef(xj_alloc *alloc, xj_error *error, const char *fmt, va_list va);
xj_value  *xj_decodef(xj_alloc *alloc, xj_error *error, const char *fmt, ...);
#endif