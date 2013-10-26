#include "gba-memory.h"

#include "gba-gpio.h"
#include "gba-io.h"
#include "hle-bios.h"

#include <limits.h>
#include <string.h>
#include <sys/mman.h>

static void GBASetActiveRegion(struct ARMMemory* memory, uint32_t region);
static int GBAWaitMultiple(struct ARMMemory* memory, uint32_t startAddress, int count);

static const char GBA_BASE_WAITSTATES[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4 };
static const char GBA_BASE_WAITSTATES_32[16] = { 0, 0, 5, 0, 0, 0, 0, 0, 7, 7, 9, 9, 13, 13, 9 };
static const char GBA_BASE_WAITSTATES_SEQ[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4 };
static const char GBA_BASE_WAITSTATES_SEQ_32[16] = { 0, 0, 5, 0, 0, 0, 0, 0, 5, 5, 9, 9, 17, 17, 9 };
static const char GBA_ROM_WAITSTATES[] = { 4, 3, 2, 8 };
static const char GBA_ROM_WAITSTATES_SEQ[] = { 2, 1, 4, 1, 8, 1 };
static const int DMA_OFFSET[] = { 1, -1, 0, 1 };

void GBAMemoryInit(struct GBAMemory* memory) {
	memory->d.load32 = GBALoad32;
	memory->d.load16 = GBALoad16;
	memory->d.loadU16 = GBALoadU16;
	memory->d.load8 = GBALoad8;
	memory->d.loadU8 = GBALoadU8;
	memory->d.store32 = GBAStore32;
	memory->d.store16 = GBAStore16;
	memory->d.store8 = GBAStore8;

	memory->bios = (uint32_t*) hleBios;
	memory->fullBios = 0;
	memory->wram = mmap(0, SIZE_WORKING_RAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->iwram = mmap(0, SIZE_WORKING_IRAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memory->rom = 0;
	memory->gpio.p = memory->p;
	memset(memory->io, 0, sizeof(memory->io));
	memset(memory->dma, 0, sizeof(memory->dma));

	if (!memory->wram || !memory->iwram) {
		GBAMemoryDeinit(memory);
		GBALog(memory->p, GBA_LOG_ERROR, "Could not map memory");
		return;
	}

	int i;
	for (i = 0; i < 16; ++i) {
		memory->waitstates16[i] = GBA_BASE_WAITSTATES[i];
		memory->waitstatesSeq16[i] = GBA_BASE_WAITSTATES_SEQ[i];
		memory->waitstatesPrefetch16[i] = GBA_BASE_WAITSTATES_SEQ[i];
		memory->waitstates32[i] = GBA_BASE_WAITSTATES_32[i];
		memory->waitstatesSeq32[i] = GBA_BASE_WAITSTATES_SEQ_32[i];
		memory->waitstatesPrefetch32[i] = GBA_BASE_WAITSTATES_SEQ_32[i];
	}
	for (; i < 256; ++i) {
		memory->waitstates16[i] = 0;
		memory->waitstatesSeq16[i] = 0;
		memory->waitstatesPrefetch16[i] = 0;
		memory->waitstates32[i] = 0;
		memory->waitstatesSeq32[i] = 0;
		memory->waitstatesPrefetch32[i] = 0;
	}

	memory->activeRegion = 0;
	memory->d.activeRegion = 0;
	memory->d.activeMask = 0;
	memory->d.setActiveRegion = GBASetActiveRegion;
	memory->d.activePrefetchCycles32 = 0;
	memory->d.activePrefetchCycles16 = 0;
	memory->biosPrefetch = 0;
	memory->d.waitMultiple = GBAWaitMultiple;
}

void GBAMemoryDeinit(struct GBAMemory* memory) {
	munmap(memory->wram, SIZE_WORKING_RAM);
	munmap(memory->iwram, SIZE_WORKING_IRAM);
	GBASavedataDeinit(&memory->savedata);
}

static void GBASetActiveRegion(struct ARMMemory* memory, uint32_t address) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;

	if (gbaMemory->activeRegion == REGION_BIOS) {
		gbaMemory->biosPrefetch = memory->load32(memory, gbaMemory->p->cpu.currentPC + WORD_SIZE_ARM * 2, 0);
	}
	gbaMemory->activeRegion = address >> BASE_OFFSET;
	memory->activePrefetchCycles32 = gbaMemory->waitstatesPrefetch32[gbaMemory->activeRegion];
	memory->activePrefetchCycles16 = gbaMemory->waitstatesPrefetch16[gbaMemory->activeRegion];
	memory->activeNonseqCycles32 = gbaMemory->waitstates32[gbaMemory->activeRegion];
	memory->activeNonseqCycles16 = gbaMemory->waitstates16[gbaMemory->activeRegion];
	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		memory->activeRegion = gbaMemory->bios;
		memory->activeMask = SIZE_BIOS - 1;
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

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		if (gbaMemory->p->cpu.currentPC >> BASE_OFFSET == REGION_BIOS) {
			if (address < SIZE_BIOS) {
				LOAD_32(value, address, gbaMemory->bios);
			} else {
				value = 0;
			}
		} else {
			value = gbaMemory->biosPrefetch;
		}
		break;
	case BASE_WORKING_RAM:
		LOAD_32(value, address & (SIZE_WORKING_RAM - 1), gbaMemory->wram);
		wait = gbaMemory->waitstates32[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		LOAD_32(value, address & (SIZE_WORKING_IRAM - 1), gbaMemory->iwram);
		break;
	case BASE_IO:
		value = GBAIORead(gbaMemory->p, (address & (SIZE_IO - 1)) & ~2) | (GBAIORead(gbaMemory->p, (address & (SIZE_IO - 1)) | 2) << 16);
		break;
	case BASE_PALETTE_RAM:
		LOAD_32(value, address & (SIZE_PALETTE_RAM - 1), gbaMemory->p->video.palette);
		break;
	case BASE_VRAM:
		LOAD_32(value, address & 0x0001FFFF, gbaMemory->p->video.renderer->vram);
		break;
	case BASE_OAM:
		LOAD_32(value, address & (SIZE_OAM - 1), gbaMemory->p->video.oam.raw);
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		wait = gbaMemory->waitstates32[address >> BASE_OFFSET];
		if ((address & (SIZE_CART0 - 1)) < gbaMemory->romSize) {
			LOAD_32(value, address & (SIZE_CART0 - 1), gbaMemory->rom);
		}
		break;
	case BASE_CART_SRAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Load32: 0x%08X", address);
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Load32: 0x%08X", address);
		if (gbaMemory->p->cpu.executionMode == MODE_ARM) {
			value = memory->load32(memory, gbaMemory->p->cpu.currentPC + WORD_SIZE_ARM * 2, 0);
		} else {
			value = memory->load16(memory, gbaMemory->p->cpu.currentPC + WORD_SIZE_THUMB * 2, 0);
			value |= value << 16;
		}
		break;
	}


	if (cycleCounter) {
		*cycleCounter += 2 + wait;
	}
	// Unaligned 32-bit loads are "rotated" so they make some semblance of sense
	int rotate = (address & 3) << 3;
	return (value >> rotate) | (value << (32 - rotate));
}

uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address, int* cycleCounter) {
	return GBALoad16(memory, address, cycleCounter);
}

int16_t GBALoad16(struct ARMMemory* memory, uint32_t address, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	uint16_t value = 0;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		if (gbaMemory->p->cpu.currentPC >> BASE_OFFSET == REGION_BIOS) {
			if (address < SIZE_BIOS) {
				LOAD_16(value, address, gbaMemory->bios);
			} else {
				value = 0;
			}
		} else {
			value = gbaMemory->biosPrefetch;
		}
		break;
	case BASE_WORKING_RAM:
		LOAD_16(value, address & (SIZE_WORKING_RAM - 1), gbaMemory->wram);
		wait = gbaMemory->waitstates16[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		LOAD_16(value, address & (SIZE_WORKING_IRAM - 1), gbaMemory->iwram);
		break;
	case BASE_IO:
		value = GBAIORead(gbaMemory->p, address & (SIZE_IO - 1));
		break;
	case BASE_PALETTE_RAM:
		LOAD_16(value, address & (SIZE_PALETTE_RAM - 1), gbaMemory->p->video.palette);
		break;
	case BASE_VRAM:
		LOAD_16(value, address & 0x0001FFFF, gbaMemory->p->video.renderer->vram);
		break;
	case BASE_OAM:
		LOAD_16(value, address & (SIZE_OAM - 1), gbaMemory->p->video.oam.raw);
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
		wait = gbaMemory->waitstates16[address >> BASE_OFFSET];
		if ((address & (SIZE_CART0 - 1)) < gbaMemory->romSize) {
			LOAD_16(value, address & (SIZE_CART0 - 1), gbaMemory->rom);
		}
		break;
	case BASE_CART2_EX:
		wait = gbaMemory->waitstates16[address >> BASE_OFFSET];
		if (gbaMemory->savedata.type == SAVEDATA_EEPROM) {
			value = GBASavedataReadEEPROM(&gbaMemory->savedata);
		} else if ((address & (SIZE_CART0 - 1)) < gbaMemory->romSize) {
			LOAD_16(value, address & (SIZE_CART0 - 1), gbaMemory->rom);
		}
		break;
	case BASE_CART_SRAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Load16: 0x%08X", address);
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Load16: 0x%08X", address);
		value = memory->load16(memory, gbaMemory->p->cpu.currentPC + (gbaMemory->p->cpu.executionMode == MODE_ARM ? WORD_SIZE_ARM : WORD_SIZE_THUMB) * 2, 0);
		break;
	}

	if (cycleCounter) {
		*cycleCounter += 2 + wait;
	}
	// Unaligned 16-bit loads are "unpredictable", but the GBA rotates them, so we have to, too.
	int rotate = (address & 1) << 3;
	return (value >> rotate) | (value << (16 - rotate));
}

uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address, int* cycleCounter) {
	return GBALoad8(memory, address, cycleCounter);
}

int8_t GBALoad8(struct ARMMemory* memory, uint32_t address, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	int8_t value = 0;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_BIOS:
		if (gbaMemory->p->cpu.currentPC >> BASE_OFFSET == REGION_BIOS) {
			if (address < SIZE_BIOS) {
				value = ((int8_t*) gbaMemory->bios)[address];
			} else {
				value = 0;
			}
		} else {
			value = gbaMemory->biosPrefetch;
		}
		break;
	case BASE_WORKING_RAM:
		value = ((int8_t*) gbaMemory->wram)[address & (SIZE_WORKING_RAM - 1)];
		wait = gbaMemory->waitstates16[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		value = ((int8_t*) gbaMemory->iwram)[address & (SIZE_WORKING_IRAM - 1)];
		break;
	case BASE_IO:
		value = (GBAIORead(gbaMemory->p, address & 0xFFFE) >> ((address & 0x0001) << 3)) & 0xFF;
		break;
	case BASE_PALETTE_RAM:
		value = ((int8_t*) gbaMemory->p->video.renderer->palette)[address & (SIZE_PALETTE_RAM - 1)];
		break;
	case BASE_VRAM:
		value = ((int8_t*) gbaMemory->p->video.renderer->vram)[address & 0x0001FFFF];
		break;
	case BASE_OAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Load8: 0x%08X", address);
		break;
	case BASE_CART0:
	case BASE_CART0_EX:
	case BASE_CART1:
	case BASE_CART1_EX:
	case BASE_CART2:
	case BASE_CART2_EX:
		wait = gbaMemory->waitstates16[address >> BASE_OFFSET];
		if ((address & (SIZE_CART0 - 1)) < gbaMemory->romSize) {
			value = ((int8_t*) gbaMemory->rom)[address & (SIZE_CART0 - 1)];
		}
		break;
	case BASE_CART_SRAM:
		wait = gbaMemory->waitstates16[address >> BASE_OFFSET];
		if (gbaMemory->savedata.type == SAVEDATA_NONE) {
			GBASavedataInitSRAM(&gbaMemory->savedata);
		}
		if (gbaMemory->savedata.type == SAVEDATA_SRAM) {
			value = gbaMemory->savedata.data[address & (SIZE_CART_SRAM - 1)];
		} else if (gbaMemory->savedata.type == SAVEDATA_FLASH512 || gbaMemory->savedata.type == SAVEDATA_FLASH1M) {
			value = GBASavedataReadFlash(&gbaMemory->savedata, address);
		}
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Load8: 0x%08x", address);
		value = memory->load16(memory, gbaMemory->p->cpu.currentPC + (gbaMemory->p->cpu.executionMode == MODE_ARM ? WORD_SIZE_ARM : WORD_SIZE_THUMB) * 2, 0) >> ((address & 1) << 3);
		break;
	}

	if (cycleCounter) {
		*cycleCounter += 2 + wait;
	}
	return value;
}

void GBAStore32(struct ARMMemory* memory, uint32_t address, int32_t value, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		STORE_32(value, address & (SIZE_WORKING_RAM - 1), gbaMemory->wram);
		wait = gbaMemory->waitstates32[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		STORE_32(value, address & (SIZE_WORKING_IRAM - 1), gbaMemory->iwram);
		break;
	case BASE_IO:
		GBAIOWrite32(gbaMemory->p, address & (SIZE_IO - 1), value);
		break;
	case BASE_PALETTE_RAM:
		STORE_32(value, address & (SIZE_PALETTE_RAM - 1), gbaMemory->p->video.palette);
		gbaMemory->p->video.renderer->writePalette(gbaMemory->p->video.renderer, (address & (SIZE_PALETTE_RAM - 1)) + 2, value >> 16);
		gbaMemory->p->video.renderer->writePalette(gbaMemory->p->video.renderer, address & (SIZE_PALETTE_RAM - 1), value);
		break;
	case BASE_VRAM:
		if ((address & OFFSET_MASK) < SIZE_VRAM - 2) {
			STORE_32(value, address & 0x0001FFFF, gbaMemory->p->video.renderer->vram);
		}
		break;
	case BASE_OAM:
		STORE_32(value, address & (SIZE_OAM - 1), gbaMemory->p->video.oam.raw);
		gbaMemory->p->video.renderer->writeOAM(gbaMemory->p->video.renderer, (address & (SIZE_OAM - 4)) >> 1);
		gbaMemory->p->video.renderer->writeOAM(gbaMemory->p->video.renderer, ((address & (SIZE_OAM - 4)) >> 1) + 1);
		break;
	case BASE_CART0:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Store32: 0x%08X", address);
		break;
	case BASE_CART_SRAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Store32: 0x%08X", address);
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Store32: 0x%08X", address);
		break;
	}

	if (cycleCounter) {
		*cycleCounter += 1 + wait;
	}
}

void GBAStore16(struct ARMMemory* memory, uint32_t address, int16_t value, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		STORE_16(value, address & (SIZE_WORKING_RAM - 1), gbaMemory->wram);
		wait = gbaMemory->waitstates16[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		STORE_16(value, address & (SIZE_WORKING_IRAM - 1), gbaMemory->iwram);
		break;
	case BASE_IO:
		GBAIOWrite(gbaMemory->p, address & (SIZE_IO - 1), value);
		break;
	case BASE_PALETTE_RAM:
		STORE_16(value, address & (SIZE_PALETTE_RAM - 1), gbaMemory->p->video.palette);
		gbaMemory->p->video.renderer->writePalette(gbaMemory->p->video.renderer, address & (SIZE_PALETTE_RAM - 1), value);
		break;
	case BASE_VRAM:
		if ((address & OFFSET_MASK) < SIZE_VRAM) {
			STORE_16(value, address & 0x0001FFFF, gbaMemory->p->video.renderer->vram);
		}
		break;
	case BASE_OAM:
		STORE_16(value, address & (SIZE_OAM - 1), gbaMemory->p->video.oam.raw);
		gbaMemory->p->video.renderer->writeOAM(gbaMemory->p->video.renderer, (address & (SIZE_OAM - 1)) >> 1);
		break;
	case BASE_CART0:
		if (IS_GPIO_REGISTER(address & 0xFFFFFF)) {
			uint32_t reg = address & 0xFFFFFF;
			GBAGPIOWrite(&gbaMemory->gpio, reg, value);
		} else {
			GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad cartridge Store16: 0x%08X", address);
		}
		break;
	case BASE_CART2_EX:
		if (gbaMemory->savedata.type == SAVEDATA_NONE) {
			GBASavedataInitEEPROM(&gbaMemory->savedata);
		}
		GBASavedataWriteEEPROM(&gbaMemory->savedata, value, 1);
		break;
	case BASE_CART_SRAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Store16: 0x%08X", address);
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Store16: 0x%08X", address);
		break;
	}

	if (cycleCounter) {
		*cycleCounter += 1 + wait;
	}
}

void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value, int* cycleCounter) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	int wait = 0;

	switch (address & ~OFFSET_MASK) {
	case BASE_WORKING_RAM:
		((int8_t*) gbaMemory->wram)[address & (SIZE_WORKING_RAM - 1)] = value;
		wait = gbaMemory->waitstates16[REGION_WORKING_RAM];
		break;
	case BASE_WORKING_IRAM:
		((int8_t*) gbaMemory->iwram)[address & (SIZE_WORKING_IRAM - 1)] = value;
		break;
	case BASE_IO:
		GBAIOWrite8(gbaMemory->p, address & (SIZE_IO - 1), value);
		break;
	case BASE_PALETTE_RAM:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Store8: 0x%08X", address);
		break;
	case BASE_VRAM:
		if (address >= 0x06018000) {
			// TODO: check BG mode
			GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Cannot Store8 to OBJ: 0x%08X", address);
			break;
		}
		((int8_t*) gbaMemory->p->video.renderer->vram)[address & 0x1FFFE] = value;
		((int8_t*) gbaMemory->p->video.renderer->vram)[(address & 0x1FFFE) | 1] = value;
		break;
	case BASE_OAM:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Cannot Store8 to OAM: 0x%08X", address);
		break;
	case BASE_CART0:
		GBALog(gbaMemory->p, GBA_LOG_STUB, "Unimplemented memory Store8: 0x%08X", address);
		break;
	case BASE_CART_SRAM:
		if (gbaMemory->savedata.type == SAVEDATA_NONE) {
			if (address == SAVEDATA_FLASH_BASE) {
				GBASavedataInitFlash(&gbaMemory->savedata);
			} else {
				GBASavedataInitSRAM(&gbaMemory->savedata);
			}
		}
		if (gbaMemory->savedata.type == SAVEDATA_FLASH512 || gbaMemory->savedata.type == SAVEDATA_FLASH1M) {
			GBASavedataWriteFlash(&gbaMemory->savedata, address, value);
		} else if (gbaMemory->savedata.type == SAVEDATA_SRAM) {
			gbaMemory->savedata.data[address & (SIZE_CART_SRAM - 1)] = value;
		}
		wait = gbaMemory->waitstates16[REGION_CART_SRAM];
		break;
	default:
		GBALog(gbaMemory->p, GBA_LOG_GAME_ERROR, "Bad memory Store8: 0x%08X", address);
		break;
	}

	if (cycleCounter) {
		*cycleCounter += 1 + wait;
	}
}

static int GBAWaitMultiple(struct ARMMemory* memory, uint32_t startAddress, int count) {
	struct GBAMemory* gbaMemory = (struct GBAMemory*) memory;
	int wait = 1 + gbaMemory->waitstates32[startAddress >> BASE_OFFSET];
	wait += (1 + gbaMemory->waitstatesSeq32[startAddress >> BASE_OFFSET]) * (count - 1);
	return wait;
}

void GBAAdjustWaitstates(struct GBAMemory* memory, uint16_t parameters) {
	int sram = parameters & 0x0003;
	int ws0 = (parameters & 0x000C) >> 2;
	int ws0seq = (parameters & 0x0010) >> 4;
	int ws1 = (parameters & 0x0060) >> 5;
	int ws1seq = (parameters & 0x0080) >> 7;
	int ws2 = (parameters & 0x0300) >> 8;
	int ws2seq = (parameters & 0x0400) >> 10;
	int prefetch = parameters & 0x4000;

	memory->waitstates16[REGION_CART_SRAM] =  GBA_ROM_WAITSTATES[sram];
	memory->waitstatesSeq16[REGION_CART_SRAM] = GBA_ROM_WAITSTATES[sram];
	memory->waitstates32[REGION_CART_SRAM] = 2 * GBA_ROM_WAITSTATES[sram] + 1;
	memory->waitstatesSeq32[REGION_CART_SRAM] = 2 * GBA_ROM_WAITSTATES[sram] + 1;

	memory->waitstates16[REGION_CART0] = memory->waitstates16[REGION_CART0_EX] = GBA_ROM_WAITSTATES[ws0];
	memory->waitstates16[REGION_CART1] = memory->waitstates16[REGION_CART1_EX] = GBA_ROM_WAITSTATES[ws1];
	memory->waitstates16[REGION_CART2] = memory->waitstates16[REGION_CART2_EX] = GBA_ROM_WAITSTATES[ws2];

	memory->waitstatesSeq16[REGION_CART0] = memory->waitstatesSeq16[REGION_CART0_EX] = GBA_ROM_WAITSTATES_SEQ[ws0seq];
	memory->waitstatesSeq16[REGION_CART1] = memory->waitstatesSeq16[REGION_CART1_EX] = GBA_ROM_WAITSTATES_SEQ[ws1seq + 2];
	memory->waitstatesSeq16[REGION_CART2] = memory->waitstatesSeq16[REGION_CART2_EX] = GBA_ROM_WAITSTATES_SEQ[ws2seq + 4];

	memory->waitstates32[REGION_CART0] = memory->waitstates32[REGION_CART0_EX] = memory->waitstates16[REGION_CART0] + 1 + memory->waitstatesSeq16[REGION_CART0];
	memory->waitstates32[REGION_CART1] = memory->waitstates32[REGION_CART1_EX] = memory->waitstates16[REGION_CART1] + 1 + memory->waitstatesSeq16[REGION_CART1];
	memory->waitstates32[REGION_CART2] = memory->waitstates32[REGION_CART2_EX] = memory->waitstates16[REGION_CART2] + 1 + memory->waitstatesSeq16[REGION_CART2];

	memory->waitstatesSeq32[REGION_CART0] = memory->waitstatesSeq32[REGION_CART0_EX] = 2 * memory->waitstatesSeq16[REGION_CART0] + 1;
	memory->waitstatesSeq32[REGION_CART1] = memory->waitstatesSeq32[REGION_CART1_EX] = 2 * memory->waitstatesSeq16[REGION_CART1] + 1;
	memory->waitstatesSeq32[REGION_CART2] = memory->waitstatesSeq32[REGION_CART2_EX] = 2 * memory->waitstatesSeq16[REGION_CART2] + 1;

	if (!prefetch) {
		memory->waitstatesPrefetch16[REGION_CART0] = memory->waitstatesPrefetch16[REGION_CART0_EX] = memory->waitstatesSeq16[REGION_CART0];
		memory->waitstatesPrefetch16[REGION_CART1] = memory->waitstatesPrefetch16[REGION_CART1_EX] = memory->waitstatesSeq16[REGION_CART1];
		memory->waitstatesPrefetch16[REGION_CART2] = memory->waitstatesPrefetch16[REGION_CART2_EX] = memory->waitstatesSeq16[REGION_CART2];

		memory->waitstatesPrefetch32[REGION_CART0] = memory->waitstatesPrefetch32[REGION_CART0_EX] = memory->waitstatesSeq32[REGION_CART0];
		memory->waitstatesPrefetch32[REGION_CART1] = memory->waitstatesPrefetch32[REGION_CART1_EX] = memory->waitstatesSeq32[REGION_CART1];
		memory->waitstatesPrefetch32[REGION_CART2] = memory->waitstatesPrefetch32[REGION_CART2_EX] = memory->waitstatesSeq32[REGION_CART2];
	} else {
		memory->waitstatesPrefetch16[REGION_CART0] = memory->waitstatesPrefetch16[REGION_CART0_EX] = 0;
		memory->waitstatesPrefetch16[REGION_CART1] = memory->waitstatesPrefetch16[REGION_CART1_EX] = 0;
		memory->waitstatesPrefetch16[REGION_CART2] = memory->waitstatesPrefetch16[REGION_CART2_EX] = 0;

		memory->waitstatesPrefetch32[REGION_CART0] = memory->waitstatesPrefetch32[REGION_CART0_EX] = 0;
		memory->waitstatesPrefetch32[REGION_CART1] = memory->waitstatesPrefetch32[REGION_CART1_EX] = 0;
		memory->waitstatesPrefetch32[REGION_CART2] = memory->waitstatesPrefetch32[REGION_CART2_EX] = 0;
	}

	memory->d.activePrefetchCycles32 = memory->waitstatesPrefetch32[memory->activeRegion];
	memory->d.activePrefetchCycles16 = memory->waitstatesPrefetch16[memory->activeRegion];
	memory->d.activeNonseqCycles32 = memory->waitstates32[memory->activeRegion];
	memory->d.activeNonseqCycles16 = memory->waitstates16[memory->activeRegion];
}

int32_t GBAMemoryProcessEvents(struct GBAMemory* memory, int32_t cycles) {
	struct GBADMA* dma;
	int32_t test = INT_MAX;

	dma = &memory->dma[0];
	dma->nextIRQ -= cycles;
	if (dma->enable && dma->doIrq && dma->nextIRQ) {
		if (dma->nextIRQ <= 0) {
			dma->nextIRQ = INT_MAX;
			GBARaiseIRQ(memory->p, IRQ_DMA0);
		} else if (dma->nextIRQ < test) {
			test = dma->nextIRQ;
		}
	}

	dma = &memory->dma[1];
	dma->nextIRQ -= cycles;
	if (dma->enable && dma->doIrq && dma->nextIRQ) {
		if (dma->nextIRQ <= 0) {
			dma->nextIRQ = INT_MAX;
			GBARaiseIRQ(memory->p, IRQ_DMA1);
		} else if (dma->nextIRQ < test) {
			test = dma->nextIRQ;
		}
	}

	dma = &memory->dma[2];
	dma->nextIRQ -= cycles;
	if (dma->enable && dma->doIrq && dma->nextIRQ) {
		if (dma->nextIRQ <= 0) {
			dma->nextIRQ = INT_MAX;
			GBARaiseIRQ(memory->p, IRQ_DMA2);
		} else if (dma->nextIRQ < test) {
			test = dma->nextIRQ;
		}
	}

	dma = &memory->dma[3];
	dma->nextIRQ -= cycles;
	if (dma->enable && dma->doIrq && dma->nextIRQ) {
		if (dma->nextIRQ <= 0) {
			dma->nextIRQ = INT_MAX;
			GBARaiseIRQ(memory->p, IRQ_DMA3);
		} else if (dma->nextIRQ < test) {
			test = dma->nextIRQ;
		}
	}

	return test;
}

void GBAMemoryWriteDMASAD(struct GBAMemory* memory, int dma, uint32_t address) {
	memory->dma[dma].source = address & 0xFFFFFFFE;
}

void GBAMemoryWriteDMADAD(struct GBAMemory* memory, int dma, uint32_t address) {
	memory->dma[dma].dest = address & 0xFFFFFFFE;
}

void GBAMemoryWriteDMACNT_LO(struct GBAMemory* memory, int dma, uint16_t count) {
	memory->dma[dma].count = count ? count : (dma == 3 ? 0x10000 : 0x4000);
}

uint16_t GBAMemoryWriteDMACNT_HI(struct GBAMemory* memory, int dma, uint16_t control) {
	struct GBADMA* currentDma = &memory->dma[dma];
	int wasEnabled = currentDma->enable;
	currentDma->packed = control;
	currentDma->nextIRQ = 0;

	if (currentDma->drq) {
		GBALog(memory->p, GBA_LOG_STUB, "DRQ not implemented");
	}

	if (!wasEnabled && currentDma->enable) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;
		currentDma->nextCount = currentDma->count;
		GBAMemoryScheduleDMA(memory, dma, currentDma);
	}
	// If the DMA has already occurred, this value might have changed since the function started
	return currentDma->packed;
};

void GBAMemoryScheduleDMA(struct GBAMemory* memory, int number, struct GBADMA* info) {
	switch (info->timing) {
	case DMA_TIMING_NOW:
		GBAMemoryServiceDMA(memory, number, info);
		break;
	case DMA_TIMING_HBLANK:
		// Handled implicitly
		break;
	case DMA_TIMING_VBLANK:
		// Handled implicitly
		break;
	case DMA_TIMING_CUSTOM:
		switch (number) {
		case 0:
			GBALog(memory->p, GBA_LOG_WARN, "Discarding invalid DMA0 scheduling");
			break;
		case 1:
		case 2:
			GBAAudioScheduleFifoDma(&memory->p->audio, number, info);
			break;
		case 3:
			//this.cpu.irq.video.scheduleVCaptureDma(dma, info);
			break;
		}
	}
}

void GBAMemoryRunHblankDMAs(struct GBAMemory* memory) {
	struct GBADMA* dma;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (dma->enable && dma->timing == DMA_TIMING_HBLANK) {
			GBAMemoryServiceDMA(memory, i, dma);
		}
	}
}

void GBAMemoryRunVblankDMAs(struct GBAMemory* memory) {
	struct GBADMA* dma;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (dma->enable && dma->timing == DMA_TIMING_VBLANK) {
			GBAMemoryServiceDMA(memory, i, dma);
		}
	}
}

void GBAMemoryServiceDMA(struct GBAMemory* memory, int number, struct GBADMA* info) {
	if (!info->enable) {
		// There was a DMA scheduled that got canceled
		return;
	}

	uint32_t width = info->width ? 4 : 2;
	int sourceOffset = DMA_OFFSET[info->srcControl] * width;
	int destOffset = DMA_OFFSET[info->dstControl] * width;
	int32_t wordsRemaining = info->nextCount;
	uint32_t source = info->nextSource;
	uint32_t dest = info->nextDest;
	uint32_t sourceRegion = source >> BASE_OFFSET;
	uint32_t destRegion = dest >> BASE_OFFSET;

	if (width == 4) {
		int32_t word;
		source &= 0xFFFFFFFC;
		dest &= 0xFFFFFFFC;
		while (wordsRemaining--) {
			word = memory->d.load32(&memory->d, source, 0);
			memory->d.store32(&memory->d, dest, word, 0);
			source += sourceOffset;
			dest += destOffset;
		}
	} else {
		uint16_t word;
		if (sourceRegion == REGION_CART2_EX && memory->savedata.type == SAVEDATA_EEPROM) {
			while (wordsRemaining--) {
				word = GBASavedataReadEEPROM(&memory->savedata);
				memory->d.store16(&memory->d, dest, word, 0);
				source += sourceOffset;
				dest += destOffset;
			}
		} else if (destRegion == REGION_CART2_EX) {
			if (memory->savedata.type == SAVEDATA_NONE) {
				GBASavedataInitEEPROM(&memory->savedata);
			}
			while (wordsRemaining) {
				word = memory->d.load16(&memory->d, source, 0);
				GBASavedataWriteEEPROM(&memory->savedata, word, wordsRemaining);
				source += sourceOffset;
				dest += destOffset;
				--wordsRemaining;
			}
		} else {
			while (wordsRemaining--) {
				word = memory->d.load16(&memory->d, source, 0);
				memory->d.store16(&memory->d, dest, word, 0);
				source += sourceOffset;
				dest += destOffset;
			}
		}
	}

	if (info->doIrq) {
		info->nextIRQ = memory->p->cpu.cycles + 2;
		info->nextIRQ += (width == 4 ? memory->waitstates32[sourceRegion] + memory->waitstates32[destRegion]
		                            : memory->waitstates16[sourceRegion] + memory->waitstates16[destRegion]);
		info->nextIRQ += (info->count - 1) * (width == 4 ? memory->waitstatesSeq32[sourceRegion] + memory->waitstatesSeq32[destRegion]
		                                               : memory->waitstatesSeq16[sourceRegion] + memory->waitstatesSeq16[destRegion]);
	}

	info->nextSource = source;
	info->nextDest = dest;
	info->nextCount = wordsRemaining;

	if (!info->repeat) {
		info->enable = 0;

		// Clear the enable bit in memory
		memory->io[(REG_DMA0CNT_HI + number * (REG_DMA1CNT_HI - REG_DMA0CNT_HI)) >> 1] &= 0x7FE0;
	} else {
		info->nextCount = info->count;
		if (info->dstControl == DMA_INCREMENT_RELOAD) {
			info->nextDest = info->dest;
		}
		GBAMemoryScheduleDMA(memory, number, info);
	}
}
