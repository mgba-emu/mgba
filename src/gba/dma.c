/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "dma.h"

#include "gba/gba.h"
#include "gba/io.h"

static void _dmaEvent(struct mTiming* timing, void* context, uint32_t cyclesLate);

static void GBADMAService(struct GBA* gba, int number, struct GBADMA* info);

static const int DMA_OFFSET[] = { 1, -1, 0, 1 };

void GBADMAInit(struct GBA* gba) {
	gba->memory.dmaEvent.name = "GBA DMA";
	gba->memory.dmaEvent.callback = _dmaEvent;
	gba->memory.dmaEvent.context = gba;
	gba->memory.dmaEvent.priority = 0x40;
}

void GBADMAReset(struct GBA* gba) {
	memset(gba->memory.dma, 0, sizeof(gba->memory.dma));
	int i;
	for (i = 0; i < 4; ++i) {
		gba->memory.dma[i].count = 0x4000;
		gba->memory.dma[i].nextEvent = INT_MAX;
	}
	gba->memory.dma[3].count = 0x10000;
	gba->memory.activeDMA = -1;
}
static bool _isValidDMASAD(int dma, uint32_t address) {
	if (dma == 0 && address >= BASE_CART0 && address < BASE_CART_SRAM) {
		return false;
	}
	return address >= BASE_WORKING_RAM;
}

static bool _isValidDMADAD(int dma, uint32_t address) {
	return dma == 3 || address < BASE_CART0;
}

uint32_t GBADMAWriteSAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	address &= 0x0FFFFFFE;
	if (_isValidDMASAD(dma, address)) {
		memory->dma[dma].source = address;
	}
	return memory->dma[dma].source;
}

uint32_t GBADMAWriteDAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	address &= 0x0FFFFFFE;
	if (_isValidDMADAD(dma, address)) {
		memory->dma[dma].dest = address;
	}
	return memory->dma[dma].dest;
}

void GBADMAWriteCNT_LO(struct GBA* gba, int dma, uint16_t count) {
	struct GBAMemory* memory = &gba->memory;
	memory->dma[dma].count = count ? count : (dma == 3 ? 0x10000 : 0x4000);
}

uint16_t GBADMAWriteCNT_HI(struct GBA* gba, int dma, uint16_t control) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* currentDma = &memory->dma[dma];
	int wasEnabled = GBADMARegisterIsEnable(currentDma->reg);
	if (dma < 3) {
		control &= 0xF7E0;
	} else {
		control &= 0xFFE0;
	}
	currentDma->reg = control;

	if (GBADMARegisterIsDRQ(currentDma->reg)) {
		mLOG(GBA_MEM, STUB, "DRQ not implemented");
	}

	if (!wasEnabled && GBADMARegisterIsEnable(currentDma->reg)) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;
		currentDma->nextCount = currentDma->count;
		GBADMASchedule(gba, dma, currentDma);
	}
	// If the DMA has already occurred, this value might have changed since the function started
	return currentDma->reg;
};

void GBADMASchedule(struct GBA* gba, int number, struct GBADMA* info) {
	info->hasStarted = 0;
	switch (GBADMARegisterGetTiming(info->reg)) {
	case DMA_TIMING_NOW:
		info->nextEvent = 2 + 1; // XXX: Account for I cycle when writing
		info->scheduledAt = mTimingCurrentTime(&gba->timing);
		GBADMAUpdate(gba, 0);
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
			mLOG(GBA_MEM, WARN, "Discarding invalid DMA0 scheduling");
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

void GBADMARunHblank(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	bool dmaSeen = false;
	if (memory->activeDMA >= 0) {
		GBADMAUpdate(gba, mTimingCurrentTime(&gba->timing) - memory->dma[memory->activeDMA].scheduledAt);
	}
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_HBLANK) {
			dma->nextEvent = 2 + cycles;
			dma->scheduledAt = mTimingCurrentTime(&gba->timing);
			dmaSeen = true;
		}
	}
	if (dmaSeen) {
		GBADMAUpdate(gba, 0);
	}
}

void GBADMARunVblank(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	bool dmaSeen = false;
	if (memory->activeDMA >= 0) {
		GBADMAUpdate(gba, mTimingCurrentTime(&gba->timing) - memory->dma[memory->activeDMA].scheduledAt);
	}
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_VBLANK) {
			dma->nextEvent = 2 + cycles;
			dma->scheduledAt = mTimingCurrentTime(&gba->timing);
			dmaSeen = true;
		}
	}
	if (dmaSeen) {
		GBADMAUpdate(gba, 0);
	}
}

void _dmaEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	struct GBA* gba = context;
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma = &memory->dma[memory->activeDMA];
	dma->nextEvent = -cyclesLate;
	GBADMAService(gba, memory->activeDMA, dma);
}

void GBADMAUpdate(struct GBA* gba, int32_t cycles) {
	int i;
	struct GBAMemory* memory = &gba->memory;
	memory->activeDMA = -1;
	for (i = 3; i >= 0; --i) {
		struct GBADMA* dma = &memory->dma[i];
		if (dma->nextEvent != INT_MAX) {
			dma->nextEvent -= cycles;
			if (GBADMARegisterIsEnable(dma->reg)) {
				memory->activeDMA = i;
			}
		}
	}

	if (memory->activeDMA >= 0) {
		mTimingDeschedule(&gba->timing, &memory->dmaEvent);
		mTimingSchedule(&gba->timing, &memory->dmaEvent, memory->dma[memory->activeDMA].nextEvent);
	} else {
		gba->cpuBlocked = false;
	}
}

void GBADMAService(struct GBA* gba, int number, struct GBADMA* info) {
	struct GBAMemory* memory = &gba->memory;
	struct ARMCore* cpu = gba->cpu;
	uint32_t width = 2 << GBADMARegisterGetWidth(info->reg);
	int32_t wordsRemaining = info->nextCount;
	uint32_t source = info->nextSource;
	uint32_t dest = info->nextDest;
	uint32_t sourceRegion = source >> BASE_OFFSET;
	uint32_t destRegion = dest >> BASE_OFFSET;
	int32_t cycles = 2;

	gba->cpuBlocked = true;
	if (info->hasStarted < 2) {
		if (sourceRegion < REGION_CART0 || destRegion < REGION_CART0) {
			cycles += 2;
		}
		if (width == 4) {
			cycles += memory->waitstatesNonseq32[sourceRegion] + memory->waitstatesNonseq32[destRegion];
		} else {
			cycles += memory->waitstatesNonseq16[sourceRegion] + memory->waitstatesNonseq16[destRegion];
		}
		if (info->hasStarted < 1) {
			info->hasStarted = wordsRemaining;
			info->nextEvent = 0;
			info->scheduledAt = mTimingCurrentTime(&gba->timing);
			GBADMAUpdate(gba, -cycles);
			return;
		}
		info->hasStarted = 2;
		source &= -width;
		dest &= -width;
	} else {
		if (width == 4) {
			cycles += memory->waitstatesSeq32[sourceRegion] + memory->waitstatesSeq32[destRegion];
		} else {
			cycles += memory->waitstatesSeq16[sourceRegion] + memory->waitstatesSeq16[destRegion];
		}
	}
	info->nextEvent += cycles;

	gba->performingDMA = 1 | (number << 1);
	uint32_t word;
	if (width == 4) {
		word = cpu->memory.load32(cpu, source, 0);
		gba->bus = word;
		cpu->memory.store32(cpu, dest, word, 0);
	} else {
		if (sourceRegion == REGION_CART2_EX && memory->savedata.type == SAVEDATA_EEPROM) {
			word = GBASavedataReadEEPROM(&memory->savedata);
			cpu->memory.store16(cpu, dest, word, 0);
		} else if (destRegion == REGION_CART2_EX) {
			if (memory->savedata.type == SAVEDATA_AUTODETECT) {
				mLOG(GBA_MEM, INFO, "Detected EEPROM savegame");
				GBASavedataInitEEPROM(&memory->savedata, gba->realisticTiming);
			}
			word = cpu->memory.load16(cpu, source, 0);
			GBASavedataWriteEEPROM(&memory->savedata, word, wordsRemaining);
		} else {
			word = cpu->memory.load16(cpu, source, 0);
			cpu->memory.store16(cpu, dest, word, 0);
		}
		gba->bus = word | (word << 16);
	}
	int sourceOffset = DMA_OFFSET[GBADMARegisterGetSrcControl(info->reg)] * width;
	int destOffset = DMA_OFFSET[GBADMARegisterGetDestControl(info->reg)] * width;
	source += sourceOffset;
	dest += destOffset;
	--wordsRemaining;
	gba->performingDMA = 0;

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
			GBADMASchedule(gba, number, info);
		}
		if (GBADMARegisterIsDoIRQ(info->reg)) {
			GBARaiseIRQ(gba, IRQ_DMA0 + number);
		}
	} else {
		info->nextDest = dest;
		info->nextCount = wordsRemaining;
		info->scheduledAt = mTimingCurrentTime(&gba->timing);
	}
	info->nextSource = source;
	GBADMAUpdate(gba, 0);
}
