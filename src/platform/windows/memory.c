#include "util/memory.h"

#include <io.h>
#include <Windows.h>

void* anonymousMemoryMap(size_t size) {
	HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size & 0xFFFFFFFF, 0);
	return MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, size);
}

void mappedMemoryFree(void* memory, size_t size) {
	// TODO fill in
}
