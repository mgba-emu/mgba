#include "gba-memory.h"

#include "gba-io.h"

#include <string.h>
#include <sys/mman.h>

static const char* GBA_CANNOT_MMAP = "Could not map memory";

static void GBASetActiveRegion(struct ARMMemory* memory, uint32_t region);

static const char GBA_BASE_WAITSTATES[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4 };
static const char GBA_BASE_WAITSTATES_SEQ[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4 };

void GBAMemoryInit(struct GBAMemory* memory) {
	memory->d.load32 = GBALoad32;
	memory->d.load16 = GBALoad16;
	memory->d.loadU16 = GBALoadU16;
	memory->d.load8 = GBALoad8;
	memory->d.loadU8 = GBALoadU8;
	memory->d.store32 = GBAStore32;
	memory->d.store16 = GBAStore16;
	memory->d.store8 = GBAStore8;

	memory->bios = 0;
	memory->wram = mmap(0, SIZE_WORKING_RAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->iwram = mmap(0, SIZE_WORKING_IRAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->rom = 0;
	memset(memory->io, 0, sizeof(memory->io));

	if (!memory->wram || !memory->iwram) {
		GBAMemoryDeinit(memory);
		memory->p->errno = GBA_OUT_OF_MEMORY;
		memory->p->errstr = GBA_CANNOT_MMAP;
	}

	int i;
	for (i = 0; i < 16; ++i) {
		memory->waitstates16[i] = GBA_BASE_WAITSTATES[i];
		memory->waitstatesSeq16[i] = GBA_BASE_WAITSTATES_SEQ[i];
		memory->waitstates32[i] = GBA_BASE_WAITSTATES[i] + GBA_BASE_WAITSTATES_SEQ[i] + 1;
		memory->waitstatesSeq32[i] = GBA_BASE_WAITSTATES_SEQ[i] + GBA_BASE_WAITSTATES_SEQ[i] + 1;
	}
	for (; i < 256; ++i) {
		memory->waitstates16[i] = 0;
		memory->waitstatesSeq16[i] = 0;
		memory->waitstates32[i] = 0;
		memory->waitstatesSeq32[i] = 0;
	}

	memory->d.activeRegion = 0;
	memory->d.activeMask = 0;
	memory->d.setActiveRegion = GBASetActiveRegion;
}

void GBAMemoryDeinit(struct GBAMemory* memory) {
	munmap(memory->wram, SIZE_WORKING_RAM);
	munmap(memory->iwram, SIZE_WORKING_IRAM);
}

static void GBASetActiveRegion(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	memory->activePrefetchCycles32 = gbaMemory->waitstates32[address >> BASE_OFFSET];
	memory->activePrefetchCycles16 = gbaMemory->waitstates16[address >> BASE_OFFSET];
	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		memory->activeRegion = gbaMemory->bios;
		memory->activeMask = 0;
		break;
	case BASE_WORKING_RAM:
		memory->activeRegion = gbaMemory->wram;
		memory->activeMask = SIZE_WORKING_RAM - 1;
		break;
	case BASE_WORKING_IRAM:
		memory->activeRegion = gbaMemory->iwram;
		memory->activeMask = SIZE_WORKING_IRAM - 1;
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		memory->activeRegion = gbaMemory->rom;
		memory->activeMask = SIZE_CART0 - 1;
		break;
	default:
		memory->activeRegion = 0;
		memory->activeMask = 0;
		break;
	}
}

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		return gbaMemory->wram[(address & (SIZE_WORKING_RAM - 1)) >> 2];
	case BASE_WORKING_IRAM:
		return gbaMemory->iwram[(address & (SIZE_WORKING_IRAM - 1)) >> 2];
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		return gbaMemory->rom[(address & (SIZE_CART0 - 1)) >> 2];
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

int16_t GBALoad16(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		return ((int16_t*) gbaMemory->wram)[(address & (SIZE_WORKING_RAM - 1)) >> 1];
	case BASE_WORKING_IRAM:
		return ((int16_t*) gbaMemory->iwram)[(address & (SIZE_WORKING_IRAM - 1)) >> 1];
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		return ((int16_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1)) >> 1];
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		return ((uint16_t*) gbaMemory->wram)[(address & (SIZE_WORKING_RAM - 1)) >> 1];
	case BASE_WORKING_IRAM:
		return ((uint16_t*) gbaMemory->iwram)[(address & (SIZE_WORKING_IRAM - 1)) >> 1];
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		return ((uint16_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1)) >> 1];
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

int8_t GBALoad8(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		return ((int8_t*) gbaMemory->wram)[address & (SIZE_WORKING_RAM - 1)];
	case BASE_WORKING_IRAM:
		return ((int8_t*) gbaMemory->iwram)[address & (SIZE_WORKING_IRAM - 1)];
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		return ((int8_t*) gbaMemory->rom)[address & (SIZE_CART0 - 1)];
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		return ((uint8_t*) gbaMemory->wram)[address & (SIZE_WORKING_RAM - 1)];
		break;
	case BASE_WORKING_IRAM:
		return ((uint8_t*) gbaMemory->iwram)[address & (SIZE_WORKING_IRAM - 1)];
		break;
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		return ((uint8_t*) gbaMemory->rom)[address & (SIZE_CART0 - 1)];
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

void GBAStore32(struct ARMMemory* memory, uint32_t address, int32_t value) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		gbaMemory->wram[(address & (SIZE_WORKING_RAM - 1)) >> 2] = value;
		break;
	case BASE_WORKING_IRAM:
		gbaMemory->iwram[(address & (SIZE_WORKING_IRAM - 1)) >> 2] = value;
		break;
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
		break;
	case BASE_CART2_EX:
		break;
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}
}

void GBAStore16(struct ARMMemory* memory, uint32_t address, int16_t value) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		((int16_t*) gbaMemory->wram)[(address & (SIZE_WORKING_RAM - 1)) >> 1] = value;
		break;
	case BASE_WORKING_IRAM:
		((int16_t*) gbaMemory->iwram)[(address & (SIZE_WORKING_IRAM - 1)) >> 1] = value;
		break;
	case BASE_IO:
		GBAIOWrite(gbaMemory->p, address & (SIZE_IO - 1), value);
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
		break;
	case BASE_CART2_EX:
		break;
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}
}

void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		((int8_t*) gbaMemory->wram)[address & (SIZE_WORKING_RAM - 1)] = value;
		break;
	case BASE_WORKING_IRAM:
		((int8_t*) gbaMemory->iwram)[address & (SIZE_WORKING_IRAM - 1)] = value;
		break;
	case BASE_IO:
		break;
	case BASE_PALETTE_RAM:
		break;
	case BASE_VRAM:
		break;
	case BASE_OAM:
		break;
	case BASE_CART0:
		break;
	case BASE_CART2_EX:
		break;
	case BASE_CART_SRAM:
		break;
	default:
		break;
	}
}
