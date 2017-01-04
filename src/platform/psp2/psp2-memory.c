/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/memory.h>
#include <mgba-util/vector.h>

#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>

DECLARE_VECTOR(SceUIDList, SceUID);
DEFINE_VECTOR(SceUIDList, SceUID);

static struct SceUIDList uids;
static bool listInit = false;

void* anonymousMemoryMap(size_t size) {
	if (!listInit) {
		SceUIDListInit(&uids, 8);
	}
	if (size & 0xFFF) {
		// Align to 4kB pages
		size += ((~size) & 0xFFF) + 1;
	}
	SceUID memblock = sceKernelAllocMemBlock("anon", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, 0);
	if (memblock < 0) {
		return 0;
	}
	*SceUIDListAppend(&uids) = memblock;
	void* ptr;
	if (sceKernelGetMemBlockBase(memblock, &ptr) < 0) {
		return 0;
	}
	return ptr;
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
	size_t i;
	for (i = 0; i < SceUIDListSize(&uids); ++i) {
		SceUID uid = *SceUIDListGetPointer(&uids, i);
		void* ptr;
		if (sceKernelGetMemBlockBase(uid, &ptr) < 0) {
			continue;
		}
		if (ptr == memory) {
			sceKernelFreeMemBlock(uid);
			SceUIDListShift(&uids, i, 1);
			return;
		}
	}
}
