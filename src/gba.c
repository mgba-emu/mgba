#include "gba.h"

#include <sys/mman.h>
#include <unistd.h>

static const char* GBA_CANNOT_MMAP = "Could not map memory";

void GBAInit(struct GBA* gba) {
	gba->errno = GBA_NO_ERROR;
	gba->errstr = 0;

	ARMInit(&gba->cpu);

	gba->memory.p = gba;
	GBAMemoryInit(&gba->memory);
	ARMAssociateMemory(&gba->cpu, &gba->memory.d);

	GBABoardInit(&gba->board);
	ARMAssociateBoard(&gba->cpu, &gba->board.d);

	ARMReset(&gba->cpu);
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
	memory->d.store32 = GBAStore32;
	memory->d.store16 = GBAStore16;
	memory->d.store8 = GBAStore8;

	memory->bios = 0;
	memory->wram = mmap(0, SIZE_WORKING_RAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->iwram = mmap(0, SIZE_WORKING_IRAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->rom = 0;

	if (!memory->wram || !memory->iwram) {
		GBAMemoryDeinit(memory);
		memory->p->errno = GBA_OUT_OF_MEMORY;
		memory->p->errstr = GBA_CANNOT_MMAP;
	}
}

void GBAMemoryDeinit(struct GBAMemory* memory) {
	munmap(memory->wram, SIZE_WORKING_RAM);
	munmap(memory->iwram, SIZE_WORKING_IRAM);
}

void GBABoardInit(struct GBABoard* board) {
	board->d.reset = GBABoardReset;
}

void GBABoardReset(struct ARMBoard* board) {
	struct ARMCore* cpu = board->cpu;
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;
}

void GBALoadROM(struct GBA* gba, int fd) {
	gba->memory.rom = mmap(0, SIZE_CART0, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FILE, fd, 0);
	// TODO: error check
}

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		break;
	case BASE_WORKING_RAM:
		break;
	case BASE_WORKING_IRAM:
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
		break;
	case BASE_WORKING_IRAM:
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
		return ((int16_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1)) >> 1];
		break;
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
		break;
	case BASE_WORKING_IRAM:
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
		return ((uint16_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1)) >> 1];
		break;
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
		break;
	case BASE_WORKING_IRAM:
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
		return ((int8_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1))];
		break;
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
		break;
	case BASE_WORKING_IRAM:
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
		return ((uint8_t*) gbaMemory->rom)[(address & (SIZE_CART0 - 1))];
		break;
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
		break;
	case BASE_WORKING_IRAM:
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
		break;
	case BASE_WORKING_IRAM:
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

void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		break;
	case BASE_WORKING_IRAM:
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
