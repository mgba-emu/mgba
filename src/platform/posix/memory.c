#include "util/memory.h"

#include <sys/mman.h>

void* anonymousMemoryMap(size_t size) {
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}
void* fileMemoryMap(int fd, size_t size, int flags) {
	int mmapFlags = MAP_PRIVATE;
	if (flags & MEMORY_WRITE) {
		mmapFlags = MAP_SHARED;
	}
	return mmap(0, size, PROT_READ | PROT_WRITE, mmapFlags, fd, 0);
}

void mappedMemoryFree(void* memory, size_t size) {
	munmap(memory, size);
}
