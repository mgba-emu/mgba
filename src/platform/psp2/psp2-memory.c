/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/memory.h>

#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>

void* anonymousMemoryMap(size_t size) {
	if (size & 0xFFF) {
		// Align to 4kB pages
		size += ((~size) & 0xFFF) + 1;
	}
	SceUID memblock = sceKernelAllocMemBlock("anon", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, 0);
	if (memblock < 0) {
		return 0;
	}
	void* ptr;
	if (sceKernelGetMemBlockBase(memblock, &ptr) < 0) {
		return 0;
	}
	return ptr;
}

void mappedMemoryFree(void* memory, size_t size) {
	SceUID uid = sceKernelFindMemBlockByAddr(memory, size);
	if (uid >= 0) {
		sceKernelFreeMemBlock(uid);
	}
}
