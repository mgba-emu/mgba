/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/serialize.h>

#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/timer.h>
#include <mgba/internal/lr35902/lr35902.h>

mLOG_DEFINE_CATEGORY(GB_STATE, "GB Savestate", "gb.serialize");

const uint32_t GB_SAVESTATE_MAGIC = 0x00400000;
const uint32_t GB_SAVESTATE_VERSION = 0x00000001;

void GBSerialize(struct GB* gb, struct GBSerializedState* state) {
	STORE_32LE(GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, 0, &state->versionMagic);
	STORE_32LE(gb->romCrc32, 0, &state->romCrc32);
	STORE_32LE(gb->timing.masterCycles, 0, &state->masterCycles);

	if (gb->memory.rom) {
		memcpy(state->title, ((struct GBCartridge*) gb->memory.rom)->titleLong, sizeof(state->title));
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
	STORE_16LE(gb->cpu->irqVector, 0, &state->cpu.irqVector);

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

	if (gb->memory.rom && memcmp(state->title, ((struct GBCartridge*) gb->memory.rom)->titleLong, sizeof(state->title))) {
		mLOG(GB_STATE, WARN, "Savestate is for a different game");
		error = true;
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
	LOAD_16LE(gb->cpu->irqVector, 0, &state->cpu.irqVector);

	GBSerializedCpuFlags flags;
	LOAD_32LE(flags, 0, &state->cpu.flags);
	gb->cpu->condition = GBSerializedCpuFlagsGetCondition(flags);
	gb->cpu->irqPending = GBSerializedCpuFlagsGetIrqPending(flags);
	gb->doubleSpeed = GBSerializedCpuFlagsGetDoubleSpeed(flags);
	gb->audio.timingFactor = gb->doubleSpeed + 1;

	uint32_t when;
	LOAD_32LE(when, 0, &state->cpu.eiPending);
	if (GBSerializedCpuFlagsIsEiPending(flags)) {
		mTimingSchedule(&gb->timing, &gb->eiPending, when);
	}

	LOAD_32LE(gb->cpu->cycles, 0, &state->cpu.cycles);
	LOAD_32LE(gb->cpu->nextEvent, 0, &state->cpu.nextEvent);
	gb->timing.root = NULL;

	gb->model = state->model;

	if (gb->model < GB_MODEL_CGB) {
		gb->audio.style = GB_AUDIO_DMG;
	} else {
		gb->audio.style = GB_AUDIO_CGB;
	}

	GBMemoryDeserialize(gb, state);
	GBVideoDeserialize(&gb->video, state);
	GBIODeserialize(gb, state);
	GBTimerDeserialize(&gb->timer, state);
	GBAudioDeserialize(&gb->audio, state);

	gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);

	return true;
}
