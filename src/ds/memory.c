/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/memory.h>

#include <mgba/internal/arm/macros.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>
#include <mgba-util/math.h>
#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(DS_MEM, "DS Memory", "ds.memory");

static uint32_t _deadbeef[1] = { 0xE710B710 }; // Illegal instruction on both ARM and Thumb
const uint32_t redzoneInstruction = 0xE7F0DEF0;

static const uint32_t _vramMask[9] = {
	0x1FFFF,
	0x1FFFF,
	0x1FFFF,
	0x1FFFF,
	0x0FFFF,
	0x03FFF,
	0x03FFF,
	0x07FFF,
	0x03FFF
};

static void DS7SetActiveRegion(struct ARMCore* cpu, uint32_t region);
static void DS9SetActiveRegion(struct ARMCore* cpu, uint32_t region);
static int32_t DSMemoryStall(struct ARMCore* cpu, int32_t wait);

static unsigned _selectVRAM(struct DSMemory* memory, uint32_t offset);

static const char DS7_BASE_WAITSTATES[16] =        { 0, 0, 8, 0, 0, 0, 0, 0 };
static const char DS7_BASE_WAITSTATES_32[16] =     { 0, 0, 9, 0, 0, 1, 1, 0 };
static const char DS7_BASE_WAITSTATES_SEQ[16] =    { 0, 0, 1, 0, 0, 0, 0, 0 };
static const char DS7_BASE_WAITSTATES_SEQ_32[16] = { 0, 0, 2, 0, 0, 1, 1, 0 };

static const char DS9_BASE_WAITSTATES[16] =        { 0, 0, 2, 6, 6, 7, 7, 6 };
static const char DS9_BASE_WAITSTATES_32[16] =     { 0, 0, 4, 6, 6, 9, 9, 6 };
static const char DS9_BASE_WAITSTATES_SEQ[16] =    { 0, 0, 1, 1, 1, 2, 2, 1 };
static const char DS9_BASE_WAITSTATES_SEQ_32[16] = { 0, 0, 2, 1, 1, 4, 4, 1 };

void DSMemoryInit(struct DS* ds) {
	struct ARMCore* arm7 = ds->ds7.cpu;
	arm7->memory.load32 = DS7Load32;
	arm7->memory.load16 = DS7Load16;
	arm7->memory.load8 = DS7Load8;
	arm7->memory.loadMultiple = DS7LoadMultiple;
	arm7->memory.store32 = DS7Store32;
	arm7->memory.store16 = DS7Store16;
	arm7->memory.store8 = DS7Store8;
	arm7->memory.storeMultiple = DS7StoreMultiple;
	arm7->memory.stall = DSMemoryStall;

	struct ARMCore* arm9 = ds->ds9.cpu;
	arm9->memory.load32 = DS9Load32;
	arm9->memory.load16 = DS9Load16;
	arm9->memory.load8 = DS9Load8;
	arm9->memory.loadMultiple = DS9LoadMultiple;
	arm9->memory.store32 = DS9Store32;
	arm9->memory.store16 = DS9Store16;
	arm9->memory.store8 = DS9Store8;
	arm9->memory.storeMultiple = DS9StoreMultiple;
	arm9->memory.stall = DSMemoryStall;

	int i;
	for (i = 0; i < 8; ++i) {
		// TODO: Formalize
		ds->ds7.memory.waitstatesNonseq16[i] = DS7_BASE_WAITSTATES[i];
		ds->ds7.memory.waitstatesSeq16[i] = DS7_BASE_WAITSTATES_SEQ[i];
		ds->ds7.memory.waitstatesPrefetchNonseq16[i] = DS7_BASE_WAITSTATES[i];
		ds->ds7.memory.waitstatesPrefetchSeq16[i] = DS7_BASE_WAITSTATES_SEQ[i];
		ds->ds7.memory.waitstatesNonseq32[i] = DS7_BASE_WAITSTATES_32[i];
		ds->ds7.memory.waitstatesSeq32[i] = DS7_BASE_WAITSTATES_SEQ_32[i];
		ds->ds7.memory.waitstatesPrefetchNonseq32[i] = DS7_BASE_WAITSTATES_32[i];
		ds->ds7.memory.waitstatesPrefetchSeq32[i] = DS7_BASE_WAITSTATES_SEQ_32[i];

		ds->ds9.memory.waitstatesNonseq16[i] = DS9_BASE_WAITSTATES[i];
		ds->ds9.memory.waitstatesSeq16[i] = DS9_BASE_WAITSTATES_SEQ[i];
		ds->ds9.memory.waitstatesPrefetchNonseq16[i] = DS9_BASE_WAITSTATES[i];
		ds->ds9.memory.waitstatesPrefetchSeq16[i] = DS9_BASE_WAITSTATES[i];
		ds->ds9.memory.waitstatesNonseq32[i] = DS9_BASE_WAITSTATES_32[i];
		ds->ds9.memory.waitstatesSeq32[i] = DS9_BASE_WAITSTATES_SEQ_32[i];
		ds->ds9.memory.waitstatesPrefetchNonseq32[i] = DS9_BASE_WAITSTATES_32[i];
		ds->ds9.memory.waitstatesPrefetchSeq32[i] = DS9_BASE_WAITSTATES_32[i];
	}

	ds->ds9.memory.waitstatesPrefetchNonseq16[2] = 0;
	ds->ds9.memory.waitstatesPrefetchSeq16[2] = 0;
	ds->ds9.memory.waitstatesPrefetchNonseq32[2] = 0;
	ds->ds9.memory.waitstatesPrefetchSeq32[2] = 0;

	for (; i < 256; ++i) {
		ds->ds7.memory.waitstatesNonseq16[i] = 0;
		ds->ds7.memory.waitstatesSeq16[i] = 0;
		ds->ds7.memory.waitstatesNonseq32[i] = 0;
		ds->ds7.memory.waitstatesSeq32[i] = 0;

		ds->ds9.memory.waitstatesNonseq16[i] = 0;
		ds->ds9.memory.waitstatesSeq16[i] = 0;
		ds->ds9.memory.waitstatesNonseq32[i] = 0;
		ds->ds9.memory.waitstatesSeq32[i] = 0;
	}

	ds->memory.bios7 = NULL;
	ds->memory.bios9 = NULL;
	ds->memory.wramBase = NULL;
	ds->memory.wram7 = NULL;
	ds->memory.ram = NULL;
	ds->memory.itcm = NULL;
	ds->memory.dtcm = NULL;
	ds->memory.rom = NULL;

	ds->ds7.memory.activeRegion = -1;
	ds->ds9.memory.activeRegion = -1;
	ds->ds7.memory.io = ds->memory.io7;
	ds->ds9.memory.io = ds->memory.io9;

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
		mappedMemoryFree(ds->memory.wramBase, DS_SIZE_WORKING_RAM * 2 + 12);
	}
	// XXX: This hack lets you roll over the end of the WRAM block without
	// looping back to the beginning. It works by placing an undefined
	// instruction in a redzone at the very beginning and end of the buffer.
	// Using clever masking tricks, the ARM loop will mask the offset so that
	// either the middle of the passed-in buffer is the actual buffer, and
	// when the loop rolls over, it hits the redzone at the beginning, or the
	// start of the passed-in buffer matches the actual buffer, causing the
	// redzone at the end to be hit. This requires a lot of dead space in
	// the middle, and a fake (too large) mask, but it is very fast.
	ds->memory.wram = anonymousMemoryMap(DS_SIZE_WORKING_RAM * 2 + 12);
	ds->memory.wram[0] = redzoneInstruction;
	ds->memory.wram[1] = redzoneInstruction;
	ds->memory.wram[2] = redzoneInstruction;
	ds->memory.wram[DS_SIZE_WORKING_RAM >> 1] = redzoneInstruction;
	ds->memory.wram[(DS_SIZE_WORKING_RAM >> 1) + 1] = redzoneInstruction;
	ds->memory.wram[(DS_SIZE_WORKING_RAM >> 1) + 2] = redzoneInstruction;
	ds->memory.wramBase = &ds->memory.wram[DS_SIZE_WORKING_RAM >> 2];

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

	memset(ds->ds7.memory.dma, 0, sizeof(ds->ds7.memory.dma));
	memset(ds->ds9.memory.dma, 0, sizeof(ds->ds9.memory.dma));
	ds->ds7.memory.activeDMA = -1;
	ds->ds9.memory.activeDMA = -1;

	// TODO: Correct size
	ds->memory.wramSize7 = 0x8000;
	ds->memory.wramBase7 = ds->memory.wram;
	ds->memory.wramSize9 = 0;
	ds->memory.wramBase9 = NULL;

	ds->memory.slot1Owner = true;
	ds->memory.slot2Owner = true;
	ds->memory.slot1.savedataType = DS_SAVEDATA_AUTODETECT;
	ds->ds7.memory.slot1Access = true;
	ds->ds9.memory.slot1Access = false;

	DSSPIReset(ds);
	DSSlot1Reset(ds);

	DSVideoConfigureVRAM(ds, 0, 0, 1);
	DSVideoConfigureVRAM(ds, 1, 0, 1);
	DSVideoConfigureVRAM(ds, 2, 0, 1);
	DSVideoConfigureVRAM(ds, 3, 0, 1);
	DSVideoConfigureVRAM(ds, 4, 0, 1);
	DSVideoConfigureVRAM(ds, 5, 0, 1);
	DSVideoConfigureVRAM(ds, 6, 0, 1);
	DSVideoConfigureVRAM(ds, 7, 0, 1);
	DSVideoConfigureVRAM(ds, 8, 0, 1);
	DSConfigureWRAM(&ds->memory, 3);

	if (!ds->memory.wram || !ds->memory.wram7 || !ds->memory.ram || !ds->memory.itcm || !ds->memory.dtcm) {
		DSMemoryDeinit(ds);
		mLOG(DS_MEM, FATAL, "Could not map memory");
	}
}

static void DS7SetActiveRegion(struct ARMCore* cpu, uint32_t address) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSCoreMemory* memory = &ds->ds7.memory;

	int newRegion = address >> DS_BASE_OFFSET;

	memory->activeRegion = newRegion;
	switch (newRegion) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			cpu->memory.activeRegion = ds->memory.wram7;
			cpu->memory.activeMask = DS7_SIZE_WORKING_RAM - 1;
		} else if (ds->memory.wramSize7 == DS_SIZE_WORKING_RAM) {
			if (address & DS_SIZE_WORKING_RAM) {
				cpu->memory.activeRegion = ds->memory.wram;
			} else {
				cpu->memory.activeRegion = ds->memory.wramBase;
			}
			cpu->memory.activeMask = (ds->memory.wramSize7 << 1) - 1;
		} else {
			cpu->memory.activeRegion = ds->memory.wramBase;
			cpu->memory.activeMask = (ds->memory.wramSize7 - 1);
		}
		break;
	case DS7_REGION_BIOS:
		if (ds->memory.bios7) {
			cpu->memory.activeRegion = ds->memory.bios7;
			cpu->memory.activeMask = DS9_SIZE_BIOS - 1;
		} else {
			cpu->memory.activeRegion = _deadbeef;
			cpu->memory.activeMask = 0;
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			cpu->memory.activeRegion = ds->memory.ram;
			cpu->memory.activeMask = DS_SIZE_RAM - 1;
			break;
		}
		goto jump_error;
	case DS_REGION_VRAM:
		if (address < 0x06040000 && ds->memory.vram7[(address & 0x3FFFF) >> 17]) {
			// TODO: redzones
			cpu->memory.activeRegion = (uint32_t*) ds->memory.vram7[(address & 0x3FFFF) >> 17];
			cpu->memory.activeMask = 0x1FFFF;
			break;
		}
		// Fall through
	default:
	jump_error:
		memory->activeRegion = -1;
		cpu->memory.activeRegion = _deadbeef;
		cpu->memory.activeMask = 0;
		mLOG(DS_MEM, FATAL, "Jumped to invalid address: %08X", address);
		break;
	}
	cpu->memory.activeSeqCycles32 = memory->waitstatesPrefetchSeq32[memory->activeRegion];
	cpu->memory.activeSeqCycles16 = memory->waitstatesPrefetchSeq16[memory->activeRegion];
	cpu->memory.activeNonseqCycles32 = memory->waitstatesPrefetchNonseq32[memory->activeRegion];
	cpu->memory.activeNonseqCycles16 = memory->waitstatesPrefetchNonseq16[memory->activeRegion];
}

uint32_t DS7Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = ds->ds7.memory.waitstatesNonseq32[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS7_REGION_BIOS:
		LOAD_32(value, address & (DS7_SIZE_BIOS - 4), memory->bios7);
		break;
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			LOAD_32(value, address & (DS7_SIZE_WORKING_RAM - 4), memory->wram7);
		} else {
			LOAD_32(value, address & (ds->memory.wramSize7 - 4), memory->wramBase7);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 4), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load32: %08X", address);
		break;
	case DS_REGION_IO:
		value = DS7IORead32(ds, address & 0x00FFFFFC);
		break;
	case DS_REGION_VRAM:
		if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			LOAD_32(value, address & 0x1FFFC, memory->vram7[(address & 0x3FFFF) >> 17]);
			break;
		}
		// Fall through
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
	int wait = ds->ds7.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS7_REGION_BIOS:
		LOAD_16(value, address & (DS7_SIZE_BIOS - 2), memory->bios7);
		break;
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			LOAD_16(value, address & (DS7_SIZE_WORKING_RAM - 2), memory->wram7);
		} else {
			LOAD_16(value, address & (ds->memory.wramSize7 - 2), memory->wramBase7);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_16(value, address & (DS_SIZE_RAM - 1), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load16: %08X", address);
	case DS_REGION_IO:
		value = DS7IORead(ds, address & DS_OFFSET_MASK);
		break;
	case DS_REGION_VRAM:
		if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			LOAD_16(value, address & 0x1FFFE, memory->vram7[(address & 0x3FFFF) >> 17]);
			break;
		}
		// Fall through
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
	int wait = ds->ds7.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			value = ((uint8_t*) memory->wram7)[address & (DS7_SIZE_WORKING_RAM - 1)];
		} else {
			value = ((uint8_t*) memory->wramBase7)[address & (ds->memory.wramSize7 - 1)];
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			value = ((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Load8: %08X", address);
		break;
	case DS_REGION_IO:
		value = (DS7IORead(ds, address & 0xFFFE) >> ((address & 0x0001) << 3)) & 0xFF;
		break;
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
	int wait = ds->ds7.memory.waitstatesNonseq32[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			STORE_32(value, address & (DS7_SIZE_WORKING_RAM - 4), memory->wram7);
		} else {
			STORE_32(value, address & (ds->memory.wramSize7 - 4), memory->wramBase7);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 4), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store32: %08X:%08X", address, value);
		break;
	case DS_REGION_IO:
		DS7IOWrite32(ds, address & DS_OFFSET_MASK, value);
		break;
	case DS_REGION_VRAM:
		if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			STORE_32(value, address & 0x1FFFC, memory->vram7[(address & 0x3FFFF) >> 17]);
			break;
		}
		// Fall through
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
	int wait = ds->ds7.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			STORE_16(value, address & (DS7_SIZE_WORKING_RAM - 2), memory->wram7);
		} else {
			STORE_16(value, address & (ds->memory.wramSize7 - 2), memory->wramBase7);
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_16(value, address & (DS_SIZE_RAM - 2), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store16: %08X:%04X", address, value);
		break;
	case DS_REGION_IO:
		DS7IOWrite(ds, address & DS_OFFSET_MASK, value);
		break;
	case DS_REGION_VRAM:
		if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			STORE_16(value, address & 0x1FFFE, memory->vram7[(address & 0x3FFFF) >> 17]);
			break;
		}
		// Fall through
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
	int wait = ds->ds7.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS_REGION_WORKING_RAM:
		if (address >= DS7_BASE_WORKING_RAM || !ds->memory.wramSize7) {
			((uint8_t*) memory->wram7)[address & (DS7_SIZE_WORKING_RAM - 1)] = value;
		} else {
			((uint8_t*) memory->wramBase7)[address & (ds->memory.wramSize7 - 1)] = value;
		}
		break;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store8: %08X:%02X", address, value);
	case DS_REGION_IO:
		DS7IOWrite8(ds, address & DS_OFFSET_MASK, value);
		break;
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DS7 Store8: %08X:%02X", address, value);
		break;
	}

	if (cycleCounter) {
		++wait;
		*cycleCounter += wait;
	}
}

#define LDM_LOOP(LDM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			LDM; \
			cpu->gprs[i] = value; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			LDM; \
			cpu->gprs[i + 1] = value; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			LDM; \
			cpu->gprs[i + 2] = value; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			LDM; \
			cpu->gprs[i + 3] = value; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
	}

#define STM_LOOP(STM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			value = cpu->gprs[i]; \
			STM; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			value = cpu->gprs[i + 1]; \
			STM; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			value = cpu->gprs[i + 2]; \
			STM; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			value = cpu->gprs[i + 3]; \
			STM; \
			++wait; \
			wait += ws32[address >> DS_BASE_OFFSET]; \
			address += 4; \
		} \
	}

uint32_t DS7LoadMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	char* ws32 = ds->ds7.memory.waitstatesNonseq32;
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
			LOAD_32(value, address & (ds->memory.wramSize7 - 1), memory->wramBase7);
		});
		break;
	case DS_REGION_RAM:
		LDM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS7 LDM: %08X", address);
		});
		break;
	case DS_REGION_IO:
		LDM_LOOP(value = DS7IORead32(ds, address));
		break;
	case DS_REGION_VRAM:
		LDM_LOOP(if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			LOAD_32(value, address & 0x1FFFF, memory->vram7[(address & 0x3FFFF) >> 17]);
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
	char* ws32 = ds->ds7.memory.waitstatesNonseq32;
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
			STORE_32(value, address & (ds->memory.wramSize7 - 1), memory->wramBase7);
		});
		break;
	case DS_REGION_RAM:
		STM_LOOP(if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		});
		break;
	case DS_REGION_VRAM:
		STM_LOOP(if (address < 0x06040000 && memory->vram7[(address & 0x3FFFF) >> 17]) {
			STORE_32(value, address & 0x1FFFF, memory->vram7[(address & 0x3FFFF) >> 17]);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS7 STM: %08X", address);
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
	struct DSCoreMemory* memory = &ds->ds9.memory;

	int newRegion = address >> DS_BASE_OFFSET;

	memory->activeRegion = newRegion;
	switch (newRegion) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < ds->memory.itcmSize) {
			cpu->memory.activeRegion = ds->memory.itcm;
			cpu->memory.activeMask = DS9_SIZE_ITCM - 1;
			break;
		}
		goto jump_error;
	case DS_REGION_RAM:
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			cpu->memory.activeRegion = ds->memory.ram;
			cpu->memory.activeMask = DS_SIZE_RAM - 1;
			break;
		}
		goto jump_error;
	case DS9_REGION_BIOS:
		// TODO: Mask properly
		if (ds->memory.bios9) {
			cpu->memory.activeRegion = ds->memory.bios9;
			cpu->memory.activeMask = DS9_SIZE_BIOS - 1;
		} else {
			cpu->memory.activeRegion = _deadbeef;
			cpu->memory.activeMask = 0;
		}
		cpu->memory.activeSeqCycles32 = 0;
		cpu->memory.activeSeqCycles16 = 0;
		cpu->memory.activeNonseqCycles32 = 0;
		cpu->memory.activeNonseqCycles16 = 0;
		return;
	default:
	jump_error:
		memory->activeRegion = -1;
		cpu->memory.activeRegion = _deadbeef;
		cpu->memory.activeMask = 0;
		mLOG(DS_MEM, FATAL, "Jumped to invalid address: %08X", address);
		return;
	}
	cpu->memory.activeSeqCycles32 = memory->waitstatesPrefetchSeq32[memory->activeRegion];
	cpu->memory.activeSeqCycles16 = memory->waitstatesPrefetchSeq16[memory->activeRegion];
	cpu->memory.activeNonseqCycles32 = memory->waitstatesPrefetchNonseq32[memory->activeRegion];
	cpu->memory.activeNonseqCycles16 = memory->waitstatesPrefetchNonseq16[memory->activeRegion];
}

uint32_t DS9Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct DS* ds = (struct DS*) cpu->master;
	struct DSMemory* memory = &ds->memory;
	uint32_t value = 0;
	int wait = ds->ds9.memory.waitstatesNonseq32[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			LOAD_32(value, address & (DS9_SIZE_ITCM - 4), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load32: %08X", address);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			LOAD_32(value, address & (ds->memory.wramSize9 - 4), memory->wramBase9);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load32: %08X", address);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_32(value, address & (DS9_SIZE_DTCM - 4), memory->dtcm);
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 4), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load32: %08X", address);
		break;
	case DS_REGION_IO:
		value = DS9IORead32(ds, address & 0x00FFFFFC);
		break;
	case DS9_REGION_PALETTE_RAM:
		LOAD_32(value, address & (DS9_SIZE_PALETTE_RAM - 4), ds->video.palette);
		break;
	case DS_REGION_VRAM: {
		unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				uint32_t newValue;
				LOAD_32(newValue, address & _vramMask[i], memory->vramBank[i]);
				value |= newValue;
			}
		}
		break;
	}
	case DS9_REGION_OAM:
		LOAD_32(value, address & (DS9_SIZE_OAM - 4), ds->video.oam.raw);
		break;
	case DS9_REGION_BIOS:
		// TODO: Fix undersized BIOS
		// TODO: Fix masking
		LOAD_32(value, address & (DS9_SIZE_BIOS - 4), memory->bios9);
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_32(value, address & (DS9_SIZE_DTCM - 4), memory->dtcm);
			break;
		}
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
	int wait = ds->ds9.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			LOAD_16(value, address & (DS9_SIZE_ITCM - 2), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load16: %08X", address);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			LOAD_16(value, address & (ds->memory.wramSize9 - 2), memory->wramBase9);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load16: %08X", address);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_16(value, address & (DS9_SIZE_DTCM - 2), memory->dtcm);
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_16(value, address & (DS_SIZE_RAM - 2), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load16: %08X", address);
	case DS_REGION_IO:
		value = DS9IORead(ds, address & DS_OFFSET_MASK);
		break;
	case DS9_REGION_PALETTE_RAM:
		LOAD_16(value, address & (DS9_SIZE_PALETTE_RAM - 2), ds->video.palette);
		break;
	case DS_REGION_VRAM: {
		unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				uint32_t newValue;
				LOAD_16(newValue, address & _vramMask[i], memory->vramBank[i]);
				value |= newValue;
			}
		}
		break;
	}
	case DS9_REGION_OAM:
		LOAD_16(value, address & (DS9_SIZE_OAM - 2), ds->video.oam.raw);
		break;
	case DS9_REGION_BIOS:
		// TODO: Fix undersized BIOS
		// TODO: Fix masking
		LOAD_16(value, address & (DS9_SIZE_BIOS - 2), memory->bios9);
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_16(value, address & (DS9_SIZE_DTCM - 2), memory->dtcm);
			break;
		}
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
	int wait = ds->ds9.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			value = ((uint8_t*) memory->itcm)[address & (DS9_SIZE_ITCM - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load8: %08X", address);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			value = ((uint8_t*) memory->wramBase9)[address & (memory->wramSize9 - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Load8: %08X", address);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			value = ((uint8_t*) memory->dtcm)[address & (DS9_SIZE_DTCM - 1)];
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			value = ((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)];
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Load8: %08X", address);
	case DS_REGION_IO:
		value = (DS9IORead(ds, address & 0xFFFE) >> ((address & 0x0001) << 3)) & 0xFF;
		break;
	case DS9_REGION_BIOS:
		// TODO: Fix undersized BIOS
		// TODO: Fix masking
		value = ((uint8_t*) memory->bios9)[address & (DS9_SIZE_BIOS - 1)];
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			value = ((uint8_t*) memory->dtcm)[address & (DS9_SIZE_DTCM - 1)];
			break;
		}
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
	int wait = ds->ds9.memory.waitstatesNonseq32[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			STORE_32(value, address & (DS9_SIZE_ITCM - 4), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store32: %08X:%08X", address, value);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			STORE_32(value, address & (ds->memory.wramSize9 - 4), memory->wramBase9);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store32: %08X:%08X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_32(value, address & (DS9_SIZE_DTCM - 4), memory->dtcm);
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 4), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store32: %08X:%08X", address, value);
		break;
	case DS_REGION_IO:
		DS9IOWrite32(ds, address & DS_OFFSET_MASK, value);
		break;
	case DS9_REGION_PALETTE_RAM:
		STORE_32(value, address & (DS9_SIZE_PALETTE_RAM - 4), ds->video.palette);
		ds->video.renderer->writePalette(ds->video.renderer, (address & (DS9_SIZE_PALETTE_RAM - 4)) + 2, value >> 16);
		ds->video.renderer->writePalette(ds->video.renderer, address & (DS9_SIZE_PALETTE_RAM - 4), value);
		break;
	case DS_REGION_VRAM: {
		unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				STORE_32(value, address & _vramMask[i], memory->vramBank[i]);
			}
		}
		break;
	}
	case DS9_REGION_OAM:
		STORE_32(value, address & (DS9_SIZE_OAM - 4), ds->video.oam.raw);
		ds->video.renderer->writeOAM(ds->video.renderer, (address & (DS9_SIZE_OAM - 4)) >> 1);
		ds->video.renderer->writeOAM(ds->video.renderer, ((address & (DS9_SIZE_OAM - 4)) >> 1) + 1);
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_32(value, address & (DS9_SIZE_DTCM - 4), memory->dtcm);
			break;
		}
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
	int wait = ds->ds9.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			STORE_16(value, address & (DS9_SIZE_ITCM - 2), memory->itcm);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store16: %08X:%04X", address, value);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			STORE_16(value, address & (ds->memory.wramSize9 - 2), memory->wramBase9);
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store16: %08X:%04X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_16(value, address & (DS9_SIZE_DTCM - 2), memory->dtcm);
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_16(value, address & (DS_SIZE_RAM - 2), memory->ram);
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store16: %08X:%04X", address, value);
		break;
	case DS_REGION_IO:
		DS9IOWrite(ds, address & DS_OFFSET_MASK, value);
		break;
	case DS9_REGION_PALETTE_RAM:
		STORE_16(value, address & (DS9_SIZE_PALETTE_RAM - 2), ds->video.palette);
		ds->video.renderer->writePalette(ds->video.renderer, address & (DS9_SIZE_PALETTE_RAM - 2), value);
		break;
	case DS_REGION_VRAM: {
		unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				STORE_16(value, address & _vramMask[i], memory->vramBank[i]);
			}
		}
		break;
	}
	case DS9_REGION_OAM:
		STORE_16(value, address & (DS9_SIZE_OAM - 2), ds->video.oam.raw);
		ds->video.renderer->writeOAM(ds->video.renderer, (address & (DS9_SIZE_OAM - 2)) >> 1);
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_16(value, address & (DS9_SIZE_DTCM - 1), memory->dtcm);
			break;
		}
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
	int wait = ds->ds9.memory.waitstatesNonseq16[address >> DS_BASE_OFFSET];

	switch (address >> DS_BASE_OFFSET) {
	case DS9_REGION_ITCM:
	case DS9_REGION_ITCM_MIRROR:
		if (address < memory->itcmSize) {
			((uint8_t*) memory->itcm)[address & (DS9_SIZE_ITCM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store8: %08X:%02X", address, value);
		break;
	case DS_REGION_WORKING_RAM:
		if (ds->memory.wramSize9) {
			((uint8_t*) memory->wramBase9)[address & (ds->memory.wramSize9 - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Bad DS9 Store8: %08X:%02X", address, value);
		break;
	case DS_REGION_RAM:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			((uint8_t*) memory->dtcm)[address & (DS9_SIZE_DTCM - 1)] = value;
			break;
		}
		if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			((uint8_t*) memory->ram)[address & (DS_SIZE_RAM - 1)] = value;
			break;
		}
		mLOG(DS_MEM, STUB, "Unimplemented DS9 Store8: %08X:%02X", address, value);
	case DS_REGION_IO:
		DS9IOWrite8(ds, address & DS_OFFSET_MASK, value);
		break;
	default:
		if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			((uint8_t*) memory->dtcm)[address & (DS9_SIZE_DTCM - 1)] = value;
			break;
		}
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
	char* ws32 = ds->ds9.memory.waitstatesNonseq32;
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
		LDM_LOOP(if (address < memory->itcmSize) {
			LOAD_32(value, address & (DS9_SIZE_ITCM - 1), memory->itcm);
		} else {
			mLOG(DS_MEM, STUB, "Bad DS9 LDM: %08X:%08X", address, value);
		});
		break;
	case DS_REGION_WORKING_RAM:
		LDM_LOOP(if (ds->memory.wramSize9) {
			LOAD_32(value, address & (ds->memory.wramSize9 - 4), memory->wramBase9);
		} else {
			mLOG(DS_MEM, STUB, "Bad DS9 LDM: %08X", address);
		});
		break;
	case DS_REGION_RAM:
		LDM_LOOP(if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_32(value, address & (DS9_SIZE_DTCM - 1), memory->dtcm);
		} else if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			LOAD_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 LDM: %08X", address);
		});
		break;
	case DS_REGION_IO:
		LDM_LOOP(value = DS9IORead32(ds, address));
		break;
	case DS9_REGION_PALETTE_RAM:
		LDM_LOOP(LOAD_32(value, address & (DS9_SIZE_PALETTE_RAM - 1), ds->video.palette));
		break;
	case DS_REGION_VRAM:
		LDM_LOOP(unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		value = 0;
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				uint32_t newValue;
				LOAD_32(newValue, address & _vramMask[i], memory->vramBank[i]);
				value |= newValue;
			}
		});
		break;
	case DS9_REGION_OAM:
		LDM_LOOP(LOAD_32(value, address & (DS9_SIZE_OAM - 1), ds->video.oam.raw));
		break;
	default:
		LDM_LOOP(if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			LOAD_32(value, address & (DS9_SIZE_DTCM - 1), memory->dtcm);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 LDM: %08X", address);
		});
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
	char* ws32 = ds->ds9.memory.waitstatesNonseq32;
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
		STM_LOOP(if (address < memory->itcmSize) {
			STORE_32(value, address & (DS9_SIZE_ITCM - 1), memory->itcm);
		} else {
			mLOG(DS_MEM, STUB, "Bad DS9 STM: %08X:%08X", address, value);
		});
		break;
	case DS_REGION_WORKING_RAM:
		STM_LOOP(if (ds->memory.wramSize9) {
			STORE_32(value, address & (ds->memory.wramSize9 - 4), memory->wramBase9);
		} else {
			mLOG(DS_MEM, STUB, "Bad DS9 STM: %08X", address);
		});
		break;
	case DS_REGION_RAM:
		STM_LOOP(if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_32(value, address & (DS9_SIZE_DTCM - 1), memory->dtcm);
		} else if ((address & (DS_SIZE_RAM - 1)) < DS_SIZE_RAM) {
			STORE_32(value, address & (DS_SIZE_RAM - 1), memory->ram);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		});
		break;
	case DS_REGION_IO:
		STM_LOOP(DS9IOWrite32(ds, address & DS_OFFSET_MASK, value));
		break;
	case DS9_REGION_PALETTE_RAM:
		STM_LOOP(STORE_32(value, address & (DS9_SIZE_PALETTE_RAM - 1), ds->video.palette);
		ds->video.renderer->writePalette(ds->video.renderer, (address & (DS9_SIZE_PALETTE_RAM - 4)) + 2, value >> 16);
		ds->video.renderer->writePalette(ds->video.renderer, address & (DS9_SIZE_PALETTE_RAM - 4), value));
		break;
	case DS_REGION_VRAM:
		STM_LOOP(unsigned mask = _selectVRAM(memory, address >> DS_VRAM_OFFSET);
		int i = 0;
		for (i = 0; i < 9; ++i) {
			if (mask & (1 << i)) {
				STORE_32(value, address & _vramMask[i], memory->vramBank[i]);
			}
		});
		break;
	case DS9_REGION_OAM:
		STM_LOOP(STORE_32(value, address & (DS9_SIZE_OAM - 1), ds->video.oam.raw);
		ds->video.renderer->writeOAM(ds->video.renderer, (address & (DS9_SIZE_OAM - 4)) >> 1);
		ds->video.renderer->writeOAM(ds->video.renderer, ((address & (DS9_SIZE_OAM - 4)) >> 1) + 1));
		break;
	default:
		STM_LOOP(if ((address & ~(DS9_SIZE_DTCM - 1)) == memory->dtcmBase) {
			STORE_32(value, address & (DS9_SIZE_DTCM - 1), memory->dtcm);
		} else {
			mLOG(DS_MEM, STUB, "Unimplemented DS9 STM: %08X", address);
		});
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

void DSConfigureWRAM(struct DSMemory* memory, uint8_t config) {
	switch (config & 3) {
	case 0:
		memory->wramSize7 = 0;
		memory->wramBase7 = NULL;
		memory->wramSize9 = DS_SIZE_WORKING_RAM;
		memory->wramBase9 = memory->wramBase;
		break;
	case 1:
		memory->wramSize7 = DS_SIZE_WORKING_RAM >> 1;
		memory->wramBase7 = memory->wram;
		memory->wramSize9 = DS_SIZE_WORKING_RAM >> 1;
		memory->wramBase9 = &memory->wramBase[DS_SIZE_WORKING_RAM >> 3];
		break;
	case 2:
		memory->wramSize7 = DS_SIZE_WORKING_RAM >> 1;
		memory->wramBase7 = &memory->wram[DS_SIZE_WORKING_RAM >> 3];
		memory->wramSize9 = DS_SIZE_WORKING_RAM >> 1;
		memory->wramBase9 = memory->wramBase;
		break;
	case 3:
		memory->wramSize7 = DS_SIZE_WORKING_RAM;
		memory->wramBase7 = memory->wramBase;
		memory->wramSize9 = 0;
		memory->wramBase9 = NULL;
		break;
	}
}

void DSConfigureExternalMemory(struct DS* ds, uint16_t config) {
	// TODO: GBA params
	ds->memory.slot1Owner = config & 0x0800;
	ds->memory.slot2Owner = config & 0x0080;
	ds->memory.io7[DS7_REG_EXMEMSTAT >> 1] = config;

	ds->ds7.memory.slot1Access = ds->memory.slot1Owner;
	ds->ds9.memory.slot1Access = !ds->memory.slot1Owner;
}

static unsigned _selectVRAM(struct DSMemory* memory, uint32_t offset) {
	unsigned mask = 0;
	offset &= 0x3FF;
	mask |= memory->vramMirror[0][offset & 0x3F] & memory->vramMode[0][offset >> 7];
	mask |= memory->vramMirror[1][offset & 0x3F] & memory->vramMode[1][offset >> 7];
	mask |= memory->vramMirror[2][offset & 0x3F] & memory->vramMode[2][offset >> 7];
	mask |= memory->vramMirror[3][offset & 0x3F] & memory->vramMode[3][offset >> 7];
	mask |= memory->vramMirror[4][offset & 0x3F] & memory->vramMode[4][offset >> 7];
	mask |= memory->vramMirror[5][offset & 0x3F] & memory->vramMode[5][offset >> 7];
	mask |= memory->vramMirror[6][offset & 0x3F] & memory->vramMode[6][offset >> 7];
	mask |= memory->vramMirror[7][offset & 0x3F] & memory->vramMode[7][offset >> 7];
	mask |= memory->vramMirror[8][offset & 0x3F] & memory->vramMode[8][offset >> 7];
	return mask;
}
