/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/serialize.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/bios.h>
#include <mgba/internal/gba/io.h>

#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

#include <fcntl.h>

MGBA_EXPORT const uint32_t GBASavestateMagic = 0x01000000;
MGBA_EXPORT const uint32_t GBASavestateVersion = 0x00000007;

mLOG_DEFINE_CATEGORY(GBA_STATE, "GBA Savestate", "gba.serialize");

struct GBABundledState {
	struct GBASerializedState* state;
	struct mStateExtdata* extdata;
};

void GBASerialize(struct GBA* gba, struct GBASerializedState* state) {
	STORE_32(GBASavestateMagic + GBASavestateVersion, 0, &state->versionMagic);
	STORE_32(gba->biosChecksum, 0, &state->biosChecksum);
	STORE_32(gba->romCrc32, 0, &state->romCrc32);
	STORE_32(gba->timing.masterCycles, 0, &state->masterCycles);
	STORE_64LE(gba->timing.globalCycles, 0, &state->globalCycles);

	if (gba->memory.rom) {
		state->id = ((struct GBACartridge*) gba->memory.rom)->id;
		memcpy(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title));
	} else {
		state->id = 0;
		memset(state->title, 0, sizeof(state->title));
	}

	int i;
	for (i = 0; i < 16; ++i) {
		STORE_32(gba->cpu->gprs[i], i * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
	}
	STORE_32(gba->cpu->cpsr.packed, 0, &state->cpu.cpsr.packed);
	STORE_32(gba->cpu->spsr.packed, 0, &state->cpu.spsr.packed);
	STORE_32(gba->cpu->cycles, 0, &state->cpu.cycles);
	STORE_32(gba->cpu->nextEvent, 0, &state->cpu.nextEvent);
	for (i = 0; i < 6; ++i) {
		int j;
		for (j = 0; j < 7; ++j) {
			STORE_32(gba->cpu->bankedRegisters[i][j], (i * 7 + j) * sizeof(gba->cpu->bankedRegisters[0][0]), state->cpu.bankedRegisters);
		}
		STORE_32(gba->cpu->bankedSPSRs[i], i * sizeof(gba->cpu->bankedSPSRs[0]), state->cpu.bankedSPSRs);
	}

	STORE_32(gba->memory.biosPrefetch, 0, &state->biosPrefetch);
	STORE_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
	STORE_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);
	STORE_32(gba->memory.lastPrefetchedPc, 0, &state->lastPrefetchedPc);

	GBASerializedMiscFlags miscFlags = 0;
	miscFlags = GBASerializedMiscFlagsSetHalted(miscFlags, gba->cpu->halted);
	miscFlags = GBASerializedMiscFlagsSetPOSTFLG(miscFlags, gba->memory.io[REG_POSTFLG >> 1] & 1);
	if (mTimingIsScheduled(&gba->timing, &gba->irqEvent)) {
		miscFlags = GBASerializedMiscFlagsFillIrqPending(miscFlags);
		STORE_32(gba->irqEvent.when - mTimingCurrentTime(&gba->timing), 0, &state->nextIrq);
	}
	miscFlags = GBASerializedMiscFlagsSetBlocked(miscFlags, gba->cpuBlocked);
	miscFlags = GBASerializedMiscFlagsSetKeyIRQKeys(miscFlags, gba->keysLast);
	STORE_32(miscFlags, 0, &state->miscFlags);
	STORE_32(gba->biosStall, 0, &state->biosStall);

	GBAMemorySerialize(&gba->memory, state);
	GBAIOSerialize(gba, state);
	GBAVideoSerialize(&gba->video, state);
	GBAAudioSerialize(&gba->audio, state);
	GBASavedataSerialize(&gba->memory.savedata, state);

	if (gba->memory.matrix.size) {
		GBAMatrixSerialize(gba, state);
	}
}

bool GBADeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	bool error = false;
	int32_t check;
	uint32_t ucheck;
	LOAD_32(ucheck, 0, &state->versionMagic);
	if (ucheck > GBASavestateMagic + GBASavestateVersion) {
		mLOG(GBA_STATE, WARN, "Invalid or too new savestate: expected %08X, got %08X", GBASavestateMagic + GBASavestateVersion, ucheck);
		error = true;
	} else if (ucheck < GBASavestateMagic) {
		mLOG(GBA_STATE, WARN, "Invalid savestate: expected %08X, got %08X", GBASavestateMagic + GBASavestateVersion, ucheck);
		error = true;
	} else if (ucheck < GBASavestateMagic + GBASavestateVersion) {
		mLOG(GBA_STATE, WARN, "Old savestate: expected %08X, got %08X, continuing anyway", GBASavestateMagic + GBASavestateVersion, ucheck);
	}
	LOAD_32(ucheck, 0, &state->biosChecksum);
	if (ucheck != gba->biosChecksum) {
		mLOG(GBA_STATE, WARN, "Savestate created using a different version of the BIOS: expected %08X, got %08X", gba->biosChecksum, ucheck);
		uint32_t pc;
		LOAD_32(pc, ARM_PC * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
		if ((ucheck == GBA_BIOS_CHECKSUM || gba->biosChecksum == GBA_BIOS_CHECKSUM) && pc < GBA_SIZE_BIOS && pc >= 0x20) {
			error = true;
		}
	}
	if (gba->memory.rom && (state->id != ((struct GBACartridge*) gba->memory.rom)->id || memcmp(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title)))) {
		mLOG(GBA_STATE, WARN, "Savestate is for a different game");
		error = true;
	} else if (!gba->memory.rom && state->id != 0) {
		mLOG(GBA_STATE, WARN, "Savestate is for a game, but no game loaded");
		error = true;
	}
	LOAD_32(ucheck, 0, &state->romCrc32);
	if (ucheck != gba->romCrc32) {
		mLOG(GBA_STATE, WARN, "Savestate is for a different version of the game");
	}
	LOAD_32(check, 0, &state->cpu.cycles);
	if (check < 0) {
		mLOG(GBA_STATE, WARN, "Savestate is corrupted: CPU cycles are negative");
		error = true;
	}
	if (check >= (int32_t) GBA_ARM7TDMI_FREQUENCY) {
		mLOG(GBA_STATE, WARN, "Savestate is corrupted: CPU cycles are too high");
		error = true;
	}
	LOAD_32(check, ARM_PC * sizeof(state->cpu.gprs[0]), state->cpu.gprs);
	int region = (check >> BASE_OFFSET);
	if ((region == GBA_REGION_ROM0 || region == GBA_REGION_ROM1 || region == GBA_REGION_ROM2) && ((check - WORD_SIZE_ARM) & GBA_SIZE_ROM0) >= gba->memory.romSize - WORD_SIZE_ARM) {
		mLOG(GBA_STATE, WARN, "Savestate created using a differently sized version of the ROM");
		error = true;
	}
	if (error) {
		return false;
	}
	mTimingClear(&gba->timing);
	LOAD_32(gba->timing.masterCycles, 0, &state->masterCycles);
	LOAD_64LE(gba->timing.globalCycles, 0, &state->globalCycles);

	size_t i;
	for (i = 0; i < 16; ++i) {
		LOAD_32(gba->cpu->gprs[i], i * sizeof(gba->cpu->gprs[0]), state->cpu.gprs);
	}
	LOAD_32(gba->cpu->cpsr.packed, 0, &state->cpu.cpsr.packed);
	LOAD_32(gba->cpu->spsr.packed, 0, &state->cpu.spsr.packed);
	LOAD_32(gba->cpu->cycles, 0, &state->cpu.cycles);
	LOAD_32(gba->cpu->nextEvent, 0, &state->cpu.nextEvent);
	for (i = 0; i < 6; ++i) {
		int j;
		for (j = 0; j < 7; ++j) {
			LOAD_32(gba->cpu->bankedRegisters[i][j], (i * 7 + j) * sizeof(gba->cpu->bankedRegisters[0][0]), state->cpu.bankedRegisters);
		}
		LOAD_32(gba->cpu->bankedSPSRs[i], i * sizeof(gba->cpu->bankedSPSRs[0]), state->cpu.bankedSPSRs);
	}
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	if (gba->cpu->gprs[ARM_PC] & 1) {
		mLOG(GBA_STATE, WARN, "Savestate has unaligned PC and is probably corrupted");
		gba->cpu->gprs[ARM_PC] &= ~1;
	}
	gba->memory.activeRegion = -1;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	if (state->biosPrefetch) {
		LOAD_32(gba->memory.biosPrefetch, 0, &state->biosPrefetch);
	}
	LOAD_32(gba->memory.lastPrefetchedPc, 0, &state->lastPrefetchedPc);
	if (gba->cpu->cpsr.t) {
		gba->cpu->executionMode = MODE_THUMB;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			LOAD_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
			LOAD_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);
			gba->cpu->prefetch[0] &= 0xFFFF;
			gba->cpu->prefetch[1] &= 0xFFFF;
		} else {
			// Maintain backwards compat
			LOAD_16(gba->cpu->prefetch[0], (gba->cpu->gprs[ARM_PC] - WORD_SIZE_THUMB) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
			LOAD_16(gba->cpu->prefetch[1], (gba->cpu->gprs[ARM_PC]) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
		}
	} else {
		gba->cpu->executionMode = MODE_ARM;
		if (state->cpuPrefetch[0] && state->cpuPrefetch[1]) {
			LOAD_32(gba->cpu->prefetch[0], 0, state->cpuPrefetch);
			LOAD_32(gba->cpu->prefetch[1], 4, state->cpuPrefetch);
		} else {
			// Maintain backwards compat
			LOAD_32(gba->cpu->prefetch[0], (gba->cpu->gprs[ARM_PC] - WORD_SIZE_ARM) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
			LOAD_32(gba->cpu->prefetch[1], (gba->cpu->gprs[ARM_PC]) & gba->cpu->memory.activeMask, gba->cpu->memory.activeRegion);
		}
	}
	GBASerializedMiscFlags miscFlags = 0;
	LOAD_32(miscFlags, 0, &state->miscFlags);
	gba->cpu->halted = GBASerializedMiscFlagsGetHalted(miscFlags);
	gba->memory.io[REG_POSTFLG >> 1] = GBASerializedMiscFlagsGetPOSTFLG(miscFlags);
	if (GBASerializedMiscFlagsIsIrqPending(miscFlags)) {
		int32_t when;
		LOAD_32(when, 0, &state->nextIrq);
		mTimingSchedule(&gba->timing, &gba->irqEvent, when);		
	}
	gba->cpuBlocked = GBASerializedMiscFlagsGetBlocked(miscFlags);
	gba->keysLast = GBASerializedMiscFlagsGetKeyIRQKeys(miscFlags);
	LOAD_32(gba->biosStall, 0, &state->biosStall);

	GBAVideoDeserialize(&gba->video, state);
	GBAMemoryDeserialize(&gba->memory, state);
	GBAIODeserialize(gba, state);
	GBAAudioDeserialize(&gba->audio, state);
	GBASavedataDeserialize(&gba->memory.savedata, state);

	if (gba->memory.matrix.size) {
		GBAMatrixDeserialize(gba, state);
	}

	mTimingInterrupt(&gba->timing);

	return true;
}
