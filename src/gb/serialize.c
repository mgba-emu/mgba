/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/serialize.h>

#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/timer.h>
#include <mgba/internal/lr35902/lr35902.h>

#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(GB_STATE, "GB Savestate", "gb.serialize");

const uint32_t GB_SAVESTATE_MAGIC = 0x00400000;
const uint32_t GB_SAVESTATE_VERSION = 0x00000002;

static void GBSGBSerialize(struct GB* gb, struct GBSerializedState* state);
static void GBSGBDeserialize(struct GB* gb, const struct GBSerializedState* state);

void GBSerialize(struct GB* gb, struct GBSerializedState* state) {
	STORE_32LE(GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, 0, &state->versionMagic);
	STORE_32LE(gb->romCrc32, 0, &state->romCrc32);
	STORE_32LE(gb->timing.masterCycles, 0, &state->masterCycles);

	if (gb->memory.rom) {
		memcpy(state->title, ((struct GBCartridge*) &gb->memory.rom[0x100])->titleLong, sizeof(state->title));
	} else {
		memset(state->title, 0, sizeof(state->title));
	}

	state->model = gb->model;

	state->cpu.a = gb->cpu->a;
	state->cpu.f = gb->cpu->f.packed;
	state->cpu.b = gb->cpu->b;
	state->cpu.c = gb->cpu->c;
	state->cpu.d = gb->cpu->d;
	state->cpu.e = gb->cpu->e;
	state->cpu.h = gb->cpu->h;
	state->cpu.l = gb->cpu->l;
	STORE_16LE(gb->cpu->sp, 0, &state->cpu.sp);
	STORE_16LE(gb->cpu->pc, 0, &state->cpu.pc);

	STORE_32LE(gb->cpu->cycles, 0, &state->cpu.cycles);
	STORE_32LE(gb->cpu->nextEvent, 0, &state->cpu.nextEvent);

	STORE_16LE(gb->cpu->index, 0, &state->cpu.index);
	state->cpu.bus = gb->cpu->bus;
	state->cpu.executionState = gb->cpu->executionState;

	GBSerializedCpuFlags flags = 0;
	flags = GBSerializedCpuFlagsSetCondition(flags, gb->cpu->condition);
	flags = GBSerializedCpuFlagsSetIrqPending(flags, gb->cpu->irqPending);
	flags = GBSerializedCpuFlagsSetDoubleSpeed(flags, gb->doubleSpeed);
	flags = GBSerializedCpuFlagsSetEiPending(flags, mTimingIsScheduled(&gb->timing, &gb->eiPending));
	STORE_32LE(flags, 0, &state->cpu.flags);
	STORE_32LE(gb->eiPending.when - mTimingCurrentTime(&gb->timing), 0, &state->cpu.eiPending);

	GBMemorySerialize(gb, state);
	GBIOSerialize(gb, state);
	GBVideoSerialize(&gb->video, state);
	GBTimerSerialize(&gb->timer, state);
	GBAudioSerialize(&gb->audio, state);

	if (gb->model == GB_MODEL_SGB) {
		GBSGBSerialize(gb, state);
	}
}

bool GBDeserialize(struct GB* gb, const struct GBSerializedState* state) {
	bool error = false;
	int32_t check;
	uint32_t ucheck;
	int16_t check16;
	uint16_t ucheck16;
	LOAD_32LE(ucheck, 0, &state->versionMagic);
	if (ucheck > GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION) {
		mLOG(GB_STATE, WARN, "Invalid or too new savestate: expected %08X, got %08X", GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, ucheck);
		error = true;
	} else if (ucheck < GB_SAVESTATE_MAGIC) {
		mLOG(GB_STATE, WARN, "Invalid savestate: expected %08X, got %08X", GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, ucheck);
		error = true;
	} else if (ucheck < GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION) {
		mLOG(GB_STATE, WARN, "Old savestate: expected %08X, got %08X, continuing anyway", GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, ucheck);
	}
	bool canSgb = ucheck >= GB_SAVESTATE_MAGIC + 2;

	if (gb->memory.rom && memcmp(state->title, ((struct GBCartridge*) &gb->memory.rom[0x100])->titleLong, sizeof(state->title))) {
		LOAD_32LE(ucheck, 0, &state->versionMagic);
		if (ucheck > GB_SAVESTATE_MAGIC + 2 || memcmp(state->title, ((struct GBCartridge*) gb->memory.rom)->titleLong, sizeof(state->title))) {
			// There was a bug in previous versions where the memory address being compared was wrong
			mLOG(GB_STATE, WARN, "Savestate is for a different game");
			error = true;
		}
	}
	LOAD_32LE(ucheck, 0, &state->romCrc32);
	if (ucheck != gb->romCrc32) {
		mLOG(GB_STATE, WARN, "Savestate is for a different version of the game");
	}
	LOAD_32LE(check, 0, &state->cpu.cycles);
	if (check < 0) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: CPU cycles are negative");
		error = true;
	}
	if (state->cpu.executionState != LR35902_CORE_FETCH) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: Execution state is not FETCH");
		error = true;
	}
	if (check >= (int32_t) DMG_LR35902_FREQUENCY) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: CPU cycles are too high");
		error = true;
	}
	LOAD_16LE(check16, 0, &state->video.x);
	if (check16 < 0 || check16 > GB_VIDEO_HORIZONTAL_PIXELS) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: video x is out of range");
		error = true;
	}
	LOAD_16LE(check16, 0, &state->video.ly);
	if (check16 < 0 || check16 > GB_VIDEO_VERTICAL_TOTAL_PIXELS) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: video y is out of range");
		error = true;
	}
	LOAD_16LE(ucheck16, 0, &state->memory.dmaDest);
	if (ucheck16 + state->memory.dmaRemaining > GB_SIZE_OAM) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: DMA destination is out of range");
		error = true;
	}
	LOAD_16LE(ucheck16, 0, &state->video.bcpIndex);
	if (ucheck16 >= 0x40) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: BCPS is out of range");
	}
	LOAD_16LE(ucheck16, 0, &state->video.ocpIndex);
	if (ucheck16 >= 0x40) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: OCPS is out of range");
	}
	if (error) {
		return false;
	}
	gb->timing.root = NULL;
	LOAD_32LE(gb->timing.masterCycles, 0, &state->masterCycles);

	gb->cpu->a = state->cpu.a;
	gb->cpu->f.packed = state->cpu.f;
	gb->cpu->b = state->cpu.b;
	gb->cpu->c = state->cpu.c;
	gb->cpu->d = state->cpu.d;
	gb->cpu->e = state->cpu.e;
	gb->cpu->h = state->cpu.h;
	gb->cpu->l = state->cpu.l;
	LOAD_16LE(gb->cpu->sp, 0, &state->cpu.sp);
	LOAD_16LE(gb->cpu->pc, 0, &state->cpu.pc);

	LOAD_16LE(gb->cpu->index, 0, &state->cpu.index);
	gb->cpu->bus = state->cpu.bus;
	gb->cpu->executionState = state->cpu.executionState;

	GBSerializedCpuFlags flags;
	LOAD_32LE(flags, 0, &state->cpu.flags);
	gb->cpu->condition = GBSerializedCpuFlagsGetCondition(flags);
	gb->cpu->irqPending = GBSerializedCpuFlagsGetIrqPending(flags);
	gb->doubleSpeed = GBSerializedCpuFlagsGetDoubleSpeed(flags);
	gb->audio.timingFactor = gb->doubleSpeed + 1;

	LOAD_32LE(gb->cpu->cycles, 0, &state->cpu.cycles);
	LOAD_32LE(gb->cpu->nextEvent, 0, &state->cpu.nextEvent);
	gb->timing.root = NULL;

	uint32_t when;
	LOAD_32LE(when, 0, &state->cpu.eiPending);
	if (GBSerializedCpuFlagsIsEiPending(flags)) {
		mTimingSchedule(&gb->timing, &gb->eiPending, when);
	}

	enum GBModel oldModel = gb->model;
	gb->model = state->model;

	if (gb->model < GB_MODEL_CGB) {
		gb->audio.style = GB_AUDIO_DMG;
	} else {
		gb->audio.style = GB_AUDIO_CGB;
	}

	if (gb->model != GB_MODEL_SGB || oldModel != GB_MODEL_SGB) {
		gb->video.sgbBorders = false;
	}

	GBMemoryDeserialize(gb, state);
	GBVideoDeserialize(&gb->video, state);
	GBIODeserialize(gb, state);
	GBTimerDeserialize(&gb->timer, state);
	GBAudioDeserialize(&gb->audio, state);

	if (gb->model == GB_MODEL_SGB && canSgb) {
		GBSGBDeserialize(gb, state);
	}

	gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);

	gb->timing.reroot = gb->timing.root;
	gb->timing.root = NULL;

	return true;
}

// TODO: Reorganize SGB into its own file
void GBSGBSerialize(struct GB* gb, struct GBSerializedState* state) {
	state->sgb.command = gb->video.sgbCommandHeader;
	state->sgb.bits = gb->sgbBit;

	GBSerializedSGBFlags flags = 0;
	flags = GBSerializedSGBFlagsSetP1Bits(flags, gb->currentSgbBits);
	flags = GBSerializedSGBFlagsSetRenderMode(flags, gb->video.renderer->sgbRenderMode);
	STORE_32LE(flags, 0, &state->sgb.flags);

	memcpy(state->sgb.packet, gb->sgbPacket, sizeof(state->sgb.packet));

	if (gb->video.renderer->sgbCharRam) {
		memcpy(state->sgb.charRam, gb->video.renderer->sgbCharRam, sizeof(state->sgb.charRam));
	}
	if (gb->video.renderer->sgbMapRam) {
		memcpy(state->sgb.mapRam, gb->video.renderer->sgbMapRam, sizeof(state->sgb.mapRam));
	}
	if (gb->video.renderer->sgbPalRam) {
		memcpy(state->sgb.palRam, gb->video.renderer->sgbPalRam, sizeof(state->sgb.palRam));
	}
	if (gb->video.renderer->sgbAttributeFiles) {
		memcpy(state->sgb.atfRam, gb->video.renderer->sgbAttributeFiles, sizeof(state->sgb.atfRam));
	}
	if (gb->video.renderer->sgbAttributes) {
		memcpy(state->sgb.attributes, gb->video.renderer->sgbAttributes, sizeof(state->sgb.attributes));
	}
}

void GBSGBDeserialize(struct GB* gb, const struct GBSerializedState* state) {
	gb->video.sgbCommandHeader = state->sgb.command;
	gb->sgbBit = state->sgb.bits;

	GBSerializedSGBFlags flags;
	LOAD_32LE(flags, 0, &state->sgb.flags);
	gb->currentSgbBits = GBSerializedSGBFlagsGetP1Bits(flags);
	gb->video.renderer->sgbRenderMode = GBSerializedSGBFlagsGetRenderMode(flags);

	memcpy(gb->sgbPacket, state->sgb.packet, sizeof(state->sgb.packet));

	if (!gb->video.renderer->sgbCharRam) {
		gb->video.renderer->sgbCharRam = anonymousMemoryMap(SGB_SIZE_CHAR_RAM);
	}
	if (!gb->video.renderer->sgbMapRam) {
		gb->video.renderer->sgbMapRam = anonymousMemoryMap(SGB_SIZE_MAP_RAM);
	}
	if (!gb->video.renderer->sgbPalRam) {
		gb->video.renderer->sgbPalRam = anonymousMemoryMap(SGB_SIZE_PAL_RAM);
	}
	if (!gb->video.renderer->sgbAttributeFiles) {
		gb->video.renderer->sgbAttributeFiles = anonymousMemoryMap(SGB_SIZE_ATF_RAM);
	}
	if (!gb->video.renderer->sgbAttributes) {
		gb->video.renderer->sgbAttributes = malloc(90 * 45);
	}

	memcpy(gb->video.renderer->sgbCharRam, state->sgb.charRam, sizeof(state->sgb.charRam));
	memcpy(gb->video.renderer->sgbMapRam, state->sgb.mapRam, sizeof(state->sgb.mapRam));
	memcpy(gb->video.renderer->sgbPalRam, state->sgb.palRam, sizeof(state->sgb.palRam));
	memcpy(gb->video.renderer->sgbAttributeFiles, state->sgb.atfRam, sizeof(state->sgb.atfRam));
	memcpy(gb->video.renderer->sgbAttributes, state->sgb.attributes, sizeof(state->sgb.attributes));

	GBVideoWriteSGBPacket(&gb->video, (uint8_t[16]) { (SGB_ATRC_EN << 3) | 1, 0 });
	GBVideoWriteSGBPacket(&gb->video, gb->sgbPacket);
}
