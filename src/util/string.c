#include "util/string.h"

#include <string.h>

#ifndef HAVE_STRNDUP
char* strndup(const char* start, size_t len) {
	// This is suboptimal, but anything recent should have strndup
	char* out = malloc((len + 1) * sizeof(char));
	strncpy(out, start, len);
	out[len] = '\0';
	return out;
}
#endif

char* strnrstr(const char* restrict haystack, const char* restrict needle, size_t len) {
	char* last = 0;
	const char* next = haystack;
	size_t needleLen = strlen(needle);
	for (; len >= needleLen; --len, ++next) {
		if (strncmp(needle, next, needleLen) == 0) {
			last = (char*) next;
		}
	}
	return last;
}
