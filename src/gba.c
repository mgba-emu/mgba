#include "gba.h"

#include <sys/mman.h>

void GBAInit(struct GBA* gba) {
	ARMInit(&gba->cpu);
	GBAMemoryInit(&gba->memory);
}

void GBADeinit(struct GBA* gba) {
	GBAMemoryDeinit(&gba->memory);
}

void GBAMemoryInit(struct GBAMemory* memory) {
	memory->d.load32 = GBALoad32;
	memory->d.load16 = GBALoad16;
	memory->d.loadU16 = GBALoadU16;
	memory->d.load8 = GBALoad8;
	memory->d.loadU8 = GBALoadU8;

	memory->bios = 0;
	memory->wram = mmap(0, SIZE_WORKING_RAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->iwram = mmap(0, SIZE_WORKING_IRAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->rom = 0;
}

void GBAMemoryDeinit(struct GBAMemory* memory) {
	munmap(memory->wram, SIZE_WORKING_RAM);
	munmap(memory->iwram, SIZE_WORKING_IRAM);
}

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & OFFSET_MASK) {
	case REGION_BIOS:
		break;
	case REGION_WORKING_RAM:
		break;
	case REGION_WORKING_IRAM:
		break;
	case REGION_IO:
		break;
	case REGION_PALETTE_RAM:
		break;
	case REGION_VRAM:
		break;
	case REGION_OAM:
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		break;
	case REGION_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

int16_t GBALoad16(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & OFFSET_MASK) {
	case REGION_BIOS:
		break;
	case REGION_WORKING_RAM:
		break;
	case REGION_WORKING_IRAM:
		break;
	case REGION_IO:
		break;
	case REGION_PALETTE_RAM:
		break;
	case REGION_VRAM:
		break;
	case REGION_OAM:
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		break;
	case REGION_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & OFFSET_MASK) {
	case REGION_BIOS:
		break;
	case REGION_WORKING_RAM:
		break;
	case REGION_WORKING_IRAM:
		break;
	case REGION_IO:
		break;
	case REGION_PALETTE_RAM:
		break;
	case REGION_VRAM:
		break;
	case REGION_OAM:
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		break;
	case REGION_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

int8_t GBALoad8(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & OFFSET_MASK) {
	case REGION_BIOS:
		break;
	case REGION_WORKING_RAM:
		break;
	case REGION_WORKING_IRAM:
		break;
	case REGION_IO:
		break;
	case REGION_PALETTE_RAM:
		break;
	case REGION_VRAM:
		break;
	case REGION_OAM:
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		break;
	case REGION_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}

uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & OFFSET_MASK) {
	case REGION_BIOS:
		break;
	case REGION_WORKING_RAM:
		break;
	case REGION_WORKING_IRAM:
		break;
	case REGION_IO:
		break;
	case REGION_PALETTE_RAM:
		break;
	case REGION_VRAM:
		break;
	case REGION_OAM:
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		break;
	case REGION_CART_SRAM:
		break;
	default:
		break;
	}

	return 0;
}