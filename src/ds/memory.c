/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "arm/macros.h"

#include "ds/ds.h"
#include "util/math.h"
#include "util/memory.h"

mLOG_DEFINE_CATEGORY(DS_MEM, "DS Memory");

#define LDM_LOOP(LDM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			LDM; \
			cpu->gprs[i] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			LDM; \
			cpu->gprs[i + 1] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			LDM; \
			cpu->gprs[i + 2] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			LDM; \
			cpu->gprs[i + 3] = value; \
			++wait; \
			address += 4; \
		} \
	}

#define STM_LOOP(STM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			value = cpu->gprs[i]; \
			STM; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			value = cpu->gprs[i + 1]; \
			STM; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			value = cpu->gprs[i + 2]; \
			STM; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			value = cpu->gprs[i + 3]; \
			STM; \
			++wait; \
			address += 4; \
		} \
	}


static uint32_t _deadbeef[1] = { 0xE710B710 }; // Illegal instruction on both ARM and Thumb

static void DS7SetActiveRegion(struct ARMCore* cpu, uint32_t region);
static void DS9SetActiveRegion(struct ARMCore* cpu, uint32_t region);
static int32_t DSMemoryStall(struct ARMCore* cpu, int32_t wait);

static const int DMA_OFFSET[] = { 1, -1, 0, 1 };

void DSMemoryInit(struct DS* ds) {
	struct ARMCore* arm7 = ds->arm7;
	arm7->memory.load32 = DS7Load32;
	arm7->memory.load16 = DS7Load16;
	arm7->memory.load8 = DS7Load8;
	arm7->memory.loadMultiple = DS7LoadMultiple;
	arm7->memory.store32 = DS7Store32;
	arm7->memory.store16 = DS7Store16;
	arm7->memory.store8 = DS7Store8;
	arm7->memory.storeMultiple = DS7StoreMultiple;
	arm7->memory.stall = DSMemoryStall;

	struct ARMCore* arm9 = ds->arm9;
	arm9->memory.load32 = DS9Load32;
	arm9->memory.load16 = DS9Load16;
	arm9->memory.load8 = DS9Load8;
	arm9->memory.loadMultiple = DS9LoadMultiple;
	arm9->memory.store32 = DS9Store32;
	arm9->memory.store16 = DS9Store16;
	arm9->memory.store8 = DS9Store8;
	arm9->memory.storeMultiple = DS9StoreMultiple;
	arm9->memory.stall = DSMemoryStall;

	ds->memory.bios7 = NULL;
	ds->memory.bios9 = NULL;
	ds->memory.wram = NULL;
	ds->memory.wram7 = NULL;
	ds->memory.ram = NULL;
	ds->memory.itcm = NULL;
	ds->memory.dtcm = NULL;
	ds->memory.rom = NULL;

	ds->memory.activeRegion7 = -1;
	ds->memory.activeRegion9 = -1;

	arm7->memory.activeRegion = 0;
	arm7->memory.activeMask = 0;
	arm7->memory.setActiveRegion = DS7SetActiveRegion;
	arm7->memory.activeSeqCycles32 = 0;
	arm7->memory.activeSeqCycles16 = 0;
	arm7->memory.activeNonseqCycles32 = 0;
	arm7->memory.activeNonseqCycles16 = 0;

	arm9->memory.activeRegion = 0;
	arm9->memory.activeMask = 0;
	arm9->memory.setActiveRegion = DS9SetActiveRegion;
	arm9->memory.activeSeqCycles32 = 0;
	arm9->memory.activeSeqCycles16 = 0;
	arm9->memory.activeNonseqCycles32 = 0;
	arm9->memory.activeNonseqCycles16 = 0;
}

void DSMemoryDeinit(struct DS* ds) {
	mappedMemoryFree(ds->memory.wram, DS_SIZE_WORKING_RAM);
	mappedMemoryFree(ds->memory.wram7, DS7_SIZE_WORKING_RAM);
	mappedMemoryFree(ds->memory.ram, DS_SIZE_RAM);
	mappedMemoryFree(ds->memory.itcm, DS9_SIZE_ITCM);
	mappedMemoryFree(ds->memory.dtcm, DS9_SIZE_DTCM);
}

void DSMemoryReset(struct DS* ds) {
	if (ds->memory.wram) {
		mappedMemoryFree(ds->memory.wram, DS_SIZE_WORKING_RAM);
	}
	ds->memory.wram = anonymousMemoryMap(DS_SIZE_WORKING_RAM);

	if (ds->memory.wram7) {
		mappedMemoryFree(ds->memory.wram7, DS7_SIZE_WORKING_RAM);
	}
	ds->memory.wram7 = anonymousMemoryMap(DS7_SIZE_WORKING_RAM);

	if (ds->memory.ram) {
		mappedMemoryFree(ds->memory.ram, DS_SIZE_RAM);
	}
	ds->memory.ram = anonymousMemoryMap(DS_SIZE_RAM);

	if (ds->memory.itcm) {
		mappedMemoryFree(ds->memory.itcm, DS9_SIZE_ITCM);
	}
	ds->memory.itcm = anonymousMemoryMap(DS9_SIZE_ITCM);

	if (ds->memory.dtcm) {
		mappedMemoryFree(ds->memory.dtcm, DS9_SIZE_DTCM);
	}
	ds->memory.dtcm = anonymousMemoryMap(DS9_SIZE_DTCM);

	memset(ds->memory.dma7, 0, sizeof(ds->memory.dma7));
	memset(ds->memory.dma9, 0, sizeof(ds->memory.dma9));
	ds->memory.activeDMA7 = -1;
	ds->memory.activeDMA9 = -1;
	ds->memory.nextDMA = INT_MAX;
	ds->memory.eventDiff = 0;

	// TODO: Correct size
	ds->memory.wramSize7 = 0x8000;
	ds->memory.wramSize9 = 0;

	if (!ds->memory.wram || !ds->memory.wram7 || !ds->memory.ram || !ds->memory.itcm || !ds->memory.dtcm) {
		DSMemoryDeinit(ds);
		mLOG(DS_MEM, FATAL, "Could not map memory");
	}
}

static void DS7SetActiveRegion(struct ARMCore* cpu, uint32_t address) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;

	int newRegion = address >> DS_BASE_OFFSET;

	memory->activeRegion7 = newRegion;
	switch (newRegion) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !memory->wramSize7) {
			cpu->memory.activeRegion = memory->wram7;
			cpu->memory.activeMask = DS7_SIZE_WORKING_RAM - 1;
		} else {
			cpu->memory.activeRegion = memory->wram;
			cpu->memory.activeMask = ds->memory.wramSize7 - 1;
		}
		return;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			cpu->memory.activeRegion = memory->ram;
			cpu->memory.activeMask = DS_SIZE_RAM - 1;
			return;
		}
		break;
	case DS7_REGION_BIOS:
		if (memory->bios7) {
			cpu->memory.activeRegion = memory->bios7;
			cpu->memory.activeMask = DS9_SIZE_BIOS - 1;
		} else {
			cpu->memory.activeRegion = _deadbeef;
			cpu->memory.activeMask = 0;
		}
		return;
	default:
		break;
	}
	cpu->memory.activeRegion = _deadbeef;
	cpu->memory.activeMask = 0;
	mLOG(DS_MEM, FATAL, "Jumped to invalid address: %08X", address);
	return;
}

uint32_t DS7Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS7_REGION_BIOS:
		LOAD_32(value, address & (DS7_SIZE_BIOS - 1), memory->bios7);
		break;
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			LOAD_32(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			LOAD_32(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load32: %08X", address);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load32: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	// Unaligned 32-bit loads are "rotated" so they make some semblance of sense
	int rotate = (address & 3) << 3;
	return ROR(value, rotate);
}

uint32_t DS7Load16(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS7_REGION_BIOS:
		LOAD_16(value, address & (DS7_SIZE_BIOS - 1), memory->bios7);
		break;
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			LOAD_16(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			LOAD_16(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_16(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load16: %08X", address);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load16: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	// Unaligned 16-bit loads are "unpredictable", TODO: See what DS does
	int rotate = (address & 1) << 3;
	return ROR(value, rotate);
}

uint32_t DS7Load8(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			value = ((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load8: %08X", address);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load8: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	return value;
}

void DS7Store32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			STORE_32(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			STORE_32(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store32: %08X:%08X", address, value);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store32: %08X:%08X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

void DS7Store16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			STORE_16(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			STORE_16(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_16(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store16: %08X:%04X", address, value);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store16: %08X:%04X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

void DS7Store8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store8: %08X:%02X", address, value);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store8: %08X:%02X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

uint32_t DS7LoadMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value;
	int wait = 0;

	int i;
	int offset = 4;
	int popcount = 0;
	if (direction & LSM_D) {
		offset = -4;
		popcount = popcount32(mask);
		address -= (popcount << 2) - 4;
	}

	if (direction & LSM_B) {
		address += offset;
	}

	uint32_t addressMisalign = address & 0x3;
	address &= 0xFFFFFFFC;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		LDM_LOOP(if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			LOAD_32(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			LOAD_32(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		});
		break;
	case DS_REGION_RAM:
		LDM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS7 LDM: %08X", address);
		});
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 LDM: %08X", address);
		LDM_LOOP(value = 0);
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}

	if (direction & LSM_B) {
		address -= offset;
	}

	if (direction & LSM_D) {
		address -= (popcount << 2) + 4;
	}

	return address | addressMisalign;
}


uint32_t DS7StoreMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value;
	int wait = 0;

	int i;
	int offset = 4;
	int popcount = 0;
	if (direction & LSM_D) {
		offset = -4;
		popcount = popcount32(mask);
		address -= (popcount << 2) - 4;
	}

	if (direction & LSM_B) {
		address += offset;
	}

	uint32_t addressMisalign = address & 0x3;
	address &= 0xFFFFFFFC;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		STM_LOOP(if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			STORE_32(value, address & (DS7_SIZE_WORKING_RAM - 1), memory->wram7);
		} else {
			STORE_32(value, address & (ds->memory.wramSize7 - 1), memory->wram);
		});
		break;
	case DS_REGION_RAM:
		STM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		});
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		STM_LOOP();
		break;
	}

	if (cycleCounter) {
		*cycleCounter += wait;
	}

	if (direction & LSM_B) {
		address -= offset;
	}

	if (direction & LSM_D) {
		address -= (popcount << 2) + 4;
	}

	return address | addressMisalign;
}

static void DS9SetActiveRegion(struct ARMCore* cpu, uint32_t address) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;

	int newRegion = address >> DS_BASE_OFFSET;

	memory->activeRegion9 = newRegion;
	switch (newRegion) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < (512U << ARMTCMControlGetVirtualSize(cpu->cp15.r9.i))) {
			cpu->memory.activeRegion = memory->itcm;
			cpu->memory.activeMask = DS9_SIZE_ITCM - 1;
			return;
		}
		goto jump_error;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			cpu->memory.activeRegion = memory->ram;
			cpu->memory.activeMask = DS_SIZE_RAM - 1;
			return;
		}
		goto jump_error;
	case DS9_REGION_BIOS:
		// TODO: Mask properly
		if (memory->bios9) {
			cpu->memory.activeRegion = memory->bios9;
			cpu->memory.activeMask = DS9_SIZE_BIOS - 1;
		} else {
			cpu->memory.activeRegion = _deadbeef;
			cpu->memory.activeMask = 0;
		}
		return;
	default:
		break;
	}

jump_error:
	cpu->memory.activeRegion = _deadbeef;
	cpu->memory.activeMask = 0;
	mLOG(DS_MEM, FATAL, "Jumped to invalid address: %08X", address);
}

uint32_t DS9Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load32: %08X", address);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load32: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	// Unaligned 32-bit loads are "rotated" so they make some semblance of sense
	int rotate = (address & 3) << 3;
	return ROR(value, rotate);
}

uint32_t DS9Load16(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_16(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load16: %08X", address);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load16: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	// Unaligned 16-bit loads are "unpredictable", TODO: See what DS does
	int rotate = (address & 1) << 3;
	return ROR(value, rotate);
}

uint32_t DS9Load8(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			value = ((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load8: %08X", address);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load8: %08X", address);
		break;
	}

	if (cycleCounter) {
		wait += 2;
		*cycleCounter += wait;
	}
	return value;
}

void DS9Store32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < (512 << ARMTCMControlGetVirtualSize(cpu->cp15.r9.i))) {
			STORE_32(value, address & (DS9_SIZE_ITCM - 1), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store32: %08X:%08X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store32: %08X:%08X", address, value);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store32: %08X:%08X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

void DS9Store16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < (512 << ARMTCMControlGetVirtualSize(cpu->cp15.r9.i))) {
			STORE_16(value, address & (DS9_SIZE_ITCM - 1), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store16: %08X:%04X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_16(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store16: %08X:%04X", address, value);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store16: %08X:%04X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

void DS9Store8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	int wait = 0;

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < (512U << ARMTCMControlGetVirtualSize(cpu->cp15.r9.i))) {
			((uint8_t*) memory->itcm)[address & (DS9_SIZE_ITCM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store8: %08X:%02X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store8: %08X:%02X", address, value);
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store8: %08X:%02X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

uint32_t DS9LoadMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value;
	int wait = 0;

	int i;
	int offset = 4;
	int popcount = 0;
	if (direction & LSM_D) {
		offset = -4;
		popcount = popcount32(mask);
		address -= (popcount << 2) - 4;
	}

	if (direction & LSM_B) {
		address += offset;
	}

	uint32_t addressMisalign = address & 0x3;
	address &= 0xFFFFFFFC;

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_RAM:
		LDM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 LDM: %08X", address);
		});
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 LDM: %08X", address);
		LDM_LOOP(value = 0);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}

	if (direction & LSM_B) {
		address -= offset;
	}

	if (direction & LSM_D) {
		address -= (popcount << 2) + 4;
	}

	return address | addressMisalign;
}


uint32_t DS9StoreMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value;
	int wait = 0;

	int i;
	int offset = 4;
	int popcount = 0;
	if (direction & LSM_D) {
		offset = -4;
		popcount = popcount32(mask);
		address -= (popcount << 2) - 4;
	}

	if (direction & LSM_B) {
		address += offset;
	}

	uint32_t addressMisalign = address & 0x3;
	address &= 0xFFFFFFFC;

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		STM_LOOP(if (address < (512U << ARMTCMControlGetVirtualSize(cpu->cp15.r9.i))) {
			STORE_32(value, address & (DS9_SIZE_ITCM - 1), memory->itcm);
		} else {
			mLOG(DS_MEM, STUB, "Bad DS9 Store32: %08X:%08X", address, value);
		});
		break;
	case DS_REGION_RAM:
		STM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		});
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		STM_LOOP();
		break;
	}

	if (cycleCounter) {
		*cycleCounter += wait;
	}

	if (direction & LSM_B) {
		address -= offset;
	}

	if (direction & LSM_D) {
		address -= (popcount << 2) + 4;
	}

	return address | addressMisalign;
}

int32_t DSMemoryStall(struct ARMCore* cpu, int32_t wait) {
	return wait;
}

