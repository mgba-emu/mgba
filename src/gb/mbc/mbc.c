/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/core/interface.h>
#include <mgba/internal/defines.h>
#include <mgba/internal/gb/gb.h>

static void _GBMBC6MapChip(struct GB*, int half, uint8_t value);

void _GBMBCLatchRTC(struct mRTCSource* rtc, uint8_t* rtcRegs, time_t* rtcLastLatch) {
	time_t t;
	if (rtc) {
		if (rtc->sample) {
			rtc->sample(rtc);
		}
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	time_t currentLatch = t;
	t -= *rtcLastLatch;
	*rtcLastLatch = currentLatch;

	int64_t diff;
	diff = rtcRegs[0] + t % 60;
	if (diff < 0) {
		diff += 60;
		t -= 60;
	}
	rtcRegs[0] = diff % 60;
	t /= 60;
	t += diff / 60;

	diff = rtcRegs[1] + t % 60;
	if (diff < 0) {
		diff += 60;
		t -= 60;
	}
	rtcRegs[1] = diff % 60;
	t /= 60;
	t += diff / 60;

	diff = rtcRegs[2] + t % 24;
	if (diff < 0) {
		diff += 24;
		t -= 24;
	}
	rtcRegs[2] = diff % 24;
	t /= 24;
	t += diff / 24;

	diff = rtcRegs[3] + ((rtcRegs[4] & 1) << 8) + (t & 0x1FF);
	rtcRegs[3] = diff;
	rtcRegs[4] &= 0xFE;
	rtcRegs[4] |= (diff >> 8) & 1;
	if (diff & 0x200) {
		rtcRegs[4] |= 0x80;
	}
}

static void _GBMBC1Update(struct GB* gb) {
	struct GBMBC1State* state = &gb->memory.mbcState.mbc1;
	int bank = state->bankLo;
	bank &= (1 << state->multicartStride) - 1;
	bank |= state->bankHi << state->multicartStride;
	if (state->mode) {
		GBMBCSwitchBank0(gb, state->bankHi << state->multicartStride);
		GBMBCSwitchSramBank(gb, state->bankHi & 3);
	} else {
		GBMBCSwitchBank0(gb, 0);
		GBMBCSwitchSramBank(gb, 0);
	}
	if (!(state->bankLo & 0x1F)) {
		++state->bankLo;
		++bank;
	}
	GBMBCSwitchBank(gb, bank);
}

void _GBMBC1(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank = value & 0x1F;
	switch (address >> 13) {
	case 0x0:
		switch (value & 0xF) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC1 unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		memory->mbcState.mbc1.bankLo = bank;
		_GBMBC1Update(gb);
		break;
	case 0x2:
		bank &= 3;
		memory->mbcState.mbc1.bankHi = bank;
		_GBMBC1Update(gb);
		break;
	case 0x3:
		memory->mbcState.mbc1.mode = value & 1;
		_GBMBC1Update(gb);
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC1 unknown address: %04X:%02X", address, value);
		break;
	}
}

void _GBMBC2(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int shift = (address & 1) * 4;
	int bank = value & 0xF;
	switch ((address & 0xC100) >> 8) {
	case 0x0:
		switch (value & 0x0F) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC2 unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		if (!bank) {
			++bank;
		}
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
		if (!memory->sramAccess) {
			return;
		}
		address &= 0x1FF;
		memory->sramBank[(address >> 1)] &= 0xF0 >> shift;
		memory->sramBank[(address >> 1)] |= (value & 0xF) << shift;
		gb->sramDirty |= mSAVEDATA_DIRT_NEW;
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC2 unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBMBC2Read(struct GBMemory* memory, uint16_t address) {
	if (!memory->sramAccess) {
		return 0xFF;
	}
	address &= 0x1FF;
	int shift = (address & 1) * 4;
	return (memory->sramBank[(address >> 1)] >> shift) | 0xF0;
}

void _GBMBC3(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank = value;
	switch (address >> 13) {
	case 0x0:
		switch (value & 0xF) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC3 unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		if (gb->memory.romSize < GB_SIZE_CART_BANK0 * 0x80) {
			bank &= 0x7F;
		}
		if (!bank) {
			++bank;
		}
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x2:
		bank &= 0xF;
		if (bank < 8) {
			GBMBCSwitchSramBank(gb, value);
			memory->rtcAccess = false;
		} else if (bank <= 0xC) {
			memory->activeRtcReg = bank - 8;
			memory->rtcAccess = true;
		}
		break;
	case 0x3:
		if (memory->rtcLatched && value == 0) {
			memory->rtcLatched = false;
		} else if (!memory->rtcLatched && value == 1) {
			_GBMBCLatchRTC(gb->memory.rtc, gb->memory.rtcRegs, &gb->memory.rtcLastLatch);
			memory->rtcLatched = true;
		}
		break;
	}
}

void _GBMBC5(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank;
	switch (address >> 12) {
	case 0x0:
	case 0x1:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC5 unknown value %02X", value);
			break;
		}
		break;
	case 0x2:
		bank = (memory->currentBank & 0x100) | value;
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x3:
		bank = (memory->currentBank & 0xFF) | ((value & 1) << 8);
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x4:
	case 0x5:
		if (memory->mbcType == GB_MBC5_RUMBLE && memory->rumble) {
			memory->rumble->setRumble(memory->rumble, (value >> 3) & 1);
			value &= ~8;
		}
		GBMBCSwitchSramBank(gb, value & 0xF);
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC5 unknown address: %04X:%02X", address, value);
		break;
	}
}

void _GBMBC6(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank = value;
	switch (address >> 10) {
	case 0:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC6 unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		GBMBCSwitchSramHalfBank(gb, 0, bank);
		break;
	case 0x2:
		GBMBCSwitchSramHalfBank(gb, 1, bank);
		break;
	case 0x3:
		mLOG(GB_MBC, STUB, "MBC6 unimplemented flash OE write: %04X:%02X", address, value);
		break;
	case 0x4:
		mLOG(GB_MBC, STUB, "MBC6 unimplemented flash WE write: %04X:%02X", address, value);
		break;
	case 0x8:
	case 0x9:
		GBMBCSwitchHalfBank(gb, 0, bank);
		break;
	case 0xA:
	case 0xB:
		_GBMBC6MapChip(gb, 0, value);
		break;
	case 0xC:
	case 0xD:
		GBMBCSwitchHalfBank(gb, 1, bank);
		break;
	case 0xE:
	case 0xF:
		_GBMBC6MapChip(gb, 1, value);
		break;
	case 0x28:
	case 0x29:
	case 0x2A:
	case 0x2B:
		if (memory->sramAccess) {
			memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM_HALFBANK - 1)] = value;
			gb->sramDirty |= mSAVEDATA_DIRT_NEW;
		}
		break;
	case 0x2C:
	case 0x2D:
	case 0x2E:
	case 0x2F:
		if (memory->sramAccess) {
			memory->sramBank1[address & (GB_SIZE_EXTERNAL_RAM_HALFBANK - 1)] = value;
		}
		break;
	default:
		mLOG(GB_MBC, STUB, "MBC6 unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBMBC6Read(struct GBMemory* memory, uint16_t address) {
	if (!memory->sramAccess) {
		return 0xFF;
	}
	switch (address >> 12) {
	case 0xA:
		return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM_HALFBANK - 1)];
	case 0xB:
		return memory->sramBank1[address & (GB_SIZE_EXTERNAL_RAM_HALFBANK - 1)];
	}
	return 0xFF;
}

static void _GBMBC6MapChip(struct GB* gb, int half, uint8_t value) {
	if (!half) {
		gb->memory.mbcState.mbc6.flashBank0 = !!(value & 0x08);
		GBMBCSwitchHalfBank(gb, half, gb->memory.currentBank);
	} else {
		gb->memory.mbcState.mbc6.flashBank1 = !!(value & 0x08);
		GBMBCSwitchHalfBank(gb, half, gb->memory.currentBank1);
	}
}

void _GBMBC7(struct GB* gb, uint16_t address, uint8_t value) {
	int bank = value & 0x7F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		default:
		case 0:
			gb->memory.mbcState.mbc7.access = 0;
			break;
		case 0xA:
			gb->memory.mbcState.mbc7.access |= 1;
			break;
		}
		break;
	case 0x1:
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x2:
		if (value == 0x40) {
			gb->memory.mbcState.mbc7.access |= 2;
		} else {
			gb->memory.mbcState.mbc7.access &= ~2;
		}
		break;
	case 0x5:
		_GBMBC7Write(&gb->memory, address, value);
		gb->sramDirty |= mSAVEDATA_DIRT_NEW;
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC7 unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBMBC7Read(struct GBMemory* memory, uint16_t address) {
	struct GBMBC7State* mbc7 = &memory->mbcState.mbc7;
	if (mbc7->access != 3) {
		return 0xFF;
	}
	switch (address & 0xF0) {
	case 0x20:
		if (memory->rotation && memory->rotation->readTiltX) {
			int32_t x = -memory->rotation->readTiltX(memory->rotation);
			x >>= 21;
			x += 0x81D0;
			return x;
		}
		return 0xFF;
	case 0x30:
		if (memory->rotation && memory->rotation->readTiltX) {
			int32_t x = -memory->rotation->readTiltX(memory->rotation);
			x >>= 21;
			x += 0x81D0;
			return x >> 8;
		}
		return 7;
	case 0x40:
		if (memory->rotation && memory->rotation->readTiltY) {
			int32_t y = -memory->rotation->readTiltY(memory->rotation);
			y >>= 21;
			y += 0x81D0;
			return y;
		}
		return 0xFF;
	case 0x50:
		if (memory->rotation && memory->rotation->readTiltY) {
			int32_t y = -memory->rotation->readTiltY(memory->rotation);
			y >>= 21;
			y += 0x81D0;
			return y >> 8;
		}
		return 7;
	case 0x60:
		return 0;
	case 0x80:
		return mbc7->eeprom;
	default:
		return 0xFF;
	}
}

void _GBMBC7Write(struct GBMemory* memory, uint16_t address, uint8_t value) {
	struct GBMBC7State* mbc7 = &memory->mbcState.mbc7;
	if (mbc7->access != 3) {
		return;
	}
	switch (address & 0xF0) {
	case 0x00:
		mbc7->latch = (value & 0x55) == 0x55;
		return;
	case 0x10:
		mbc7->latch |= (value & 0xAA);
		if (mbc7->latch == 0xAB && memory->rotation && memory->rotation->sample) {
			memory->rotation->sample(memory->rotation);
		}
		mbc7->latch = 0;
		return;
	default:
		mLOG(GB_MBC, STUB, "MBC7 unknown register: %04X:%02X", address, value);
		return;
	case 0x80:
		break;
	}
	GBMBC7Field old = memory->mbcState.mbc7.eeprom;
	value = GBMBC7FieldFillDO(value); // Hi-Z
	if (!GBMBC7FieldIsCS(old) && GBMBC7FieldIsCS(value)) {
		mbc7->state = GBMBC7_STATE_IDLE;
	}
	if (!GBMBC7FieldIsCLK(old) && GBMBC7FieldIsCLK(value)) {
		if (mbc7->state == GBMBC7_STATE_READ_COMMAND || mbc7->state == GBMBC7_STATE_EEPROM_WRITE || mbc7->state == GBMBC7_STATE_EEPROM_WRAL) {
			mbc7->sr <<= 1;
			mbc7->sr |= GBMBC7FieldGetDI(value);
			++mbc7->srBits;
		}
		switch (mbc7->state) {
		case GBMBC7_STATE_IDLE:
			if (GBMBC7FieldIsDI(value)) {
				mbc7->state = GBMBC7_STATE_READ_COMMAND;
				mbc7->srBits = 0;
				mbc7->sr = 0;
			}
			break;
		case GBMBC7_STATE_READ_COMMAND:
			if (mbc7->srBits == 10) {
				mbc7->state = 0x10 | (mbc7->sr >> 6);
				if (mbc7->state & 0xC) {
					mbc7->state &= ~0x3;
				}
				mbc7->srBits = 0;
				mbc7->address = mbc7->sr & 0x7F;
			}
			break;
		case GBMBC7_STATE_DO:
			value = GBMBC7FieldSetDO(value, mbc7->sr >> 15);
			mbc7->sr <<= 1;
			--mbc7->srBits;
			if (!mbc7->srBits) {
				mbc7->state = GBMBC7_STATE_IDLE;
			}
			break;
		default:
			break;
		}
		switch (mbc7->state) {
		case GBMBC7_STATE_EEPROM_EWEN:
			mbc7->writable = true;
			mbc7->state = GBMBC7_STATE_IDLE;
			break;
		case GBMBC7_STATE_EEPROM_EWDS:
			mbc7->writable = false;
			mbc7->state = GBMBC7_STATE_IDLE;
			break;
		case GBMBC7_STATE_EEPROM_WRITE:
			if (mbc7->srBits == 16) {
				if (mbc7->writable) {
					memory->sram[mbc7->address * 2] = mbc7->sr >> 8;
					memory->sram[mbc7->address * 2 + 1] = mbc7->sr;
				}
				mbc7->state = GBMBC7_STATE_IDLE;
			}
			break;
		case GBMBC7_STATE_EEPROM_ERASE:
			if (mbc7->writable) {
				memory->sram[mbc7->address * 2] = 0xFF;
				memory->sram[mbc7->address * 2 + 1] = 0xFF;
			}
			mbc7->state = GBMBC7_STATE_IDLE;
			break;
		case GBMBC7_STATE_EEPROM_READ:
			mbc7->srBits = 16;
			mbc7->sr = memory->sram[mbc7->address * 2] << 8;
			mbc7->sr |= memory->sram[mbc7->address * 2 + 1];
			mbc7->state = GBMBC7_STATE_DO;
			value = GBMBC7FieldClearDO(value);
			break;
		case GBMBC7_STATE_EEPROM_WRAL:
			if (mbc7->srBits == 16) {
				if (mbc7->writable) {
					int i;
					for (i = 0; i < 128; ++i) {
						memory->sram[i * 2] = mbc7->sr >> 8;
						memory->sram[i * 2 + 1] = mbc7->sr;
					}
				}
				mbc7->state = GBMBC7_STATE_IDLE;
			}
			break;
		case GBMBC7_STATE_EEPROM_ERAL:
			if (mbc7->writable) {
				int i;
				for (i = 0; i < 128; ++i) {
					memory->sram[i * 2] = 0xFF;
					memory->sram[i * 2 + 1] = 0xFF;
				}
			}
			mbc7->state = GBMBC7_STATE_IDLE;
			break;
		default:
			break;
		}
	} else if (GBMBC7FieldIsCS(value) && GBMBC7FieldIsCLK(old) && !GBMBC7FieldIsCLK(value)) {
		value = GBMBC7FieldSetDO(value, GBMBC7FieldGetDO(old));
	}
	mbc7->eeprom = value;
}
