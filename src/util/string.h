#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include "util/common.h"

#ifndef strndup
// This is sometimes a macro
char* strndup(const char* start, size_t len);
#endif

char* strnrstr(const char* restrict s1, const char* restrict s2, size_t len);

#endif
