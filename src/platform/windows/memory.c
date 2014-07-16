#include "util/memory.h"

#include <Windows.h>

void* anonymousMemoryMap(size_t size) {
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
	// size is not useful here because we're freeing the memory, not decommitting it
	VirtualFree(memory, 0, MEM_RELEASE);
}
