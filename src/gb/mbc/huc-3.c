/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/gb/gb.h>
#include <mgba-util/vfs.h>

static void _latchHuC3Rtc(struct mRTCSource* rtc, uint8_t* huc3Regs, time_t* rtcLastLatch) {
	time_t t;
	if (rtc) {
		if (rtc->sample) {
			rtc->sample(rtc);
		}
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	t -= *rtcLastLatch;
	t /= 60;

	if (!t) {
		return;
	}
	*rtcLastLatch += t * 60;

	int minutes = huc3Regs[GBHUC3_RTC_MINUTES_HI] << 8;
	minutes |= huc3Regs[GBHUC3_RTC_MINUTES_MI] << 4;
	minutes |= huc3Regs[GBHUC3_RTC_MINUTES_LO];
	minutes += t % 1440;
	t /= 1440;
	if (minutes >= 1440) {
		minutes -= 1440;
		++t;
	} else if (minutes < 0) {
		minutes += 1440;
		--t;
	}
	huc3Regs[GBHUC3_RTC_MINUTES_LO] = minutes & 0xF;
	huc3Regs[GBHUC3_RTC_MINUTES_MI] = (minutes >> 4) & 0xF;
	huc3Regs[GBHUC3_RTC_MINUTES_HI] = (minutes >> 8) & 0xF;

	int days = huc3Regs[GBHUC3_RTC_DAYS_LO];
	days |= huc3Regs[GBHUC3_RTC_DAYS_MI] << 4;
	days |= huc3Regs[GBHUC3_RTC_DAYS_HI] << 8;

	days += t;

	huc3Regs[GBHUC3_RTC_DAYS_LO] = days & 0xF;
	huc3Regs[GBHUC3_RTC_DAYS_MI] = (days >> 4) & 0xF;
	huc3Regs[GBHUC3_RTC_DAYS_HI] = (days >> 8) & 0xF;
}

static void _huc3Commit(struct GB* gb, struct GBHuC3State* state) {
	size_t c;
	switch (state->value & 0x70) {
	case 0x10:
		if ((state->index & 0xF8) == 0x10) {
			_latchHuC3Rtc(gb->memory.rtc, state->registers, &gb->memory.rtcLastLatch);
		}
		state->value &= 0xF0;
		state->value |= state->registers[state->index] & 0xF;
		mLOG(GB_MBC, DEBUG, "HuC-3 read: %02X:%X", state->index, state->value & 0xF);
		if (state->value & 0x10) {
			++state->index;
		}
		break;
	case 0x30:
		mLOG(GB_MBC, DEBUG, "HuC-3 write: %02X:%X", state->index, state->value & 0xF);
		state->registers[state->index] = state->value & 0xF;
		if (state->value & 0x10) {
			++state->index;
		}
		break;
	case 0x40:
		state->index &= 0xF0;
		state->index |= (state->value) & 0xF;
		mLOG(GB_MBC, DEBUG, "HuC-3 index (low): %02X", state->index);
		break;
	case 0x50:
		state->index &= 0x0F;
		state->index |= ((state->value) & 0xF) << 4;
		mLOG(GB_MBC, DEBUG, "HuC-3 index (high): %02X", state->index);
		break;
	case 0x60:
		switch (state->value & 0xF) {
		case GBHUC3_CMD_LATCH:
			_latchHuC3Rtc(gb->memory.rtc, state->registers, &gb->memory.rtcLastLatch);
			memcpy(state->registers, &state->registers[GBHUC3_RTC_MINUTES_LO], 6);
			mLOG(GB_MBC, DEBUG, "HuC-3 RTC latch");
			break;
		case GBHUC3_CMD_SET_RTC:
			memcpy(&state->registers[GBHUC3_RTC_MINUTES_LO], state->registers, 6);
			mLOG(GB_MBC, DEBUG, "HuC-3 set RTC");
			break;
		case GBHUC3_CMD_RO:
			mLOG(GB_MBC, STUB, "HuC-3 unimplemented read-only mode");
			break;
		case GBHUC3_CMD_TONE:
			if (state->registers[GBHUC3_SPEAKER_ENABLE] == 1) {
				for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
					struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
					if (callbacks->alarm) {
						callbacks->alarm(callbacks->context);
					}
				}
				mLOG(GB_MBC, DEBUG, "HuC-3 tone %i", state->registers[GBHUC3_SPEAKER_TONE] & 3);
			}
			break;
		default:
			mLOG(GB_MBC, STUB, "HuC-3 unknown command: %X", state->value & 0xF);
			break;
		}
		state->value = 0xE1;
		break;
	default:
		mLOG(GB_MBC, STUB, "HuC-3 unknown mode commit: %02X:%02X", state->index, state->value);
		break;
	}
}

void _GBHuC3(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	struct GBHuC3State* state = &memory->mbcState.huc3;
	int bank = value & 0x7F;
	if (address & 0x1FFF) {
		mLOG(GB_MBC, STUB, "HuC-3 unknown value %04X:%02X", address, value);
	}

	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			memory->sramAccess = false;
			break;
		}
		state->mode = value;
		break;
	case 0x1:
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x2:
		GBMBCSwitchSramBank(gb, bank);
		break;
	case 0x5:
		switch (state->mode) {
		case GBHUC3_MODE_IN:
			state->value = 0x80 | value;
			break;
		case GBHUC3_MODE_COMMIT:
			_huc3Commit(gb, state);
			break;
		default:
			mLOG(GB_MBC, STUB, "HuC-3 unknown mode write: %02X:%02X", state->mode, value);
		}
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "HuC-3 unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBHuC3Read(struct GBMemory* memory, uint16_t address) {
	struct GBHuC3State* state = &memory->mbcState.huc3;
	switch (state->mode) {
	case GBHUC3_MODE_SRAM_RO:
	case GBHUC3_MODE_SRAM_RW:
		return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
	case GBHUC3_MODE_IN:
	case GBHUC3_MODE_OUT:
		return 0x80 | state->value;
	default:
		return 0xFF;
	}
}

void GBMBCHuC3Read(struct GB* gb) {
	struct GBMBCHuC3SaveBuffer buffer;
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}
	vf->seek(vf, gb->sramSize, SEEK_SET);
	if (vf->read(vf, &buffer, sizeof(buffer)) < (ssize_t) sizeof(buffer)) {
		return;
	}

	size_t i;
	for (i = 0; i < 0x80; ++i) {
		gb->memory.mbcState.huc3.registers[i * 2] = buffer.regs[i] & 0xF;
		gb->memory.mbcState.huc3.registers[i * 2 + 1] = buffer.regs[i] >> 4;
	}
	LOAD_64LE(gb->memory.rtcLastLatch, 0, &buffer.latchedUnix);
}

void GBMBCHuC3Write(struct GB* gb) {
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}

	struct GBMBCHuC3SaveBuffer buffer;
	size_t i;
	for (i = 0; i < 0x80; ++i) {
		buffer.regs[i] = gb->memory.mbcState.huc3.registers[i * 2] & 0xF;
		buffer.regs[i] |= gb->memory.mbcState.huc3.registers[i * 2 + 1] << 4;
	}
	STORE_64LE(gb->memory.rtcLastLatch, 0, &buffer.latchedUnix);

	_GBMBCAppendSaveSuffix(gb, &buffer, sizeof(buffer));
}
