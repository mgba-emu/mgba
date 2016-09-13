/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "serialize.h"

#include "gb/io.h"
#include "gb/timer.h"

mLOG_DEFINE_CATEGORY(GB_STATE, "GB Savestate");

#ifdef _MSC_VER
#include <time.h>
#else
#include <sys/time.h>
#endif

const uint32_t GB_SAVESTATE_MAGIC = 0x00400000;
const uint32_t GB_SAVESTATE_VERSION = 0x00000000;

void GBSerialize(struct GB* gb, struct GBSerializedState* state) {
	STORE_32LE(GB_SAVESTATE_MAGIC + GB_SAVESTATE_VERSION, 0, &state->versionMagic);
	STORE_32LE(gb->romCrc32, 0, &state->romCrc32);

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

	STORE_32LE(gb->eiPending, 0, &state->cpu.eiPending);

	GBSerializedCpuFlags flags = 0;
	flags = GBSerializedCpuFlagsSetCondition(flags, gb->cpu->condition);
	flags = GBSerializedCpuFlagsSetIrqPending(flags, gb->cpu->irqPending);
	flags = GBSerializedCpuFlagsSetDoubleSpeed(flags, gb->doubleSpeed);
	STORE_32LE(flags, 0, &state->cpu.flags);

	GBMemorySerialize(gb, state);
	GBIOSerialize(gb, state);
	GBVideoSerialize(&gb->video, state);
	GBTimerSerialize(&gb->timer, state);
	GBAudioSerialize(&gb->audio, state);

#ifndef _MSC_VER
	struct timeval tv;
	if (!gettimeofday(&tv, 0)) {
		uint64_t usec = tv.tv_usec;
		usec += tv.tv_sec * 1000000LL;
		STORE_64LE(usec, 0, &state->creationUsec);
	}
#else
	struct timespec ts;
	if (timespec_get(&ts, TIME_UTC)) {
		uint64_t usec = ts.tv_nsec / 1000;
		usec += ts.tv_sec * 1000000LL;
		STORE_64LE(usec, 0, &state->creationUsec);
	}
#endif
	else {
		state->creationUsec = 0;
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
	LOAD_32LE(check, 0, &state->video.eventDiff);
	if (check < 0) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: video eventDiff is negative");
		error = true;
	}
	LOAD_16LE(check16, 0, &state->video.ly);
	if (check16 < 0 || check16 > GB_VIDEO_VERTICAL_TOTAL_PIXELS) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: video y is out of range");
		error = true;
	}
	LOAD_16LE(ucheck16, 0, &state->memory.dmaDest);
	if (ucheck16 >= GB_SIZE_OAM) {
		mLOG(GB_STATE, WARN, "Savestate is corrupted: DMA destination is out of range");
		error = true;
	}
	if (error) {
		return false;
	}

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

	LOAD_32LE(gb->eiPending, 0, &state->cpu.eiPending);

	GBSerializedCpuFlags flags;
	LOAD_32LE(flags, 0, &state->cpu.flags);
	gb->cpu->condition = GBSerializedCpuFlagsGetCondition(flags);
	gb->cpu->irqPending = GBSerializedCpuFlagsGetIrqPending(flags);
	gb->doubleSpeed = GBSerializedCpuFlagsGetDoubleSpeed(flags);

	LOAD_32LE(gb->cpu->cycles, 0, &state->cpu.cycles);
	LOAD_32LE(gb->cpu->nextEvent, 0, &state->cpu.nextEvent);

	gb->model = state->model;

	if (gb->model < GB_MODEL_CGB) {
		gb->audio.style = GB_AUDIO_DMG;
	} else {
		gb->audio.style = GB_AUDIO_CGB;
	}

	GBMemoryDeserialize(gb, state);
	GBIODeserialize(gb, state);
	GBVideoDeserialize(&gb->video, state);
	GBTimerDeserialize(&gb->timer, state);
	GBAudioDeserialize(&gb->audio, state);

	gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);

	return true;
}
