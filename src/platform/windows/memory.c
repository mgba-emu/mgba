#include "util/memory.h"

#include <io.h>
#include <Windows.h>

void* anonymousMemoryMap(size_t size) {
	HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size & 0xFFFFFFFF, 0);
	return MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, size);
}

void* fileMemoryMap(int fd, size_t size, int flags) {
	int createFlags = PAGE_READONLY;
	int mapFiles = FILE_MAP_READ;
	if (flags & MEMORY_WRITE) {
		createFlags = PAGE_READWRITE;
		mapFiles = FILE_MAP_WRITE;
	}
	size_t location = lseek(fd, 0, SEEK_CUR);
	size_t fileSize = lseek(fd, 0, SEEK_END);
	lseek(fd, location, SEEK_SET);
	if (size > fileSize) {
		size = fileSize;
	}
	HANDLE hMap = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, createFlags, 0, size & 0xFFFFFFFF, 0);
	return MapViewOfFile(hMap, mapFiles, 0, 0, size);
}

void mappedMemoryFree(void* memory, size_t size) {
	// TODO fill in
}
