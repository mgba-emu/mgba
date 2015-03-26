/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/memory.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

void* anonymousMemoryMap(size_t size) {
#ifdef _WIN32
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
}

void mappedMemoryFree(void* memory, size_t size) {
#ifdef _WIN32
	UNUSED(size);
	// size is not useful here because we're freeing the memory, not decommitting it
	VirtualFree(memory, 0, MEM_RELEASE);
#else
	munmap(memory, size);
#endif
}
