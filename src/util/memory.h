#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

#define MEMORY_READ 1
#define MEMORY_WRITE 2

void* anonymousMemoryMap(size_t size);
void mappedMemoryFree(void* memory, size_t size);

#endif
