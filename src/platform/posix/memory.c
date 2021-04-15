/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/memory.h>

#ifndef DISABLE_ANON_MMAP
#ifdef __SANITIZE_ADDRESS__
#define DISABLE_ANON_MMAP
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define DISABLE_ANON_MMAP
#endif
#endif
#endif

#ifndef DISABLE_ANON_MMAP
#include <sys/mman.h>

void* anonymousMemoryMap(size_t size) {
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void mappedMemoryFree(void* memory, size_t size) {
	munmap(memory, size);
}
#else
void* anonymousMemoryMap(size_t size) {
	return calloc(1, size);
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
	free(memory);
}
#endif
