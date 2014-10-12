#ifndef MEMORY_H
#define MEMORY_H

#include "util/common.h"

void* anonymousMemoryMap(size_t size);
void mappedMemoryFree(void* memory, size_t size);

#endif
