/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/defines.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/vfs.h>

static const uint8_t _tama6RTCMask[32] = {
	//0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	0xF, 0x7, 0xF, 0x7, 0xF, 0x3, 0x7, 0xF, 0x3, 0xF, 0x1, 0xF, 0xF, 0x0, 0x0, 0x0,
	0x0, 0x0, 0xF, 0x7, 0xF, 0x3, 0x7, 0xF, 0x3, 0x0, 0x1, 0x3, 0x0, 0x0, 0x0, 0x0,
};

static const int _daysToMonth[] = {
	[ 1] = 0,
	[ 2] = 31,
	[ 3] = 31 + 28,
	[ 4] = 31 + 28 + 31,
	[ 5] = 31 + 28 + 31 + 30,
	[ 6] = 31 + 28 + 31 + 30 + 31,
	[ 7] = 31 + 28 + 31 + 30 + 31 + 30,
	[ 8] = 31 + 28 + 31 + 30 + 31 + 30 + 31,
	[ 9] = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
	[10] = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
	[11] = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
	[12] = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
};

static int _tama6DMYToDayOfYear(int day, int month, int year) {
	if (month < 1 || month > 12) {
		return -1;
	}
	day += _daysToMonth[month];
	if (month > 2 && (year & 3) == 0) {
		++day;
	}
	return day;
}

static int _tama6DayOfYearToMonth(int day, int year) {
	int month;
	for (month = 1; month < 12; ++month) {
		if (day <= _daysToMonth[month + 1]) {
			return month;
		}
		if (month == 2 && (year & 3) == 0) {
			if (day == 60) {
				return 2;
			}
			--day;
		}
	}
	return 12;
}

static int _tama6DayOfYearToDayOfMonth(int day, int year) {
	int month;
	for (month = 1; month < 12; ++month) {
		if (day <= _daysToMonth[month + 1]) {
			return day - _daysToMonth[month];
		}
		if (month == 2 && (year & 3) == 0) {
			if (day == 60) {
				return 29;
			}
			--day;
		}
	}
	return day - _daysToMonth[12];
}

static void _latchTAMA6Rtc(struct mRTCSource* rtc, struct GBTAMA5State* tama5, time_t* rtcLastLatch) {
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
	if (!t || tama5->disabled) {
		return;
	}

	uint8_t* timerRegs = tama5->rtcTimerPage;
	bool is24hour = tama5->rtcAlarmPage[GBTAMA6_RTC_PA1_24_HOUR];
	int64_t diff;
	diff = timerRegs[GBTAMA6_RTC_PA0_SECOND_1] + timerRegs[GBTAMA6_RTC_PA0_SECOND_10] * 10 + t % 60;
	if (diff < 0) {
		diff += 60;
		t -= 60;
	}
	timerRegs[GBTAMA6_RTC_PA0_SECOND_1] = diff % 10;
	timerRegs[GBTAMA6_RTC_PA0_SECOND_10] = (diff % 60) / 10;
	t /= 60;
	t += diff / 60;

	diff = timerRegs[GBTAMA6_RTC_PA0_MINUTE_1] + timerRegs[GBTAMA6_RTC_PA0_MINUTE_10] * 10 + t % 60;
	if (diff < 0) {
		diff += 60;
		t -= 60;
	}
	timerRegs[GBTAMA6_RTC_PA0_MINUTE_1] = diff % 10;
	timerRegs[GBTAMA6_RTC_PA0_MINUTE_10] = (diff % 60) / 10;
	t /= 60;
	t += diff / 60;

	diff = timerRegs[GBTAMA6_RTC_PA0_HOUR_1];
	if (is24hour) {
		diff += timerRegs[GBTAMA6_RTC_PA0_HOUR_10] * 10;
	} else {
		int hour10 = timerRegs[GBTAMA6_RTC_PA0_HOUR_10];
		diff += (hour10 & 1) * 10;
		diff += (hour10 & 2) * 12;
	}
	diff += t % 24;
	if (diff < 0) {
		diff += 24;
		t -= 24;
	}
	if (is24hour) {
		timerRegs[GBTAMA6_RTC_PA0_HOUR_1] = (diff % 24) % 10;
		timerRegs[GBTAMA6_RTC_PA0_HOUR_10] = (diff % 24) / 10;
	} else {
		timerRegs[GBTAMA6_RTC_PA0_HOUR_1] = (diff % 12) % 10;
		timerRegs[GBTAMA6_RTC_PA0_HOUR_10] = (diff % 12) / 10 + (diff / 12) * 2;		
	}
	t /= 24;
	t += diff / 24;

	int day = timerRegs[GBTAMA6_RTC_PA0_DAY_1] + timerRegs[GBTAMA6_RTC_PA0_DAY_10] * 10;
	int month = timerRegs[GBTAMA6_RTC_PA0_MONTH_1] + timerRegs[GBTAMA6_RTC_PA0_MONTH_10] * 10;
	int year = timerRegs[GBTAMA6_RTC_PA0_YEAR_1] + timerRegs[GBTAMA6_RTC_PA0_YEAR_10] * 10;
	int leapYear = tama5->rtcAlarmPage[GBTAMA6_RTC_PA1_LEAP_YEAR];
	int dayOfWeek = timerRegs[GBTAMA6_RTC_PA0_WEEK];
	int dayInYear = _tama6DMYToDayOfYear(day, month, leapYear);
	diff = dayInYear + t;
	while (diff <= 0) {
		// Previous year
		if (leapYear & 3) {
			diff += 365;
		} else {
			diff += 366;
		}
		--year;
		--leapYear;
	}
	while (diff > (leapYear & 3 ? 365 : 366)) {
		// Future year
		if (year % 4) {
			diff -= 365;
		} else {
			diff -= 366;
		}
		++year;
		++leapYear;
	}
	dayOfWeek = (dayOfWeek + diff) % 7;
	year %= 100;
	leapYear &= 3;

	day = _tama6DayOfYearToDayOfMonth(diff, leapYear);
	month = _tama6DayOfYearToMonth(diff, leapYear);

	timerRegs[GBTAMA6_RTC_PA0_WEEK] = dayOfWeek;
	tama5->rtcAlarmPage[GBTAMA6_RTC_PA1_LEAP_YEAR] = leapYear;

	timerRegs[GBTAMA6_RTC_PA0_DAY_1] = day % 10;
	timerRegs[GBTAMA6_RTC_PA0_DAY_10] = day / 10;

	timerRegs[GBTAMA6_RTC_PA0_MONTH_1] = month % 10;
	timerRegs[GBTAMA6_RTC_PA0_MONTH_10] = month / 10;

	timerRegs[GBTAMA6_RTC_PA0_YEAR_1] = year % 10;
	timerRegs[GBTAMA6_RTC_PA0_YEAR_10] = year / 10;
}

void _GBTAMA5(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	struct GBTAMA5State* tama5 = &memory->mbcState.tama5;
	switch (address >> 13) {
	case 0x5:
		if (address & 1) {
			tama5->reg = value;
		} else {
			value &= 0xF;
			if (tama5->reg < GBTAMA5_MAX) {
				mLOG(GB_MBC, DEBUG, "TAMA5 write: %02X:%X", tama5->reg, value);
				tama5->registers[tama5->reg] = value;
				uint8_t address = ((tama5->registers[GBTAMA5_ADDR_HI] << 4) & 0x10) | tama5->registers[GBTAMA5_ADDR_LO];
				uint8_t out = (tama5->registers[GBTAMA5_WRITE_HI] << 4) | tama5->registers[GBTAMA5_WRITE_LO];
				switch (tama5->reg) {
				case GBTAMA5_BANK_LO:
				case GBTAMA5_BANK_HI:
					GBMBCSwitchBank(gb, tama5->registers[GBTAMA5_BANK_LO] | (tama5->registers[GBTAMA5_BANK_HI] << 4));
					break;
				case GBTAMA5_WRITE_LO:
				case GBTAMA5_WRITE_HI:
				case GBTAMA5_ADDR_HI:
					break;
				case GBTAMA5_ADDR_LO:
					switch (tama5->registers[GBTAMA5_ADDR_HI] >> 1) {
					case 0x0: // RAM write
						memory->sram[address] = out;
						gb->sramDirty |= mSAVEDATA_DIRT_NEW;
						break;
					case 0x1: // RAM read
						break;
					case 0x2: // Other commands
						switch (address) {
						case GBTAMA6_DISABLE_TIMER:
							tama5->disabled = true;
							tama5->rtcTimerPage[GBTAMA6_RTC_PAGE] &= 0x7;
							tama5->rtcAlarmPage[GBTAMA6_RTC_PAGE] &= 0x7;
							tama5->rtcFreePage0[GBTAMA6_RTC_PAGE] &= 0x7;
							tama5->rtcFreePage1[GBTAMA6_RTC_PAGE] &= 0x7;
							break;
						case GBTAMA6_ENABLE_TIMER:
							tama5->disabled = false;
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_SECOND_1] = 0;
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_SECOND_10] = 0;
							tama5->rtcTimerPage[GBTAMA6_RTC_PAGE] |= 0x8;
							tama5->rtcAlarmPage[GBTAMA6_RTC_PAGE] |= 0x8;
							tama5->rtcFreePage0[GBTAMA6_RTC_PAGE] |= 0x8;
							tama5->rtcFreePage1[GBTAMA6_RTC_PAGE] |= 0x8;
							break;
						case GBTAMA6_MINUTE_WRITE:
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_MINUTE_1] = out & 0xF;
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_MINUTE_10] = out >> 4;
							break;
						case GBTAMA6_HOUR_WRITE:
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_HOUR_1] = out & 0xF;
							tama5->rtcTimerPage[GBTAMA6_RTC_PA0_HOUR_10] = out >> 4;
							break;
						case GBTAMA6_DISABLE_ALARM:
							tama5->rtcTimerPage[GBTAMA6_RTC_PAGE] &= 0xB;
							tama5->rtcAlarmPage[GBTAMA6_RTC_PAGE] &= 0xB;
							tama5->rtcFreePage0[GBTAMA6_RTC_PAGE] &= 0xB;
							tama5->rtcFreePage1[GBTAMA6_RTC_PAGE] &= 0xB;
							break;
						case GBTAMA6_ENABLE_ALARM:
							tama5->rtcTimerPage[GBTAMA6_RTC_PAGE] |= 0x4;
							tama5->rtcAlarmPage[GBTAMA6_RTC_PAGE] |= 0x4;
							tama5->rtcFreePage0[GBTAMA6_RTC_PAGE] |= 0x4;
							tama5->rtcFreePage1[GBTAMA6_RTC_PAGE] |= 0x4;
							break;
						}
						break;
					case 0x4: // RTC access
						address = tama5->registers[GBTAMA5_WRITE_LO];
						if (address >= GBTAMA6_RTC_PAGE) {
							break;
						}
						out = tama5->registers[GBTAMA5_WRITE_HI];
						switch (tama5->registers[GBTAMA5_ADDR_LO]) {
						case 0:
							out &= _tama6RTCMask[address];
							tama5->rtcTimerPage[address] = out;
							break;
						case 2:
							out &= _tama6RTCMask[address | 0x10];
							tama5->rtcAlarmPage[address] = out;
							break;
						case 4:
							tama5->rtcFreePage0[address] = out;
							break;
						case 6:
							tama5->rtcFreePage1[address] = out;
							break;
						}
						break;
					default:
						mLOG(GB_MBC, STUB, "TAMA5 unknown address: %02X:%02X", address, out);
						break;
					}
					break;
				default:
					mLOG(GB_MBC, STUB, "TAMA5 unknown write: %02X:%X", tama5->reg, value);
					break;
				}
			} else {
				mLOG(GB_MBC, STUB, "TAMA5 unknown write: %02X", tama5->reg);
			}
		}
		break;
	default:
		mLOG(GB_MBC, STUB, "TAMA5 unknown address: %04X:%02X", address, value);
	}
}

uint8_t _GBTAMA5Read(struct GBMemory* memory, uint16_t address) {
	struct GBTAMA5State* tama5 = &memory->mbcState.tama5;
	if ((address & 0x1FFF) > 1) {
		mLOG(GB_MBC, STUB, "TAMA5 unknown address: %04X", address);
	}
	if (address & 1) {
		return 0xFF;
	} else {
		uint8_t value = 0xF0;
		uint8_t address = ((tama5->registers[GBTAMA5_ADDR_HI] << 4) & 0x10) | tama5->registers[GBTAMA5_ADDR_LO];
		switch (tama5->reg) {
		case GBTAMA5_ACTIVE:
			return 0xF1;
		case GBTAMA5_READ_LO:
		case GBTAMA5_READ_HI:
			switch (tama5->registers[GBTAMA5_ADDR_HI] >> 1) {
			case 0x1:
				value = memory->sram[address];
				break;
			case 0x2:
				mLOG(GB_MBC, STUB, "TAMA5 unknown read %s: %02X", tama5->reg == GBTAMA5_READ_HI ? "hi" : "lo", address);
				_latchTAMA6Rtc(memory->rtc, tama5, &memory->rtcLastLatch);
				switch (address) {
				case GBTAMA6_MINUTE_READ:
					value = (tama5->rtcTimerPage[GBTAMA6_RTC_PA0_MINUTE_10] << 4) | tama5->rtcTimerPage[GBTAMA6_RTC_PA0_MINUTE_1];
					break;
				case GBTAMA6_HOUR_READ:
					value = (tama5->rtcTimerPage[GBTAMA6_RTC_PA0_HOUR_10] << 4) | tama5->rtcTimerPage[GBTAMA6_RTC_PA0_HOUR_1];
					break;
				default:
					value = address;
					break;
				}
				break;
			case 0x4:
				if (tama5->reg == GBTAMA5_READ_HI) {
					mLOG(GB_MBC, GAME_ERROR, "TAMA5 reading RTC incorrectly");
					break;
				}
				_latchTAMA6Rtc(memory->rtc, tama5, &memory->rtcLastLatch);
				address = tama5->registers[GBTAMA5_WRITE_LO];
				if (address > GBTAMA6_RTC_PAGE) {
					value = 0;
					break;
				}
				switch (tama5->registers[GBTAMA5_ADDR_LO]) {
				case 1:
					value = tama5->rtcTimerPage[address];
					break;
				case 3:
					value = tama5->rtcTimerPage[address];
					break;
				case 5:
					value = tama5->rtcTimerPage[address];
					break;
				case 7:
					value = tama5->rtcTimerPage[address];
					break;
				}
				break;
			default:
				mLOG(GB_MBC, STUB, "TAMA5 unknown read %s: %02X", tama5->reg == GBTAMA5_READ_HI ? "hi" : "lo", address);
				break;
			}
			if (tama5->reg == GBTAMA5_READ_HI) {
				value >>= 4;
			}
			value |= 0xF0;
			return value;
		default:
			mLOG(GB_MBC, STUB, "TAMA5 unknown read: %02X", tama5->reg);
			return 0xF1;
		}
	}
}


void GBMBCTAMA5Read(struct GB* gb) {
	struct GBMBCTAMA5SaveBuffer buffer;
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}
	vf->seek(vf, gb->sramSize, SEEK_SET);
	if (vf->read(vf, &buffer, sizeof(buffer)) < (ssize_t) sizeof(buffer)) {
		gb->memory.mbcState.tama5.disabled = false;
		return;
	}

	size_t i;
	for (i = 0; i < 0x8; ++i) {
		gb->memory.mbcState.tama5.rtcTimerPage[i * 2] = buffer.rtcTimerPage[i] & 0xF;
		gb->memory.mbcState.tama5.rtcTimerPage[i * 2 + 1] = buffer.rtcTimerPage[i] >> 4;
		gb->memory.mbcState.tama5.rtcAlarmPage[i * 2] = buffer.rtcAlarmPage[i] & 0xF;
		gb->memory.mbcState.tama5.rtcAlarmPage[i * 2 + 1] = buffer.rtcAlarmPage[i] >> 4;
		gb->memory.mbcState.tama5.rtcFreePage0[i * 2] = buffer.rtcFreePage0[i] & 0xF;
		gb->memory.mbcState.tama5.rtcFreePage0[i * 2 + 1] = buffer.rtcFreePage0[i] >> 4;
		gb->memory.mbcState.tama5.rtcFreePage1[i * 2] = buffer.rtcFreePage1[i] & 0xF;
		gb->memory.mbcState.tama5.rtcFreePage1[i * 2 + 1] = buffer.rtcFreePage1[i] >> 4;
	}
	LOAD_64LE(gb->memory.rtcLastLatch, 0, &buffer.latchedUnix);

	gb->memory.mbcState.tama5.disabled = !(gb->memory.mbcState.tama5.rtcTimerPage[GBTAMA6_RTC_PAGE] & 0x8);

	gb->memory.mbcState.tama5.rtcTimerPage[GBTAMA6_RTC_PAGE] &= 0xC;
	gb->memory.mbcState.tama5.rtcAlarmPage[GBTAMA6_RTC_PAGE] &= 0xC;
	gb->memory.mbcState.tama5.rtcAlarmPage[GBTAMA6_RTC_PAGE] |= 1;
	gb->memory.mbcState.tama5.rtcFreePage0[GBTAMA6_RTC_PAGE] &= 0xC;
	gb->memory.mbcState.tama5.rtcFreePage0[GBTAMA6_RTC_PAGE] |= 2;
	gb->memory.mbcState.tama5.rtcFreePage1[GBTAMA6_RTC_PAGE] &= 0xC;
	gb->memory.mbcState.tama5.rtcFreePage1[GBTAMA6_RTC_PAGE] |= 3;
}

void GBMBCTAMA5Write(struct GB* gb) {
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}

	struct GBMBCTAMA5SaveBuffer buffer = {0};
	size_t i;
	for (i = 0; i < 8; ++i) {
		buffer.rtcTimerPage[i] = gb->memory.mbcState.tama5.rtcTimerPage[i * 2] & 0xF;
		buffer.rtcTimerPage[i] |= gb->memory.mbcState.tama5.rtcTimerPage[i * 2 + 1] << 4;
		buffer.rtcAlarmPage[i] = gb->memory.mbcState.tama5.rtcAlarmPage[i * 2] & 0xF;
		buffer.rtcAlarmPage[i] |= gb->memory.mbcState.tama5.rtcAlarmPage[i * 2 + 1] << 4;
		buffer.rtcFreePage0[i] = gb->memory.mbcState.tama5.rtcFreePage0[i * 2] & 0xF;
		buffer.rtcFreePage0[i] |= gb->memory.mbcState.tama5.rtcFreePage0[i * 2 + 1] << 4;
		buffer.rtcFreePage1[i] = gb->memory.mbcState.tama5.rtcFreePage1[i * 2] & 0xF;
		buffer.rtcFreePage1[i] |= gb->memory.mbcState.tama5.rtcFreePage1[i * 2 + 1] << 4;
	}
	STORE_64LE(gb->memory.rtcLastLatch, 0, &buffer.latchedUnix);

	_GBMBCAppendSaveSuffix(gb, &buffer, sizeof(buffer));
}
