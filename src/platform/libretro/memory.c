/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/memory.h>
#include <mgba-util/vector.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(GEKKO) || defined(__CELLOS_LV2__) || defined(_3DS) || defined(__SWITCH__)
/* stub */
#elif defined(VITA)
#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>
DECLARE_VECTOR(SceUIDList, SceUID);
DEFINE_VECTOR(SceUIDList, SceUID);

static struct SceUIDList uids;
static bool listInit = false;
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
#elif defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_3DS) || defined(__SWITCH__)
   return (void*)malloc(size);
#elif defined(VITA)
	if (!listInit)
		SceUIDListInit(&uids, 8);
	if (size & 0xFFF)
   {
		// Align to 4kB pages
		size += ((~size) & 0xFFF) + 1;
	}
	SceUID memblock = sceKernelAllocMemBlock("anon", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, 0);
	if (memblock < 0)
		return 0;
	*SceUIDListAppend(&uids) = memblock;
	void* ptr;
	if (sceKernelGetMemBlockBase(memblock, &ptr) < 0)
		return 0;
	return ptr;
#else
	return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
}

void mappedMemoryFree(void* memory, size_t size) {
	UNUSED(size);
#ifdef _WIN32
	// size is not useful here because we're freeing the memory, not decommitting it
	VirtualFree(memory, 0, MEM_RELEASE);
#elif defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_3DS) || defined(__SWITCH__)
   free(memory);
#elif defined(VITA)
	UNUSED(size);
	size_t i;
	for (i = 0; i < SceUIDListSize(&uids); ++i)
   {
		SceUID uid = *SceUIDListGetPointer(&uids, i);
		void* ptr;
		if (sceKernelGetMemBlockBase(uid, &ptr) < 0)
			continue;
		if (ptr == memory)
      {
			sceKernelFreeMemBlock(uid);
			SceUIDListUnshift(&uids, i, 1);
			return;
		}
	}
#else
	munmap(memory, size);
#endif
}
