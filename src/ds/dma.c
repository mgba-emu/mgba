/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/dma.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>
#include <mgba/internal/ds/memory.h>

static void _dmaEvent(struct mTiming* timing, void* context, uint32_t cyclesLate);

static void DSDMAService(struct DSCommon* dscore, int number, struct GBADMA* info);

static const int DMA_OFFSET[] = { 1, -1, 0, 1 };

void DSDMAInit(struct DS* ds) {
	ds->ds7.memory.dmaEvent.name = "DS7 DMA";
	ds->ds7.memory.dmaEvent.callback = _dmaEvent;
	ds->ds7.memory.dmaEvent.context = &ds->ds7;
	ds->ds7.memory.dmaEvent.priority = 0x40;
	ds->ds9.memory.dmaEvent.name = "DS9 DMA";
	ds->ds9.memory.dmaEvent.callback = _dmaEvent;
	ds->ds9.memory.dmaEvent.context = &ds->ds9;
	ds->ds9.memory.dmaEvent.priority = 0x40;
}

void DSDMAReset(struct DSCommon* dscore) {
	memset(dscore->memory.dma, 0, sizeof(dscore->memory.dma));
	int i;
	for (i = 0; i < 4; ++i) {
		// TODO: This is wrong for DS7
		dscore->memory.dma[i].count = 0x200000;
	}
	dscore->memory.activeDMA = -1;
}

uint32_t DSDMAWriteSAD(struct DSCommon* dscore, int dma, uint32_t address) {
	address &= 0x0FFFFFFE;
	dscore->memory.dma[dma].source = address;
	return dscore->memory.dma[dma].source;
}

uint32_t DSDMAWriteDAD(struct DSCommon* dscore, int dma, uint32_t address) {
	address &= 0x0FFFFFFE;
	dscore->memory.dma[dma].dest = address;
	return dscore->memory.dma[dma].dest;
}

void DS7DMAWriteCNT(struct DSCommon* dscore, int dma, uint32_t value) {
	struct DSCoreMemory* memory = &dscore->memory;
	struct GBADMA* currentDma = &memory->dma[dma];
	uint32_t count = value & 0xFFFF;
	currentDma->count = count ? count : (dma == 3 ? 0x10000 : 0x4000);
	int wasEnabled = GBADMARegisterIsEnable(currentDma->reg);
	unsigned control = (value >> 16) & 0xF7E0;
	currentDma->reg = control;

	if (!wasEnabled && GBADMARegisterIsEnable(currentDma->reg)) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;
		DSDMASchedule(dscore, dma, currentDma);
	}
}

void DS9DMAWriteCNT(struct DSCommon* dscore, int dma, uint32_t value) {
	struct DSCoreMemory* memory = &dscore->memory;
	struct GBADMA* currentDma = &memory->dma[dma];
	uint32_t count = value & 0x1FFFFF;
	currentDma->count = count ? count : 0x200000;
	int wasEnabled = GBADMARegisterIsEnable(currentDma->reg);
	unsigned control = (value >> 16) & 0xFFE0;
	currentDma->reg = control;

	if (!wasEnabled && GBADMARegisterIsEnable(currentDma->reg)) {
		currentDma->nextSource = currentDma->source;
		currentDma->nextDest = currentDma->dest;
		DSDMASchedule(dscore, dma, currentDma);
	}
}

void DSDMASchedule(struct DSCommon* dscore, int number, struct GBADMA* info) {
	int which;
	if (dscore == &dscore->p->ds9) {
		which = GBADMARegisterGetTiming9(info->reg);
	} else {
		which = GBADMARegisterGetTiming(info->reg);
	}
	switch (which) {
	case DS_DMA_TIMING_NOW:
		info->when = mTimingCurrentTime(&dscore->timing) + 3; // DMAs take 3 cycles to start
		info->nextCount = info->count;
		break;
	case DS_DMA_TIMING_VBLANK:
		// Handled implicitly
		return;
	case DS9_DMA_TIMING_SLOT1:
		DSSlot1ScheduleDMA(dscore, number, info);
		return;
	case DS_DMA_TIMING_HBLANK: // DS7_DMA_TIMING_SLOT1
	default:
		mLOG(DS_MEM, STUB, "Unimplemented DMA");
	}
	DSDMAUpdate(dscore);
}

void _dmaEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DSCommon* dscore = context;
	struct DSCoreMemory* memory = &dscore->memory;
	struct GBADMA* dma = &memory->dma[memory->activeDMA];
	if (dma->nextCount == dma->count) {
		dma->when = mTimingCurrentTime(&dscore->timing);
	}
	if (dma->nextCount & 0xFFFFF) {
		if (dscore->p->cpuBlocked & ~DS_CPU_BLOCK_DMA) {
			// Delay DMA until after the CPU unblocks
			dma->when = mTimingCurrentTime(&dscore->timing) + mTimingNextEvent(&dscore->timing) + 1;
			DSDMAUpdate(dscore);
		} else {
			dscore->p->cpuBlocked |= DS_CPU_BLOCK_DMA; // TODO: Fix for ITCM
			DSDMAService(dscore, memory->activeDMA, dma);
		}
	} else {
		dma->nextCount = 0;
		if (!GBADMARegisterIsRepeat(dma->reg) || GBADMARegisterGetTiming(dma->reg) == DMA_TIMING_NOW) {
			dma->reg = GBADMARegisterClearEnable(dma->reg);

			// Clear the enable bit in memory
			memory->io[(DS_REG_DMA0CNT_HI + memory->activeDMA * (DS_REG_DMA1CNT_HI - DS_REG_DMA0CNT_HI)) >> 1] &= 0x7FFF;
		}
		if (GBADMARegisterGetDestControl(dma->reg) == DMA_INCREMENT_RELOAD) {
			dma->nextDest = dma->dest;
		}
		if (GBADMARegisterIsDoIRQ(dma->reg)) {
			DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_DMA0 + memory->activeDMA);
		}
		DSDMAUpdate(dscore);
	}
}

void DSDMAUpdate(struct DSCommon* dscore) {
	int i;
	struct DSCoreMemory* memory = &dscore->memory;
	memory->activeDMA = -1;
	uint32_t currentTime = mTimingCurrentTime(&dscore->timing);
	for (i = 0; i < 4; ++i) {
		struct GBADMA* dma = &memory->dma[i];
		if (GBADMARegisterIsEnable(dma->reg) && dma->nextCount) {
			memory->activeDMA = i;
			break;
		}
	}

	if (memory->activeDMA >= 0) {
		mTimingDeschedule(&dscore->timing, &memory->dmaEvent);
		mTimingSchedule(&dscore->timing, &memory->dmaEvent, memory->dma[memory->activeDMA].when - currentTime);
	} else {
		dscore->p->cpuBlocked &= ~DS_CPU_BLOCK_DMA;
	}
}

void DSDMAService(struct DSCommon* dscore, int number, struct GBADMA* info) {
	struct DSCoreMemory* memory = &dscore->memory;
	struct ARMCore* cpu = dscore->cpu;
	uint32_t width = 2 << GBADMARegisterGetWidth(info->reg);
	int32_t wordsRemaining = info->nextCount;
	uint32_t source = info->nextSource;
	uint32_t dest = info->nextDest;
	uint32_t sourceRegion = source >> DS_BASE_OFFSET;
	uint32_t destRegion = dest >> DS_BASE_OFFSET;
	int32_t cycles = 2;

	if (info->count == info->nextCount) {
		if (width == 4) {
			cycles += dscore->memory.waitstatesNonseq32[sourceRegion] + dscore->memory.waitstatesNonseq32[destRegion];
		} else {
			cycles += dscore->memory.waitstatesNonseq16[sourceRegion] + dscore->memory.waitstatesNonseq16[destRegion];
		}
		source &= -width;
		dest &= -width;
	} else {
		if (width == 4) {
			cycles += dscore->memory.waitstatesSeq32[sourceRegion] + dscore->memory.waitstatesSeq32[destRegion];
		} else {
			cycles += dscore->memory.waitstatesSeq16[sourceRegion] + dscore->memory.waitstatesSeq16[destRegion];
		}
	}
	info->when += cycles;

	uint32_t word;
	if (width == 4) {
		word = cpu->memory.load32(cpu, source, 0);
		cpu->memory.store32(cpu, dest, word, 0);
	} else {
		word = cpu->memory.load16(cpu, source, 0);
		cpu->memory.store16(cpu, dest, word, 0);
	}
	int sourceOffset = DMA_OFFSET[GBADMARegisterGetSrcControl(info->reg)] * width;
	int destOffset = DMA_OFFSET[GBADMARegisterGetDestControl(info->reg)] * width;
	source += sourceOffset;
	dest += destOffset;
	--wordsRemaining;

	info->nextCount = wordsRemaining;
	info->nextSource = source;
	info->nextDest = dest;
	if (!wordsRemaining) {
		info->nextCount |= 0x80000000;
	}
	DSDMAUpdate(dscore);
}
