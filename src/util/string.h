#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include "util/common.h"

char* strndup(const char* start, size_t len);
char* strnrstr(const char* restrict s1, const char* restrict s2, size_t len);

#endif
