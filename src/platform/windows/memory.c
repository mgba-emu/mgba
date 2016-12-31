/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/memory.h>

#include <windows.h>

void* anonymousMemoryMap(size_t size) {
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
	// size is not useful here because we're freeing the memory, not decommitting it
	VirtualFree(memory, 0, MEM_RELEASE);
}
