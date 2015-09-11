/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "macros.h"

#include "decoder.h"
#include "gba/hardware.h"
#include "gba/io.h"
#include "gba/serialize.h"
#include "gba/hle-bios.h"
#include "util/math.h"
#include "util/memory.h"

#define IDLE_LOOP_THRESHOLD 10000

static void _pristineCow(struct GBA* gba);
static uint32_t _deadbeef[1] = { 0xE710B710 }; // Illegal instruction on both ARM and Thumb

static void GBASetActiveRegion(struct ARMCore* cpu, uint32_t region);
static void GBAMemoryServiceDMA(struct GBA* gba, int number, struct GBADMA* info);
static int32_t GBAMemoryStall(struct ARMCore* cpu, int32_t wait);

static const char GBA_BASE_WAITSTATES[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4 };
static const char GBA_BASE_WAITSTATES_32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 9 };
static const char GBA_BASE_WAITSTATES_SEQ[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4 };
static const char GBA_BASE_WAITSTATES_SEQ_32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 9 };
static const char GBA_ROM_WAITSTATES[] = { 4, 3, 2, 8 };
static const char GBA_ROM_WAITSTATES_SEQ[] = { 2, 1, 4, 1, 8, 1 };
static const int DMA_OFFSET[] = { 1, -1, 0, 1 };

void GBAMemoryInit(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	cpu->memory.load32 = GBALoad32;
	cpu->memory.load16 = GBALoad16;
	cpu->memory.load8 = GBALoad8;
	cpu->memory.loadMultiple = GBALoadMultiple;
	cpu->memory.store32 = GBAStore32;
	cpu->memory.store16 = GBAStore16;
	cpu->memory.store8 = GBAStore8;
	cpu->memory.storeMultiple = GBAStoreMultiple;
	cpu->memory.stall = GBAMemoryStall;

	gba->memory.bios = (uint32_t*) hleBios;
	gba->memory.fullBios = 0;
	gba->memory.wram = 0;
	gba->memory.iwram = 0;
	gba->memory.rom = 0;
	gba->memory.romSize = 0;
	gba->memory.romMask = 0;
	gba->memory.hw.p = gba;

	int i;
	for (i = 0; i < 16; ++i) {
		gba->memory.waitstatesNonseq16[i] = GBA_BASE_WAITSTATES[i];
		gba->memory.waitstatesSeq16[i] = GBA_BASE_WAITSTATES_SEQ[i];
		gba->memory.waitstatesPrefetchNonseq16[i] = GBA_BASE_WAITSTATES[i];
		gba->memory.waitstatesPrefetchSeq16[i] = GBA_BASE_WAITSTATES_SEQ[i];
		gba->memory.waitstatesNonseq32[i] = GBA_BASE_WAITSTATES_32[i];
		gba->memory.waitstatesSeq32[i] = GBA_BASE_WAITSTATES_SEQ_32[i];
		gba->memory.waitstatesPrefetchNonseq32[i] = GBA_BASE_WAITSTATES_32[i];
		gba->memory.waitstatesPrefetchSeq32[i] = GBA_BASE_WAITSTATES_SEQ_32[i];
	}
	for (; i < 256; ++i) {
		gba->memory.waitstatesNonseq16[i] = 0;
		gba->memory.waitstatesSeq16[i] = 0;
		gba->memory.waitstatesNonseq32[i] = 0;
		gba->memory.waitstatesSeq32[i] = 0;
	}

	gba->memory.activeRegion = -1;
	cpu->memory.activeRegion = 0;
	cpu->memory.activeMask = 0;
	cpu->memory.setActiveRegion = GBASetActiveRegion;
	cpu->memory.activeSeqCycles32 = 0;
	cpu->memory.activeSeqCycles16 = 0;
	cpu->memory.activeNonseqCycles32 = 0;
	cpu->memory.activeNonseqCycles16 = 0;
	gba->memory.biosPrefetch = 0;
}

void GBAMemoryDeinit(struct GBA* gba) {
	mappedMemoryFree(gba->memory.wram, SIZE_WORKING_RAM);
	mappedMemoryFree(gba->memory.iwram, SIZE_WORKING_IRAM);
	if (gba->memory.rom) {
		mappedMemoryFree(gba->memory.rom, gba->memory.romSize);
	}
	GBASavedataDeinit(&gba->memory.savedata);
}

void GBAMemoryReset(struct GBA* gba) {
	if (gba->memory.wram) {
		mappedMemoryFree(gba->memory.wram, SIZE_WORKING_RAM);
	}
	gba->memory.wram = anonymousMemoryMap(SIZE_WORKING_RAM);

	if (gba->memory.iwram) {
		mappedMemoryFree(gba->memory.iwram, SIZE_WORKING_IRAM);
	}
	gba->memory.iwram = anonymousMemoryMap(SIZE_WORKING_IRAM);

	memset(gba->memory.io, 0, sizeof(gba->memory.io));
	memset(gba->memory.dma, 0, sizeof(gba->memory.dma));
	int i;
	for (i = 0; i < 4; ++i) {
		gba->memory.dma[i].count = 0x4000;
		gba->memory.dma[i].nextEvent = INT_MAX;
	}
	gba->memory.dma[3].count = 0x10000;
	gba->memory.activeDMA = -1;
	gba->memory.nextDMA = INT_MAX;
	gba->memory.eventDiff = 0;

	gba->memory.prefetch = false;
	gba->memory.lastPrefetchedPc = 0;

	if (!gba->memory.wram || !gba->memory.iwram) {
		GBAMemoryDeinit(gba);
		GBALog(gba, GBA_LOG_FATAL, "Could not map memory");
	}
}

static void _analyzeForIdleLoop(struct GBA* gba, struct ARMCore* cpu, uint32_t address) {
	struct ARMInstructionInfo info;
	uint32_t nextAddress = address;
	memset(gba->taintedRegisters, 0, sizeof(gba->taintedRegisters));
	if (cpu->executionMode == MODE_THUMB) {
		while (true) {
			uint16_t opcode;
			LOAD_16(opcode, nextAddress & cpu->memory.activeMask, cpu->memory.activeRegion);
			ARMDecodeThumb(opcode, &info);
			switch (info.branchType) {
			case ARM_BRANCH_NONE:
				if (info.operandFormat & ARM_OPERAND_MEMORY_2) {
					if (info.mnemonic == ARM_MN_STR || gba->taintedRegisters[info.memory.baseReg]) {
						gba->idleDetectionStep = -1;
						return;
					}
					uint32_t loadAddress = gba->cachedRegisters[info.memory.baseReg];
					uint32_t offset = 0;
					if (info.memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
						offset = info.memory.offset.immediate;
					} else if (info.memory.format & ARM_MEMORY_REGISTER_OFFSET) {
						int reg = info.memory.offset.reg;
						if (gba->cachedRegisters[reg]) {
							gba->idleDetectionStep = -1;
							return;
						}
						offset = gba->cachedRegisters[reg];
					}
					if (info.memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
						loadAddress -= offset;
					} else {
						loadAddress += offset;
					}
					if ((loadAddress >> BASE_OFFSET) == REGION_IO && !GBAIOIsReadConstant(loadAddress)) {
						gba->idleDetectionStep = -1;
						return;
					}
					if ((loadAddress >> BASE_OFFSET) < REGION_CART0 || (loadAddress >> BASE_OFFSET) > REGION_CART2_EX) {
						gba->taintedRegisters[info.op1.reg] = true;
					} else {
						switch (info.memory.width) {
						case 1:
							gba->cachedRegisters[info.op1.reg] = GBALoad8(cpu, loadAddress, 0);
							break;
						case 2:
							gba->cachedRegisters[info.op1.reg] = GBALoad16(cpu, loadAddress, 0);
							break;
						case 4:
							gba->cachedRegisters[info.op1.reg] = GBALoad32(cpu, loadAddress, 0);
							break;
						}
					}
				} else if (info.operandFormat & ARM_OPERAND_AFFECTED_1) {
					gba->taintedRegisters[info.op1.reg] = true;
				}
				nextAddress += WORD_SIZE_THUMB;
				break;
			case ARM_BRANCH:
				if ((uint32_t) info.op1.immediate + nextAddress + WORD_SIZE_THUMB * 2 == address) {
					gba->idleLoop = address;
					gba->idleOptimization = IDLE_LOOP_REMOVE;
				}
				gba->idleDetectionStep = -1;
				return;
			default:
				gba->idleDetectionStep = -1;
				return;
			}
		}
	} else {
		gba->idleDetectionStep = -1;
	}
}

static void GBASetActiveRegion(struct ARMCore* cpu, uint32_t address) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;

	int newRegion = address >> BASE_OFFSET;
	if (gba->idleOptimization >= IDLE_LOOP_REMOVE && memory->activeRegion != REGION_BIOS) {
		if (address == gba->idleLoop) {
			if (gba->haltPending) {
				gba->haltPending = false;
				GBAHalt(gba);
			} else {
				gba->haltPending = true;
			}
		} else if (gba->idleOptimization >= IDLE_LOOP_DETECT && newRegion == memory->activeRegion) {
			if (address == gba->lastJump) {
				switch (gba->idleDetectionStep) {
				case 0:
					memcpy(gba->cachedRegisters, cpu->gprs, sizeof(gba->cachedRegisters));
					++gba->idleDetectionStep;
					break;
				case 1:
					if (memcmp(gba->cachedRegisters, cpu->gprs, sizeof(gba->cachedRegisters))) {
						gba->idleDetectionStep = -1;
						++gba->idleDetectionFailures;
						if (gba->idleDetectionFailures > IDLE_LOOP_THRESHOLD) {
							gba->idleOptimization = IDLE_LOOP_IGNORE;
						}
						break;
					}
					_analyzeForIdleLoop(gba, cpu, address);
					break;
				}
			} else {
				gba->idleDetectionStep = 0;
			}
		}
	}

	gba->lastJump = address;
	memory->lastPrefetchedPc = 0;
	memory->lastPrefetchedLoads = 0;
	if (newRegion == memory->activeRegion && (newRegion < REGION_CART0 || (address & (SIZE_CART0 - 1)) < memory->romSize)) {
		return;
	}

	if (memory->activeRegion == REGION_BIOS) {
		memory->biosPrefetch = cpu->prefetch[1];
	}
	memory->activeRegion = newRegion;
	switch (newRegion) {
	case REGION_BIOS:
		cpu->memory.activeRegion = memory->bios;
		cpu->memory.activeMask = SIZE_BIOS - 1;
		break;
	case REGION_WORKING_RAM:
		cpu->memory.activeRegion = memory->wram;
		cpu->memory.activeMask = SIZE_WORKING_RAM - 1;
		break;
	case REGION_WORKING_IRAM:
		cpu->memory.activeRegion = memory->iwram;
		cpu->memory.activeMask = SIZE_WORKING_IRAM - 1;
		break;
	case REGION_VRAM:
		cpu->memory.activeRegion = (uint32_t*) gba->video.renderer->vram;
		cpu->memory.activeMask = 0x0000FFFF;
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		cpu->memory.activeRegion = memory->rom;
		cpu->memory.activeMask = memory->romMask;
		if ((address & (SIZE_CART0 - 1)) < memory->romSize) {
			break;
		}
	// Fall through
	default:
		memory->activeRegion = -1;
		cpu->memory.activeRegion = _deadbeef;
		cpu->memory.activeMask = 0;
		enum GBALogLevel errorLevel = GBA_LOG_FATAL;
		if (gba->yankedRomSize || !gba->hardCrash) {
			errorLevel = GBA_LOG_GAME_ERROR;
		}
		GBALog(gba, errorLevel, "Jumped to invalid address: %08X", address);
		return;
	}
	cpu->memory.activeSeqCycles32 = memory->waitstatesSeq32[memory->activeRegion];
	cpu->memory.activeSeqCycles16 = memory->waitstatesSeq16[memory->activeRegion];
	cpu->memory.activeNonseqCycles32 = memory->waitstatesNonseq32[memory->activeRegion];
	cpu->memory.activeNonseqCycles16 = memory->waitstatesNonseq16[memory->activeRegion];
}

#define LOAD_BAD \
	if (gba->performingDMA) { \
		value = gba->bus; \
	} else { \
		value = cpu->prefetch[1]; \
		if (cpu->executionMode == MODE_THUMB) { \
			/* http://ngemu.com/threads/gba-open-bus.170809/ */ \
			switch (cpu->gprs[ARM_PC] >> BASE_OFFSET) { \
			case REGION_BIOS: \
			case REGION_OAM: \
				/* This isn't right half the time, but we don't have $+6 handy */ \
				value <<= 16; \
				value |= cpu->prefetch[0]; \
				break; \
			case REGION_WORKING_IRAM: \
				/* This doesn't handle prefetch clobbering */ \
				if (cpu->gprs[ARM_PC] & 2) { \
					value |= cpu->prefetch[0] << 16; \
				} else { \
					value <<= 16; \
					value |= cpu->prefetch[0]; \
				} \
			default: \
				value |= value << 16; \
			} \
		} \
	}

#define LOAD_BIOS \
	if (address < SIZE_BIOS) { \
		if (memory->activeRegion == REGION_BIOS) { \
			LOAD_32(value, address, memory->bios); \
		} else { \
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad BIOS Load32: 0x%08X", address); \
			value = memory->biosPrefetch; \
		} \
	} else { \
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load32: 0x%08X", address); \
		LOAD_BAD; \
	}

#define LOAD_WORKING_RAM \
	LOAD_32(value, address & (SIZE_WORKING_RAM - 4), memory->wram); \
	wait += waitstatesRegion[REGION_WORKING_RAM];

#define LOAD_WORKING_IRAM LOAD_32(value, address & (SIZE_WORKING_IRAM - 4), memory->iwram);
#define LOAD_IO value = GBAIORead(gba, (address & (SIZE_IO - 1)) & ~2) | (GBAIORead(gba, (address & (SIZE_IO - 1)) | 2) << 16);

#define LOAD_PALETTE_RAM \
	LOAD_32(value, address & (SIZE_PALETTE_RAM - 4), gba->video.palette); \
	wait += waitstatesRegion[REGION_PALETTE_RAM];

#define LOAD_VRAM \
	if ((address & 0x0001FFFF) < SIZE_VRAM) { \
		LOAD_32(value, address & 0x0001FFFC, gba->video.renderer->vram); \
	} else { \
		LOAD_32(value, address & 0x00017FFC, gba->video.renderer->vram); \
	} \
	wait += waitstatesRegion[REGION_VRAM];

#define LOAD_OAM LOAD_32(value, address & (SIZE_OAM - 4), gba->video.oam.raw);

#define LOAD_CART \
	wait += waitstatesRegion[address >> BASE_OFFSET]; \
	if ((address & (SIZE_CART0 - 1)) < memory->romSize) { \
		LOAD_32(value, address & (SIZE_CART0 - 4), memory->rom); \
	} else { \
		GBALog(gba, GBA_LOG_GAME_ERROR, "Out of bounds ROM Load32: 0x%08X", address); \
		value = (address >> 1) & 0xFFFF; \
		value |= ((address + 2) >> 1) << 16; \
	}

#define LOAD_SRAM \
	wait = memory->waitstatesNonseq16[address >> BASE_OFFSET]; \
	value = GBALoad8(cpu, address, 0); \
	value |= value << 8; \
	value |= value << 16;

uint32_t GBALoad32(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	uint32_t value = 0;
	int wait = 0;
	char* waitstatesRegion = memory->waitstatesNonseq32;

	switch (address >> BASE_OFFSET) {
	case REGION_BIOS:
		LOAD_BIOS;
		break;
	case REGION_WORKING_RAM:
		LOAD_WORKING_RAM;
		break;
	case REGION_WORKING_IRAM:
		LOAD_WORKING_IRAM;
		break;
	case REGION_IO:
		LOAD_IO;
		break;
	case REGION_PALETTE_RAM:
		LOAD_PALETTE_RAM;
		break;
	case REGION_VRAM:
		LOAD_VRAM;
		break;
	case REGION_OAM:
		LOAD_OAM;
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		LOAD_CART;
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		LOAD_SRAM;
		break;
	default:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load32: 0x%08X", address);
		LOAD_BAD;
		break;
	}

	if (cycleCounter) {
		wait += 2;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
	// Unaligned 32-bit loads are "rotated" so they make some semblance of sense
	int rotate = (address & 3) << 3;
	return ROR(value, rotate);
}

uint32_t GBALoad16(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> BASE_OFFSET) {
	case REGION_BIOS:
		if (address < SIZE_BIOS) {
			if (memory->activeRegion == REGION_BIOS) {
				LOAD_16(value, address, memory->bios);
			} else {
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad BIOS Load16: 0x%08X", address);
				LOAD_16(value, address & 2, &memory->biosPrefetch);
			}
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load16: 0x%08X", address);
			LOAD_BAD;
			value = (value >> ((address & 2) * 8)) & 0xFFFF;
		}
		break;
	case REGION_WORKING_RAM:
		LOAD_16(value, address & (SIZE_WORKING_RAM - 2), memory->wram);
		wait = memory->waitstatesNonseq16[REGION_WORKING_RAM];
		break;
	case REGION_WORKING_IRAM:
		LOAD_16(value, address & (SIZE_WORKING_IRAM - 2), memory->iwram);
		break;
	case REGION_IO:
		value = GBAIORead(gba, address & (SIZE_IO - 2));
		break;
	case REGION_PALETTE_RAM:
		LOAD_16(value, address & (SIZE_PALETTE_RAM - 2), gba->video.palette);
		break;
	case REGION_VRAM:
		if ((address & 0x0001FFFF) < SIZE_VRAM) {
			LOAD_16(value, address & 0x0001FFFE, gba->video.renderer->vram);
		} else {
			LOAD_16(value, address & 0x00017FFE, gba->video.renderer->vram);
		}
		break;
	case REGION_OAM:
		LOAD_16(value, address & (SIZE_OAM - 2), gba->video.oam.raw);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
		wait = memory->waitstatesNonseq16[address >> BASE_OFFSET];
		if ((address & (SIZE_CART0 - 1)) < memory->romSize) {
			LOAD_16(value, address & (SIZE_CART0 - 2), memory->rom);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Out of bounds ROM Load16: 0x%08X", address);
			value = (address >> 1) & 0xFFFF;
		}
		break;
	case REGION_CART2_EX:
		wait = memory->waitstatesNonseq16[address >> BASE_OFFSET];
		if (memory->savedata.type == SAVEDATA_EEPROM) {
			value = GBASavedataReadEEPROM(&memory->savedata);
		} else if ((address & (SIZE_CART0 - 1)) < memory->romSize) {
			LOAD_16(value, address & (SIZE_CART0 - 2), memory->rom);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Out of bounds ROM Load16: 0x%08X", address);
			value = (address >> 1) & 0xFFFF;
		}
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		wait = memory->waitstatesNonseq16[address >> BASE_OFFSET];
		value = GBALoad8(cpu, address, 0);
		value |= value << 8;
		break;
	default:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load16: 0x%08X", address);
		LOAD_BAD;
		value = (value >> ((address & 2) * 8)) & 0xFFFF;
		break;
	}

	if (cycleCounter) {
		wait += 2;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
	// Unaligned 16-bit loads are "unpredictable", but the GBA rotates them, so we have to, too.
	int rotate = (address & 1) << 3;
	return ROR(value, rotate);
}

uint32_t GBALoad8(struct ARMCore* cpu, uint32_t address, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	uint32_t value = 0;
	int wait = 0;

	switch (address >> BASE_OFFSET) {
	case REGION_BIOS:
		if (address < SIZE_BIOS) {
			if (memory->activeRegion == REGION_BIOS) {
				value = ((uint8_t*) memory->bios)[address];
			} else {
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad BIOS Load8: 0x%08X", address);
				value = ((uint8_t*) &memory->biosPrefetch)[address & 3];
			}
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load8: 0x%08x", address);
			LOAD_BAD;
			value = ((uint8_t*) &value)[address & 3];
		}
		break;
	case REGION_WORKING_RAM:
		value = ((uint8_t*) memory->wram)[address & (SIZE_WORKING_RAM - 1)];
		wait = memory->waitstatesNonseq16[REGION_WORKING_RAM];
		break;
	case REGION_WORKING_IRAM:
		value = ((uint8_t*) memory->iwram)[address & (SIZE_WORKING_IRAM - 1)];
		break;
	case REGION_IO:
		value = (GBAIORead(gba, address & 0xFFFE) >> ((address & 0x0001) << 3)) & 0xFF;
		break;
	case REGION_PALETTE_RAM:
		value = ((uint8_t*) gba->video.palette)[address & (SIZE_PALETTE_RAM - 1)];
		break;
	case REGION_VRAM:
		if ((address & 0x0001FFFF) < SIZE_VRAM) {
			value = ((uint8_t*) gba->video.renderer->vram)[address & 0x0001FFFF];
		} else {
			value = ((uint8_t*) gba->video.renderer->vram)[address & 0x00017FFF];
		}
		break;
	case REGION_OAM:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Load8: 0x%08X", address);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		wait = memory->waitstatesNonseq16[address >> BASE_OFFSET];
		if ((address & (SIZE_CART0 - 1)) < memory->romSize) {
			value = ((uint8_t*) memory->rom)[address & (SIZE_CART0 - 1)];
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Out of bounds ROM Load8: 0x%08X", address);
			value = (address >> 1) & 0xFF;
		}
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		wait = memory->waitstatesNonseq16[address >> BASE_OFFSET];
		if (memory->savedata.type == SAVEDATA_AUTODETECT) {
			GBALog(gba, GBA_LOG_INFO, "Detected SRAM savegame");
			GBASavedataInitSRAM(&memory->savedata);
		}
		if (memory->savedata.type == SAVEDATA_SRAM) {
			value = memory->savedata.data[address & (SIZE_CART_SRAM - 1)];
		} else if (memory->savedata.type == SAVEDATA_FLASH512 || memory->savedata.type == SAVEDATA_FLASH1M) {
			value = GBASavedataReadFlash(&memory->savedata, address);
		} else if (memory->hw.devices & HW_TILT) {
			value = GBAHardwareTiltRead(&memory->hw, address & OFFSET_MASK);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Reading from non-existent SRAM: 0x%08X", address);
			value = 0xFF;
		}
		value &= 0xFF;
		break;
	default:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Load8: 0x%08x", address);
		LOAD_BAD;
		value = ((uint8_t*) &value)[address & 3];
		break;
	}

	if (cycleCounter) {
		wait += 2;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
	return value;
}

#define STORE_WORKING_RAM \
	STORE_32(value, address & (SIZE_WORKING_RAM - 4), memory->wram); \
	wait += waitstatesRegion[REGION_WORKING_RAM];

#define STORE_WORKING_IRAM \
	STORE_32(value, address & (SIZE_WORKING_IRAM - 4), memory->iwram);

#define STORE_IO \
	GBAIOWrite32(gba, address & (SIZE_IO - 4), value);

#define STORE_PALETTE_RAM \
	STORE_32(value, address & (SIZE_PALETTE_RAM - 4), gba->video.palette); \
	gba->video.renderer->writePalette(gba->video.renderer, (address & (SIZE_PALETTE_RAM - 4)) + 2, value >> 16); \
	wait += waitstatesRegion[REGION_PALETTE_RAM]; \
	gba->video.renderer->writePalette(gba->video.renderer, address & (SIZE_PALETTE_RAM - 4), value);

#define STORE_VRAM \
	if ((address & 0x0001FFFF) < SIZE_VRAM) { \
		STORE_32(value, address & 0x0001FFFC, gba->video.renderer->vram); \
		gba->video.renderer->writeVRAM(gba->video.renderer, (address & 0x0001FFFC) + 2); \
		gba->video.renderer->writeVRAM(gba->video.renderer, (address & 0x0001FFFC)); \
	} else { \
		STORE_32(value, address & 0x00017FFC, gba->video.renderer->vram); \
		gba->video.renderer->writeVRAM(gba->video.renderer, (address & 0x00017FFC) + 2); \
		gba->video.renderer->writeVRAM(gba->video.renderer, (address & 0x00017FFC)); \
	} \
	wait += waitstatesRegion[REGION_VRAM];

#define STORE_OAM \
	STORE_32(value, address & (SIZE_OAM - 4), gba->video.oam.raw); \
	gba->video.renderer->writeOAM(gba->video.renderer, (address & (SIZE_OAM - 4)) >> 1); \
	gba->video.renderer->writeOAM(gba->video.renderer, ((address & (SIZE_OAM - 4)) >> 1) + 1);

#define STORE_CART \
	wait += waitstatesRegion[address >> BASE_OFFSET]; \
	GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Store32: 0x%08X", address);

#define STORE_SRAM \
	GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Store32: 0x%08X", address);

#define STORE_BAD \
	GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Store32: 0x%08X", address);

void GBAStore32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int wait = 0;
	char* waitstatesRegion = memory->waitstatesNonseq32;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		STORE_WORKING_RAM;
		break;
	case REGION_WORKING_IRAM:
		STORE_WORKING_IRAM
		break;
	case REGION_IO:
		STORE_IO;
		break;
	case REGION_PALETTE_RAM:
		STORE_PALETTE_RAM;
		break;
	case REGION_VRAM:
		STORE_VRAM;
		break;
	case REGION_OAM:
		STORE_OAM;
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		STORE_CART;
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		STORE_SRAM;
		break;
	default:
		STORE_BAD;
		break;
	}

	if (cycleCounter) {
		++wait;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
}

void GBAStore16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int wait = 0;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		STORE_16(value, address & (SIZE_WORKING_RAM - 2), memory->wram);
		wait = memory->waitstatesNonseq16[REGION_WORKING_RAM];
		break;
	case REGION_WORKING_IRAM:
		STORE_16(value, address & (SIZE_WORKING_IRAM - 2), memory->iwram);
		break;
	case REGION_IO:
		GBAIOWrite(gba, address & (SIZE_IO - 2), value);
		break;
	case REGION_PALETTE_RAM:
		STORE_16(value, address & (SIZE_PALETTE_RAM - 2), gba->video.palette);
		gba->video.renderer->writePalette(gba->video.renderer, address & (SIZE_PALETTE_RAM - 2), value);
		break;
	case REGION_VRAM:
		if ((address & 0x0001FFFF) < SIZE_VRAM) {
			STORE_16(value, address & 0x0001FFFE, gba->video.renderer->vram);
			gba->video.renderer->writeVRAM(gba->video.renderer, address & 0x0001FFFE);
		} else {
			STORE_16(value, address & 0x00017FFE, gba->video.renderer->vram);
			gba->video.renderer->writeVRAM(gba->video.renderer, address & 0x00017FFE);
		}
		break;
	case REGION_OAM:
		STORE_16(value, address & (SIZE_OAM - 2), gba->video.oam.raw);
		gba->video.renderer->writeOAM(gba->video.renderer, (address & (SIZE_OAM - 2)) >> 1);
		break;
	case REGION_CART0:
		if (memory->hw.devices != HW_NONE && IS_GPIO_REGISTER(address & 0xFFFFFE)) {
			uint32_t reg = address & 0xFFFFFE;
			GBAHardwareGPIOWrite(&memory->hw, reg, value);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad cartridge Store16: 0x%08X", address);
		}
		break;
	case REGION_CART2_EX:
		if (memory->savedata.type == SAVEDATA_AUTODETECT) {
			GBALog(gba, GBA_LOG_INFO, "Detected EEPROM savegame");
			GBASavedataInitEEPROM(&memory->savedata);
		}
		GBASavedataWriteEEPROM(&memory->savedata, value, 1);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Store16: 0x%08X", address);
		break;
	default:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Store16: 0x%08X", address);
		break;
	}

	if (cycleCounter) {
		++wait;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
}

void GBAStore8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int wait = 0;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		((int8_t*) memory->wram)[address & (SIZE_WORKING_RAM - 1)] = value;
		wait = memory->waitstatesNonseq16[REGION_WORKING_RAM];
		break;
	case REGION_WORKING_IRAM:
		((int8_t*) memory->iwram)[address & (SIZE_WORKING_IRAM - 1)] = value;
		break;
	case REGION_IO:
		GBAIOWrite8(gba, address & (SIZE_IO - 1), value);
		break;
	case REGION_PALETTE_RAM:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Store8: 0x%08X", address);
		break;
	case REGION_VRAM:
		if (address >= 0x06018000) {
			// TODO: check BG mode
			GBALog(gba, GBA_LOG_GAME_ERROR, "Cannot Store8 to OBJ: 0x%08X", address);
			break;
		}
		gba->video.renderer->vram[(address & 0x1FFFE) >> 1] = ((uint8_t) value) | (value << 8);
		gba->video.renderer->writeVRAM(gba->video.renderer, address & 0x0001FFFE);
		break;
	case REGION_OAM:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Cannot Store8 to OAM: 0x%08X", address);
		break;
	case REGION_CART0:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Store8: 0x%08X", address);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		if (memory->savedata.type == SAVEDATA_AUTODETECT) {
			if (address == SAVEDATA_FLASH_BASE) {
				GBALog(gba, GBA_LOG_INFO, "Detected Flash savegame");
				GBASavedataInitFlash(&memory->savedata, gba->realisticTiming);
			} else {
				GBALog(gba, GBA_LOG_INFO, "Detected SRAM savegame");
				GBASavedataInitSRAM(&memory->savedata);
			}
		}
		if (memory->savedata.type == SAVEDATA_FLASH512 || memory->savedata.type == SAVEDATA_FLASH1M) {
			GBASavedataWriteFlash(&memory->savedata, address, value);
		} else if (memory->savedata.type == SAVEDATA_SRAM) {
			memory->savedata.data[address & (SIZE_CART_SRAM - 1)] = value;
			memory->savedata.dirty |= SAVEDATA_DIRT_NEW;
		} else if (memory->hw.devices & HW_TILT) {
			GBAHardwareTiltWrite(&memory->hw, address & OFFSET_MASK, value);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Writing to non-existent SRAM: 0x%08X", address);
		}
		wait = memory->waitstatesNonseq16[REGION_CART_SRAM];
		break;
	default:
		GBALog(gba, GBA_LOG_GAME_ERROR, "Bad memory Store8: 0x%08X", address);
		break;
	}

	if (cycleCounter) {
		++wait;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
		*cycleCounter += wait;
	}
}

void GBAPatch32(struct ARMCore* cpu, uint32_t address, int32_t value, int32_t* old) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int32_t oldValue = -1;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		LOAD_32(oldValue, address & (SIZE_WORKING_RAM - 4), memory->wram);
		STORE_32(value, address & (SIZE_WORKING_RAM - 4), memory->wram);
		break;
	case REGION_WORKING_IRAM:
		LOAD_32(oldValue, address & (SIZE_WORKING_IRAM - 4), memory->iwram);
		STORE_32(value, address & (SIZE_WORKING_IRAM - 4), memory->iwram);
		break;
	case REGION_IO:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch32: 0x%08X", address);
		break;
	case REGION_PALETTE_RAM:
		LOAD_32(oldValue, address & (SIZE_PALETTE_RAM - 1), gba->video.palette);
		STORE_32(value, address & (SIZE_PALETTE_RAM - 4), gba->video.palette);
		gba->video.renderer->writePalette(gba->video.renderer, address & (SIZE_PALETTE_RAM - 4), value);
		gba->video.renderer->writePalette(gba->video.renderer, (address & (SIZE_PALETTE_RAM - 4)) + 2, value >> 16);
		break;
	case REGION_VRAM:
		if ((address & 0x0001FFFF) < SIZE_VRAM) {
			LOAD_32(oldValue, address & 0x0001FFFC, gba->video.renderer->vram);
			STORE_32(value, address & 0x0001FFFC, gba->video.renderer->vram);
		} else {
			LOAD_32(oldValue, address & 0x00017FFC, gba->video.renderer->vram);
			STORE_32(value, address & 0x00017FFC, gba->video.renderer->vram);
		}
		break;
	case REGION_OAM:
		LOAD_32(oldValue, address & (SIZE_OAM - 4), gba->video.oam.raw);
		STORE_32(value, address & (SIZE_OAM - 4), gba->video.oam.raw);
		gba->video.renderer->writeOAM(gba->video.renderer, (address & (SIZE_OAM - 4)) >> 1);
		gba->video.renderer->writeOAM(gba->video.renderer, ((address & (SIZE_OAM - 4)) + 2) >> 1);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		_pristineCow(gba);
		if ((address & (SIZE_CART0 - 4)) >= gba->memory.romSize) {
			gba->memory.romSize = (address & (SIZE_CART0 - 4)) + 4;
			gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
		}
		LOAD_32(oldValue, address & (SIZE_CART0 - 4), gba->memory.rom);
		STORE_32(value, address & (SIZE_CART0 - 4), gba->memory.rom);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		if (memory->savedata.type == SAVEDATA_SRAM) {
			LOAD_32(oldValue, address & (SIZE_CART_SRAM - 4), memory->savedata.data);
			STORE_32(value, address & (SIZE_CART_SRAM - 4), memory->savedata.data);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Writing to non-existent SRAM: 0x%08X", address);
		}
		break;
	default:
		GBALog(gba, GBA_LOG_WARN, "Bad memory Patch16: 0x%08X", address);
		break;
	}
	if (old) {
		*old = oldValue;
	}
}

void GBAPatch16(struct ARMCore* cpu, uint32_t address, int16_t value, int16_t* old) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int16_t oldValue = -1;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		LOAD_16(oldValue, address & (SIZE_WORKING_RAM - 2), memory->wram);
		STORE_16(value, address & (SIZE_WORKING_RAM - 2), memory->wram);
		break;
	case REGION_WORKING_IRAM:
		LOAD_16(oldValue, address & (SIZE_WORKING_IRAM - 2), memory->iwram);
		STORE_16(value, address & (SIZE_WORKING_IRAM - 2), memory->iwram);
		break;
	case REGION_IO:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch16: 0x%08X", address);
		break;
	case REGION_PALETTE_RAM:
		LOAD_16(oldValue, address & (SIZE_PALETTE_RAM - 2), gba->video.palette);
		STORE_16(value, address & (SIZE_PALETTE_RAM - 2), gba->video.palette);
		gba->video.renderer->writePalette(gba->video.renderer, address & (SIZE_PALETTE_RAM - 2), value);
		break;
	case REGION_VRAM:
		if ((address & 0x0001FFFF) < SIZE_VRAM) {
			LOAD_16(oldValue, address & 0x0001FFFE, gba->video.renderer->vram);
			STORE_16(value, address & 0x0001FFFE, gba->video.renderer->vram);
		} else {
			LOAD_16(oldValue, address & 0x00017FFE, gba->video.renderer->vram);
			STORE_16(value, address & 0x00017FFE, gba->video.renderer->vram);
		}
		break;
	case REGION_OAM:
		LOAD_16(oldValue, address & (SIZE_OAM - 2), gba->video.oam.raw);
		STORE_16(value, address & (SIZE_OAM - 2), gba->video.oam.raw);
		gba->video.renderer->writeOAM(gba->video.renderer, (address & (SIZE_OAM - 2)) >> 1);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		_pristineCow(gba);
		if ((address & (SIZE_CART0 - 1)) >= gba->memory.romSize) {
			gba->memory.romSize = (address & (SIZE_CART0 - 2)) + 2;
			gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
		}
		LOAD_16(oldValue, address & (SIZE_CART0 - 2), gba->memory.rom);
		STORE_16(value, address & (SIZE_CART0 - 2), gba->memory.rom);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		if (memory->savedata.type == SAVEDATA_SRAM) {
			LOAD_16(oldValue, address & (SIZE_CART_SRAM - 2), memory->savedata.data);
			STORE_16(value, address & (SIZE_CART_SRAM - 2), memory->savedata.data);
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Writing to non-existent SRAM: 0x%08X", address);
		}
		break;
	default:
		GBALog(gba, GBA_LOG_WARN, "Bad memory Patch16: 0x%08X", address);
		break;
	}
	if (old) {
		*old = oldValue;
	}
}

void GBAPatch8(struct ARMCore* cpu, uint32_t address, int8_t value, int8_t* old) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	int8_t oldValue = -1;

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		oldValue = ((int8_t*) memory->wram)[address & (SIZE_WORKING_RAM - 1)];
		((int8_t*) memory->wram)[address & (SIZE_WORKING_RAM - 1)] = value;
		break;
	case REGION_WORKING_IRAM:
		oldValue = ((int8_t*) memory->iwram)[address & (SIZE_WORKING_IRAM - 1)];
		((int8_t*) memory->iwram)[address & (SIZE_WORKING_IRAM - 1)] = value;
		break;
	case REGION_IO:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch8: 0x%08X", address);
		break;
	case REGION_PALETTE_RAM:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch8: 0x%08X", address);
		break;
	case REGION_VRAM:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch8: 0x%08X", address);
		break;
	case REGION_OAM:
		GBALog(gba, GBA_LOG_STUB, "Unimplemented memory Patch8: 0x%08X", address);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		_pristineCow(gba);
		if ((address & (SIZE_CART0 - 1)) >= gba->memory.romSize) {
			gba->memory.romSize = (address & (SIZE_CART0 - 2)) + 2;
			gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
		}
		oldValue = ((int8_t*) memory->rom)[address & (SIZE_CART0 - 1)];
		((int8_t*) memory->rom)[address & (SIZE_CART0 - 1)] = value;
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		if (memory->savedata.type == SAVEDATA_SRAM) {
			oldValue = ((int8_t*) memory->savedata.data)[address & (SIZE_CART_SRAM - 1)];
			((int8_t*) memory->savedata.data)[address & (SIZE_CART_SRAM - 1)] = value;
		} else {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Writing to non-existent SRAM: 0x%08X", address);
		}
		break;
	default:
		GBALog(gba, GBA_LOG_WARN, "Bad memory Patch8: 0x%08X", address);
		break;
	}
	if (old) {
		*old = oldValue;
	}
}

#define LDM_LOOP(LDM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			LDM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			cpu->gprs[i] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			LDM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			cpu->gprs[i + 1] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			LDM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			cpu->gprs[i + 2] = value; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			LDM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			cpu->gprs[i + 3] = value; \
			++wait; \
			address += 4; \
		} \
	}

uint32_t GBALoadMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	uint32_t value;
	int wait = 0;
	char* waitstatesRegion = memory->waitstatesNonseq32;

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

	switch (address >> BASE_OFFSET) {
	case REGION_BIOS:
		LDM_LOOP(LOAD_BIOS);
		break;
	case REGION_WORKING_RAM:
		LDM_LOOP(LOAD_WORKING_RAM);
		break;
	case REGION_WORKING_IRAM:
		LDM_LOOP(LOAD_WORKING_IRAM);
		break;
	case REGION_IO:
		LDM_LOOP(LOAD_IO);
		break;
	case REGION_PALETTE_RAM:
		LDM_LOOP(LOAD_PALETTE_RAM);
		break;
	case REGION_VRAM:
		LDM_LOOP(LOAD_VRAM);
		break;
	case REGION_OAM:
		LDM_LOOP(LOAD_OAM);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		LDM_LOOP(LOAD_CART);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		LDM_LOOP(LOAD_SRAM);
		break;
	default:
		LDM_LOOP(LOAD_BAD);
		break;
	}

	if (cycleCounter) {
		++wait;
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
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

#define STM_LOOP(STM) \
	for (i = 0; i < 16; i += 4) { \
		if (UNLIKELY(mask & (1 << i))) { \
			value = cpu->gprs[i]; \
			STM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (2 << i))) { \
			value = cpu->gprs[i + 1]; \
			STM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (4 << i))) { \
			value = cpu->gprs[i + 2]; \
			STM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			++wait; \
			address += 4; \
		} \
		if (UNLIKELY(mask & (8 << i))) { \
			value = cpu->gprs[i + 3]; \
			STM; \
			waitstatesRegion = memory->waitstatesSeq32; \
			++wait; \
			address += 4; \
		} \
	}

uint32_t GBAStoreMultiple(struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;
	uint32_t value;
	int wait = 0;
	char* waitstatesRegion = memory->waitstatesNonseq32;

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

	switch (address >> BASE_OFFSET) {
	case REGION_WORKING_RAM:
		STM_LOOP(STORE_WORKING_RAM);
		break;
	case REGION_WORKING_IRAM:
		STM_LOOP(STORE_WORKING_IRAM);
		break;
	case REGION_IO:
		STM_LOOP(STORE_IO);
		break;
	case REGION_PALETTE_RAM:
		STM_LOOP(STORE_PALETTE_RAM);
		break;
	case REGION_VRAM:
		STM_LOOP(STORE_VRAM);
		break;
	case REGION_OAM:
		STM_LOOP(STORE_OAM);
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
	case REGION_CART1:
	case REGION_CART1_EX:
	case REGION_CART2:
	case REGION_CART2_EX:
		STM_LOOP(STORE_CART);
		break;
	case REGION_CART_SRAM:
	case REGION_CART_SRAM_MIRROR:
		STM_LOOP(STORE_SRAM);
		break;
	default:
		STM_LOOP(STORE_BAD);
		break;
	}

	if (cycleCounter) {
		if (address >> BASE_OFFSET < REGION_CART0) {
			wait = GBAMemoryStall(cpu, wait);
		}
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

void GBAAdjustWaitstates(struct GBA* gba, uint16_t parameters) {
	struct GBAMemory* memory = &gba->memory;
	struct ARMCore* cpu = gba->cpu;
	int sram = parameters & 0x0003;
	int ws0 = (parameters & 0x000C) >> 2;
	int ws0seq = (parameters & 0x0010) >> 4;
	int ws1 = (parameters & 0x0060) >> 5;
	int ws1seq = (parameters & 0x0080) >> 7;
	int ws2 = (parameters & 0x0300) >> 8;
	int ws2seq = (parameters & 0x0400) >> 10;
	int prefetch = parameters & 0x4000;

	memory->waitstatesNonseq16[REGION_CART_SRAM] = memory->waitstatesNonseq16[REGION_CART_SRAM_MIRROR] = GBA_ROM_WAITSTATES[sram];
	memory->waitstatesSeq16[REGION_CART_SRAM] = memory->waitstatesSeq16[REGION_CART_SRAM_MIRROR] = GBA_ROM_WAITSTATES[sram];
	memory->waitstatesNonseq32[REGION_CART_SRAM] = memory->waitstatesNonseq32[REGION_CART_SRAM_MIRROR] = 2 * GBA_ROM_WAITSTATES[sram] + 1;
	memory->waitstatesSeq32[REGION_CART_SRAM] = memory->waitstatesSeq32[REGION_CART_SRAM_MIRROR] = 2 * GBA_ROM_WAITSTATES[sram] + 1;

	memory->waitstatesNonseq16[REGION_CART0] = memory->waitstatesNonseq16[REGION_CART0_EX] = GBA_ROM_WAITSTATES[ws0];
	memory->waitstatesNonseq16[REGION_CART1] = memory->waitstatesNonseq16[REGION_CART1_EX] = GBA_ROM_WAITSTATES[ws1];
	memory->waitstatesNonseq16[REGION_CART2] = memory->waitstatesNonseq16[REGION_CART2_EX] = GBA_ROM_WAITSTATES[ws2];

	memory->waitstatesSeq16[REGION_CART0] = memory->waitstatesSeq16[REGION_CART0_EX] = GBA_ROM_WAITSTATES_SEQ[ws0seq];
	memory->waitstatesSeq16[REGION_CART1] = memory->waitstatesSeq16[REGION_CART1_EX] = GBA_ROM_WAITSTATES_SEQ[ws1seq + 2];
	memory->waitstatesSeq16[REGION_CART2] = memory->waitstatesSeq16[REGION_CART2_EX] = GBA_ROM_WAITSTATES_SEQ[ws2seq + 4];

	memory->waitstatesNonseq32[REGION_CART0] = memory->waitstatesNonseq32[REGION_CART0_EX] = memory->waitstatesNonseq16[REGION_CART0] + 1 + memory->waitstatesSeq16[REGION_CART0];
	memory->waitstatesNonseq32[REGION_CART1] = memory->waitstatesNonseq32[REGION_CART1_EX] = memory->waitstatesNonseq16[REGION_CART1] + 1 + memory->waitstatesSeq16[REGION_CART1];
	memory->waitstatesNonseq32[REGION_CART2] = memory->waitstatesNonseq32[REGION_CART2_EX] = memory->waitstatesNonseq16[REGION_CART2] + 1 + memory->waitstatesSeq16[REGION_CART2];

	memory->waitstatesSeq32[REGION_CART0] = memory->waitstatesSeq32[REGION_CART0_EX] = 2 * memory->waitstatesSeq16[REGION_CART0] + 1;
	memory->waitstatesSeq32[REGION_CART1] = memory->waitstatesSeq32[REGION_CART1_EX] = 2 * memory->waitstatesSeq16[REGION_CART1] + 1;
	memory->waitstatesSeq32[REGION_CART2] = memory->waitstatesSeq32[REGION_CART2_EX] = 2 * memory->waitstatesSeq16[REGION_CART2] + 1;

	memory->prefetch = prefetch;

	cpu->memory.activeSeqCycles32 = memory->waitstatesSeq32[memory->activeRegion];
	cpu->memory.activeSeqCycles16 = memory->waitstatesSeq16[memory->activeRegion];

	cpu->memory.activeNonseqCycles32 = memory->waitstatesNonseq32[memory->activeRegion];
	cpu->memory.activeNonseqCycles16 = memory->waitstatesNonseq16[memory->activeRegion];
}

void GBAMemoryWriteDMASAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	memory->dma[dma].source = address & 0x0FFFFFFE;
}

void GBAMemoryWriteDMADAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	memory->dma[dma].dest = address & 0x0FFFFFFE;
}

void GBAMemoryWriteDMACNT_LO(struct GBA* gba, int dma, uint16_t count) {
	struct GBAMemory* memory = &gba->memory;
	memory->dma[dma].count = count ? count : (dma == 3 ? 0x10000 : 0x4000);
}

uint16_t GBAMemoryWriteDMACNT_HI(struct GBA* gba, int dma, uint16_t control) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* currentDma = &memory->dma[dma];
	int wasEnabled = GBADMARegisterIsEnable(currentDma->reg);
	currentDma->reg = control;

	if (GBADMARegisterIsDRQ(currentDma->reg)) {
		GBALog(gba, GBA_LOG_STUB, "DRQ not implemented");
	}

	if (!wasEnabled && GBADMARegisterIsEnable(currentDma->reg)) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;
		currentDma->nextCount = currentDma->count;
		GBAMemoryScheduleDMA(gba, dma, currentDma);
	}
	// If the DMA has already occurred, this value might have changed since the function started
	return currentDma->reg;
};

void GBAMemoryScheduleDMA(struct GBA* gba, int number, struct GBADMA* info) {
	struct ARMCore* cpu = gba->cpu;
	switch (GBADMARegisterGetTiming(info->reg)) {
	case DMA_TIMING_NOW:
		info->nextEvent = cpu->cycles;
		GBAMemoryUpdateDMAs(gba, 0);
		break;
	case DMA_TIMING_HBLANK:
		// Handled implicitly
		info->nextEvent = INT_MAX;
		break;
	case DMA_TIMING_VBLANK:
		// Handled implicitly
		info->nextEvent = INT_MAX;
		break;
	case DMA_TIMING_CUSTOM:
		info->nextEvent = INT_MAX;
		switch (number) {
		case 0:
			GBALog(gba, GBA_LOG_WARN, "Discarding invalid DMA0 scheduling");
			break;
		case 1:
		case 2:
			GBAAudioScheduleFifoDma(&gba->audio, number, info);
			break;
		case 3:
			// GBAVideoScheduleVCaptureDma(dma, info);
			break;
		}
	}
}

void GBAMemoryRunHblankDMAs(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_HBLANK) {
			dma->nextEvent = cycles;
		}
	}
	GBAMemoryUpdateDMAs(gba, 0);
}

void GBAMemoryRunVblankDMAs(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_VBLANK) {
			dma->nextEvent = cycles;
		}
	}
	GBAMemoryUpdateDMAs(gba, 0);
}

int32_t GBAMemoryRunDMAs(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	if (memory->nextDMA == INT_MAX) {
		return INT_MAX;
	}
	memory->nextDMA -= cycles;
	memory->eventDiff += cycles;
	while (memory->nextDMA <= 0) {
		struct GBADMA* dma = &memory->dma[memory->activeDMA];
		GBAMemoryServiceDMA(gba, memory->activeDMA, dma);
		GBAMemoryUpdateDMAs(gba, memory->eventDiff);
		memory->eventDiff = 0;
	}
	return memory->nextDMA;
}

void GBAMemoryUpdateDMAs(struct GBA* gba, int32_t cycles) {
	int i;
	struct GBAMemory* memory = &gba->memory;
	struct ARMCore* cpu = gba->cpu;
	memory->activeDMA = -1;
	memory->nextDMA = INT_MAX;
	for (i = 3; i >= 0; --i) {
		struct GBADMA* dma = &memory->dma[i];
		if (dma->nextEvent != INT_MAX) {
			dma->nextEvent -= cycles;
			if (GBADMARegisterIsEnable(dma->reg)) {
				memory->activeDMA = i;
				memory->nextDMA = dma->nextEvent;
			}
		}
	}
	if (memory->nextDMA < cpu->nextEvent) {
		cpu->nextEvent = memory->nextDMA;
	}
}

void GBAMemoryServiceDMA(struct GBA* gba, int number, struct GBADMA* info) {
	struct GBAMemory* memory = &gba->memory;
	struct ARMCore* cpu = gba->cpu;
	uint32_t width = GBADMARegisterGetWidth(info->reg) ? 4 : 2;
	int sourceOffset = DMA_OFFSET[GBADMARegisterGetSrcControl(info->reg)] * width;
	int destOffset = DMA_OFFSET[GBADMARegisterGetDestControl(info->reg)] * width;
	int32_t wordsRemaining = info->nextCount;
	uint32_t source = info->nextSource;
	uint32_t dest = info->nextDest;
	uint32_t sourceRegion = source >> BASE_OFFSET;
	uint32_t destRegion = dest >> BASE_OFFSET;
	int32_t cycles = 2;

	if (source == info->source) {
		// TODO: support 4 cycles for ROM access
		cycles += 2;
		if (width == 4) {
			cycles += memory->waitstatesNonseq32[sourceRegion] + memory->waitstatesNonseq32[destRegion];
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
		} else {
			cycles += memory->waitstatesNonseq16[sourceRegion] + memory->waitstatesNonseq16[destRegion];
		}
	} else {
		if (width == 4) {
			cycles += memory->waitstatesSeq32[sourceRegion] + memory->waitstatesSeq32[destRegion];
		} else {
			cycles += memory->waitstatesSeq16[sourceRegion] + memory->waitstatesSeq16[destRegion];
		}
	}

	gba->performingDMA = true;
	int32_t word;
	if (width == 4) {
		word = cpu->memory.load32(cpu, source, 0);
		gba->bus = word;
		cpu->memory.store32(cpu, dest, word, 0);
		source += sourceOffset;
		dest += destOffset;
		--wordsRemaining;
	} else {
		if (sourceRegion == REGION_CART2_EX && memory->savedata.type == SAVEDATA_EEPROM) {
			word = GBASavedataReadEEPROM(&memory->savedata);
			gba->bus = word | (word << 16);
			cpu->memory.store16(cpu, dest, word, 0);
			source += sourceOffset;
			dest += destOffset;
			--wordsRemaining;
		} else if (destRegion == REGION_CART2_EX) {
			if (memory->savedata.type == SAVEDATA_AUTODETECT) {
				GBALog(gba, GBA_LOG_INFO, "Detected EEPROM savegame");
				GBASavedataInitEEPROM(&memory->savedata);
			}
			word = cpu->memory.load16(cpu, source, 0);
			gba->bus = word | (word << 16);
			GBASavedataWriteEEPROM(&memory->savedata, word, wordsRemaining);
			source += sourceOffset;
			dest += destOffset;
			--wordsRemaining;
		} else {
			word = cpu->memory.load16(cpu, source, 0);
			gba->bus = word | (word << 16);
			cpu->memory.store16(cpu, dest, word, 0);
			source += sourceOffset;
			dest += destOffset;
			--wordsRemaining;
		}
	}
	gba->performingDMA = false;

	if (!wordsRemaining) {
		if (!GBADMARegisterIsRepeat(info->reg) || GBADMARegisterGetTiming(info->reg) == DMA_TIMING_NOW) {
			info->reg = GBADMARegisterClearEnable(info->reg);
			info->nextEvent = INT_MAX;

			// Clear the enable bit in memory
			memory->io[(REG_DMA0CNT_HI + number * (REG_DMA1CNT_HI - REG_DMA0CNT_HI)) >> 1] &= 0x7FE0;
		} else {
			info->nextCount = info->count;
			if (GBADMARegisterGetDestControl(info->reg) == DMA_INCREMENT_RELOAD) {
				info->nextDest = info->dest;
			}
			GBAMemoryScheduleDMA(gba, number, info);
		}
		if (GBADMARegisterIsDoIRQ(info->reg)) {
			GBARaiseIRQ(gba, IRQ_DMA0 + number);
		}
	} else {
		info->nextDest = dest;
		info->nextCount = wordsRemaining;
	}
	info->nextSource = source;

	if (info->nextEvent != INT_MAX) {
		info->nextEvent += cycles;
	}
	cpu->cycles += cycles;
}

int32_t GBAMemoryStall(struct ARMCore* cpu, int32_t wait) {
	struct GBA* gba = (struct GBA*) cpu->master;
	struct GBAMemory* memory = &gba->memory;

	if (memory->activeRegion < REGION_CART0 || !memory->prefetch) {
		// The wait is the stall
		return wait;
	}

	int32_t s = cpu->memory.activeSeqCycles16 + 1;
	int32_t n2s = cpu->memory.activeNonseqCycles16 - cpu->memory.activeSeqCycles16 + 1;

	// Figure out how many sequential loads we can jam in
	int32_t stall = s;
	int32_t loads = 1;
	int32_t previousLoads = 0;

	// Don't prefetch too much if we're overlapping with a previous prefetch
	uint32_t dist = (memory->lastPrefetchedPc - cpu->gprs[ARM_PC]) >> 1;
	if (dist < memory->lastPrefetchedLoads) {
		previousLoads = dist;
	}
	while (stall < wait) {
		stall += s;
		++loads;
	}
	if (loads + previousLoads > 8) {
		int diff = (loads + previousLoads) - 8;
		loads -= diff;
		stall -= s * diff;
	} else if (stall > wait && loads == 1) {
		// We might need to stall a bit extra if we haven't finished the first S cycle
		wait = stall;
	}
	// This instruction used to have an N, convert it to an S.
	wait -= n2s;

	// TODO: Invalidate prefetch on branch
	memory->lastPrefetchedLoads = loads;
	memory->lastPrefetchedPc = cpu->gprs[ARM_PC] + WORD_SIZE_THUMB * loads;

	// The next |loads|S waitstates disappear entirely, so long as they're all in a row
	cpu->cycles -= (s - 1) * loads;
	return wait;
}

void GBAMemorySerialize(const struct GBAMemory* memory, struct GBASerializedState* state) {
	memcpy(state->wram, memory->wram, SIZE_WORKING_RAM);
	memcpy(state->iwram, memory->iwram, SIZE_WORKING_IRAM);
}

void GBAMemoryDeserialize(struct GBAMemory* memory, const struct GBASerializedState* state) {
	memcpy(memory->wram, state->wram, SIZE_WORKING_RAM);
	memcpy(memory->iwram, state->iwram, SIZE_WORKING_IRAM);
}

void _pristineCow(struct GBA* gba) {
	if (gba->memory.rom != gba->pristineRom) {
		return;
	}
	gba->memory.rom = anonymousMemoryMap(SIZE_CART0);
	memcpy(gba->memory.rom, gba->pristineRom, gba->memory.romSize);
	memset(((uint8_t*) gba->memory.rom) + gba->memory.romSize, 0xFF, SIZE_CART0 - gba->memory.romSize);
}
