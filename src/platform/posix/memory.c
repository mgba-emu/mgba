#include "util/memory.h"

#include <sys/mman.h>

void* anonymousMemoryMap(size_t size) {
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void mappedMemoryFree(void* memory, size_t size) {
	munmap(memory, size);
}
