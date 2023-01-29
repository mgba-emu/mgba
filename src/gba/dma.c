/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/dma.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

mLOG_DEFINE_CATEGORY(GBA_DMA, "GBA DMA", "gba.dma");

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
	}
	gba->memory.dma[3].count = 0x10000;
	gba->memory.activeDMA = -1;
}
static bool _isValidDMASAD(int dma, uint32_t address) {
	if (dma == 0 && address >= GBA_BASE_ROM0 && address < GBA_BASE_SRAM) {
		return false;
	}
	return address >= GBA_BASE_EWRAM;
}

static bool _isValidDMADAD(int dma, uint32_t address) {
	return dma == 3 || address < GBA_BASE_ROM0;
}

uint32_t GBADMAWriteSAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	if (_isValidDMASAD(dma, address)) {
		memory->dma[dma].source = address & 0x0FFFFFFE;
	} else {
		mLOG(GBA_DMA, GAME_ERROR, "Invalid DMA source address: 0x%08X", address);
		memory->dma[dma].source = 0;
	}
	return memory->dma[dma].source;
}

uint32_t GBADMAWriteDAD(struct GBA* gba, int dma, uint32_t address) {
	struct GBAMemory* memory = &gba->memory;
	address &= 0x0FFFFFFE;
	if (_isValidDMADAD(dma, address)) {
		memory->dma[dma].dest = address;
	} else {
		mLOG(GBA_DMA, GAME_ERROR, "Invalid DMA destination address: 0x%08X", address);
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
		mLOG(GBA_DMA, STUB, "DRQ not implemented");
	}

	if (!wasEnabled && GBADMARegisterIsEnable(currentDma->reg)) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;

		uint32_t width = 2 << GBADMARegisterGetWidth(currentDma->reg);
		if (currentDma->nextSource & (width - 1)) {
			mLOG(GBA_DMA, GAME_ERROR, "Misaligned DMA source address: 0x%08X", currentDma->nextSource);
		}
		if (currentDma->nextDest & (width - 1)) {
			mLOG(GBA_DMA, GAME_ERROR, "Misaligned DMA destination address: 0x%08X", currentDma->nextDest);
		}
		mLOG(GBA_DMA, INFO, "Starting DMA %i 0x%08X -> 0x%08X (%04X:%04X)", dma,
		     currentDma->nextSource, currentDma->nextDest,
		     currentDma->reg, currentDma->count & 0xFFFF);

		currentDma->nextSource &= -width;
		currentDma->nextDest &= -width;

		GBADMASchedule(gba, dma, currentDma);
	}
	// If the DMA has already occurred, this value might have changed since the function started
	return currentDma->reg;
};

void GBADMASchedule(struct GBA* gba, int number, struct GBADMA* info) {
	switch (GBADMARegisterGetTiming(info->reg)) {
	case GBA_DMA_TIMING_NOW:
		info->when = mTimingCurrentTime(&gba->timing) + 3; // DMAs take 3 cycles to start
		info->nextCount = info->count;
		break;
	case GBA_DMA_TIMING_HBLANK:
	case GBA_DMA_TIMING_VBLANK:
		// Handled implicitly
		return;
	case GBA_DMA_TIMING_CUSTOM:
		switch (number) {
		case 0:
			mLOG(GBA_DMA, WARN, "Discarding invalid DMA0 scheduling");
			return;
		case 1:
		case 2:
			GBAAudioScheduleFifoDma(&gba->audio, number, info);
			break;
		case 3:
			// Handled implicitly
			break;
		}
	}
	GBADMAUpdate(gba);
}

void GBADMARunHblank(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	bool found = false;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_HBLANK && !dma->nextCount) {
			dma->when = mTimingCurrentTime(&gba->timing) + 3 + cycles;
			dma->nextCount = dma->count;
			found = true;
		}
	}
	if (found) {
		GBADMAUpdate(gba);
	}
}

void GBADMARunVblank(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma;
	bool found = false;
	int i;
	for (i = 0; i < 4; ++i) {
		dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_VBLANK && !dma->nextCount) {
			dma->when = mTimingCurrentTime(&gba->timing) + 3 + cycles;
			dma->nextCount = dma->count;
			found = true;
		}
	}
	if (found) {
		GBADMAUpdate(gba);
	}
}

void GBADMARunDisplayStart(struct GBA* gba, int32_t cycles) {
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma = &memory->dma[3];
	if (GBADMARegisterIsEnable(dma->reg) && GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_CUSTOM && !dma->nextCount) {
		dma->when = mTimingCurrentTime(&gba->timing) + 3 + cycles;
		dma->nextCount = dma->count;
		GBADMAUpdate(gba);
	}
}

void _dmaEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GBA* gba = context;
	struct GBAMemory* memory = &gba->memory;
	struct GBADMA* dma = &memory->dma[memory->activeDMA];
	if (dma->nextCount == dma->count) {
		dma->when = mTimingCurrentTime(&gba->timing);
	}
	if (dma->nextCount & 0xFFFFF) {
		GBADMAService(gba, memory->activeDMA, dma);
	} else {
		dma->nextCount = 0;
		bool noRepeat = !GBADMARegisterIsRepeat(dma->reg);
		noRepeat |= GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_NOW;
		noRepeat |= memory->activeDMA == 3 && GBADMARegisterGetTiming(dma->reg) == GBA_DMA_TIMING_CUSTOM && gba->video.vcount == GBA_VIDEO_VERTICAL_PIXELS + 1;
		if (noRepeat) {
			dma->reg = GBADMARegisterClearEnable(dma->reg);

			// Clear the enable bit in memory
			memory->io[(REG_DMA0CNT_HI + memory->activeDMA * (REG_DMA1CNT_HI - REG_DMA0CNT_HI)) >> 1] &= 0x7FE0;
		}
		if (GBADMARegisterGetDestControl(dma->reg) == GBA_DMA_INCREMENT_RELOAD) {
			dma->nextDest = dma->dest;
		}
		if (GBADMARegisterIsDoIRQ(dma->reg)) {
			GBARaiseIRQ(gba, GBA_IRQ_DMA0 + memory->activeDMA, cyclesLate);
		}
		GBADMAUpdate(gba);
	}
}

void GBADMAUpdate(struct GBA* gba) {
	int i;
	struct GBAMemory* memory = &gba->memory;
	uint32_t currentTime = mTimingCurrentTime(&gba->timing);
	int32_t leastTime = INT_MAX;
	memory->activeDMA = -1;
	for (i = 0; i < 4; ++i) {
		struct GBADMA* dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && dma->nextCount) {
			int32_t time = dma->when - currentTime;
			if (memory->activeDMA == -1 || time < leastTime) {
				leastTime = time;
				memory->activeDMA = i;
			}
		}
	}

	if (memory->activeDMA >= 0) {
		gba->dmaPC = gba->cpu->gprs[ARM_PC];
		mTimingDeschedule(&gba->timing, &memory->dmaEvent);
		mTimingSchedule(&gba->timing, &memory->dmaEvent, memory->dma[memory->activeDMA].when - currentTime);
	} else {
		gba->cpuBlocked = false;
	}
}

void GBADMAService(struct GBA* gba, int number, struct GBADMA* info) {
	struct GBAMemory* memory = &gba->memory;
	struct ARMCore* cpu = gba->cpu;
	uint32_t width = 2 << GBADMARegisterGetWidth(info->reg);
	uint32_t source = info->nextSource;
	uint32_t dest = info->nextDest;
	uint32_t sourceRegion = source >> BASE_OFFSET;
	uint32_t destRegion = dest >> BASE_OFFSET;
	int32_t cycles = 2;

	gba->cpuBlocked = true;
	gba->performingDMA = 1 | (number << 1);

	if (info->count == info->nextCount) {
		if (width == 4) {
			cycles += memory->waitstatesNonseq32[sourceRegion] + memory->waitstatesNonseq32[destRegion];
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
	info->when += cycles;

	if (width == 4) {
		if (source) {
			memory->dmaTransferRegister = cpu->memory.load32(cpu, source, 0);
		}
		cpu->memory.store32(cpu, dest, memory->dmaTransferRegister, 0);
	} else {
		if (sourceRegion == GBA_REGION_ROM2_EX && (memory->savedata.type == SAVEDATA_EEPROM || memory->savedata.type == SAVEDATA_EEPROM512)) {
			memory->dmaTransferRegister = GBASavedataReadEEPROM(&memory->savedata);
			memory->dmaTransferRegister |= memory->dmaTransferRegister << 16;
		} else if (source) {
			memory->dmaTransferRegister = cpu->memory.load16(cpu, source, 0);
			memory->dmaTransferRegister |= memory->dmaTransferRegister << 16;
		}
		if (destRegion == GBA_REGION_ROM2_EX) {
			if (memory->savedata.type == SAVEDATA_AUTODETECT) {
				mLOG(GBA_MEM, INFO, "Detected EEPROM savegame");
				GBASavedataInitEEPROM(&memory->savedata);
			}
			if (memory->savedata.type == SAVEDATA_EEPROM512 || memory->savedata.type == SAVEDATA_EEPROM) {
				GBASavedataWriteEEPROM(&memory->savedata, memory->dmaTransferRegister, info->nextCount);
			}
		} else {
			cpu->memory.store16(cpu, dest, memory->dmaTransferRegister, 0);
		}
	}
	gba->bus = memory->dmaTransferRegister;

	int sourceOffset;
	if (info->nextSource >= GBA_BASE_ROM0 && info->nextSource < GBA_BASE_SRAM && GBADMARegisterGetSrcControl(info->reg) < 3) {
		sourceOffset = width;
	} else {
		sourceOffset = DMA_OFFSET[GBADMARegisterGetSrcControl(info->reg)] * width;
	}
	int destOffset = DMA_OFFSET[GBADMARegisterGetDestControl(info->reg)] * width;
	if (source) {
		info->nextSource += sourceOffset;
	}
	info->nextDest += destOffset;
	--info->nextCount;

	gba->performingDMA = 0;

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBADMA* dma = &memory->dma[i];
		int32_t time = dma->when - info->when;
		if (time < 0 && GBADMARegisterIsEnable(dma->reg) && dma->nextCount) {
			dma->when = info->when;
		}
	}

	if (!info->nextCount) {
		info->nextCount |= 0x80000000;
		if (sourceRegion < GBA_REGION_ROM0 || destRegion < GBA_REGION_ROM0) {
			info->when += 2;
		}
	}
	GBADMAUpdate(gba);
}
