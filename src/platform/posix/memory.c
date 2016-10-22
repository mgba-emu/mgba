/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/memory.h"

#include <sys/mman.h>

extern uint32_t _execMem;
static void* _execMemHead = &_execMem;

void* anonymousMemoryMap(size_t size) {
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void* executableMemoryMap(size_t size) {
	void* out = _execMemHead;
	_execMemHead += size / sizeof(_execMemHead);
	return out;
}

void mappedMemoryFree(void* memory, size_t size) {
	if (memory < _execMemHead + 0x20000) {
		_execMemHead -= size / sizeof(_execMemHead);
		return;
	}
	munmap(memory, size);
}
