
/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/hardware.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/memory.h>

#define EREADER_BLOCK_SIZE 40

static void _eReaderReset(struct GBACartridgeHardware* hw);
static void _eReaderWriteControl0(struct GBACartridgeHardware* hw, uint8_t value);
static void _eReaderWriteControl1(struct GBACartridgeHardware* hw, uint8_t value);
static void _eReaderReadData(struct GBACartridgeHardware* hw);

const int EREADER_NYBBLE_5BIT[16][5] = {
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 1, 0 },
	{ 1, 0, 0, 1, 0 },
	{ 0, 0, 1, 0, 0 },
	{ 0, 0, 1, 0, 1 },
	{ 0, 0, 1, 1, 0 },
	{ 1, 0, 1, 1, 0 },
	{ 0, 1, 0, 0, 0 },
	{ 0, 1, 0, 0, 1 },
	{ 0, 1, 0, 1, 0 },
	{ 1, 0, 1, 0, 0 },
	{ 0, 1, 1, 0, 0 },
	{ 0, 1, 1, 0, 1 },
	{ 1, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0 }
};

const uint8_t EREADER_CALIBRATION_TEMPLATE[] = {
	0x43, 0x61, 0x72, 0x64, 0x2d, 0x45, 0x20, 0x52, 0x65, 0x61, 0x64, 0x65, 0x72, 0x20, 0x32, 0x30,
	0x30, 0x31, 0x00, 0x00, 0xcf, 0x72, 0x2f, 0x37, 0x3a, 0x3a, 0x3a, 0x38, 0x33, 0x30, 0x30, 0x37,
	0x3a, 0x39, 0x37, 0x35, 0x33, 0x2f, 0x2f, 0x34, 0x36, 0x36, 0x37, 0x36, 0x34, 0x31, 0x2d, 0x30,
	0x32, 0x34, 0x35, 0x35, 0x34, 0x30, 0x2a, 0x2d, 0x2d, 0x2f, 0x31, 0x32, 0x31, 0x2f, 0x29, 0x2a,
	0x2c, 0x2b, 0x2c, 0x2e, 0x2e, 0x2d, 0x18, 0x2d, 0x8f, 0x03, 0x00, 0x00, 0xc0, 0xfd, 0x77, 0x00,
	0x00, 0x00, 0x01
};

const uint16_t EREADER_ADDRESS_CODES[] = {
	1023,
	1174,
	2628,
	3373,
	4233,
	6112,
	6450,
	7771,
	8826,
	9491,
	11201,
	11432,
	12556,
	13925,
	14519,
	16350,
	16629,
	18332,
	18766,
	20007,
	21379,
	21738,
	23096,
	23889,
	24944,
	26137,
	26827,
	28578,
	29190,
	30063,
	31677,
	31956,
	33410,
	34283,
	35641,
	35920,
	37364,
	38557,
	38991,
	40742,
	41735,
	42094,
	43708,
	44501,
	45169,
	46872,
	47562,
	48803,
	49544,
	50913,
	51251,
	53082,
	54014,
	54679
};

void GBAHardwareInitEReader(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_EREADER;
	_eReaderReset(hw);

	if (hw->p->memory.savedata.data[0xD000] == 0xFF) {
		memset(&hw->p->memory.savedata.data[0xD000], 0, 0x1000);
		memcpy(&hw->p->memory.savedata.data[0xD000], EREADER_CALIBRATION_TEMPLATE, sizeof(EREADER_CALIBRATION_TEMPLATE));
	}
	if (hw->p->memory.savedata.data[0xE000] == 0xFF) {
		memset(&hw->p->memory.savedata.data[0xE000], 0, 0x1000);
		memcpy(&hw->p->memory.savedata.data[0xE000], EREADER_CALIBRATION_TEMPLATE, sizeof(EREADER_CALIBRATION_TEMPLATE));
	}
}

void GBAHardwareEReaderWrite(struct GBACartridgeHardware* hw, uint32_t address, uint16_t value) {
	address &= 0x700FF;
	switch (address >> 17) {
	case 0:
		hw->eReaderRegisterUnk = value & 0xF;
		break;
	case 1:
		hw->eReaderRegisterReset = (value & 0x8A) | 4;
		if (value & 2) {
			_eReaderReset(hw);
		}
		break;
	case 2:
		mLOG(GBA_HW, GAME_ERROR, "e-Reader write to read-only registers: %05X:%04X", address, value);
		break;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader write: %05X:%04X", address, value);
	}
}

void GBAHardwareEReaderWriteFlash(struct GBACartridgeHardware* hw, uint32_t address, uint8_t value) {
	address &= 0xFFFF;
	switch (address) {
	case 0xFFB0:
		_eReaderWriteControl0(hw, value);
		break;
	case 0xFFB1:
		_eReaderWriteControl1(hw, value);
		break;
	case 0xFFB2:
		hw->eReaderRegisterLed &= 0xFF00;
		hw->eReaderRegisterLed |= value;
		break;
	case 0xFFB3:
		hw->eReaderRegisterLed &= 0x00FF;
		hw->eReaderRegisterLed |= value << 8;
		break;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader write to flash: %04X:%02X", address, value);
	}
}

uint16_t GBAHardwareEReaderRead(struct GBACartridgeHardware* hw, uint32_t address) {
	address &= 0x700FF;
	uint16_t value;
	switch (address >> 17) {
	case 0:
		return hw->eReaderRegisterUnk;
	case 1:
		return hw->eReaderRegisterReset;
	case 2:
		if (address > 0x40088) {
			return 0;
		}
		LOAD_16(value, address & 0xFE, hw->eReaderData);
		return value;
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader read: %05X", address);
	return 0;
}

uint8_t GBAHardwareEReaderReadFlash(struct GBACartridgeHardware* hw, uint32_t address) {
	address &= 0xFFFF;
	switch (address) {
	case 0xFFB0:
		return hw->eReaderRegisterControl0;
	case 0xFFB1:
		return hw->eReaderRegisterControl1;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader read from flash: %04X", address);
		return 0;
	}
}

static void _eReaderAnchor(uint8_t* origin) {
	origin[EREADER_DOTCODE_STRIDE * 0 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 0 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 0 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 3] = 1;
}

static void _eReaderAlignment(uint8_t* origin) {
	origin[8] = 1;
	origin[10] = 1;
	origin[12] = 1;
	origin[14] = 1;
	origin[16] = 1;
	origin[18] = 1;
	origin[21] = 1;
	origin[23] = 1;
	origin[25] = 1;
	origin[27] = 1;
	origin[29] = 1;
	origin[31] = 1;
}

static void _eReaderAddress(uint8_t* origin, int a) {
	origin[EREADER_DOTCODE_STRIDE * 7 + 2] = 1;
	uint16_t addr = EREADER_ADDRESS_CODES[a];
	int i;
	for (i = 0; i < 16; ++i) {
		origin[EREADER_DOTCODE_STRIDE * (16 + i) + 2] = (addr >> (15 - i)) & 1;
	}
}

void GBAHardwareEReaderScan(struct GBACartridgeHardware* hw, const void* data, size_t size) {
	if (!hw->eReaderDots) {
		hw->eReaderDots = anonymousMemoryMap(EREADER_DOTCODE_SIZE);
	}
	memset(hw->eReaderDots, 0, EREADER_DOTCODE_SIZE);

	int base;
	switch (size) {
	case 2912:
		base = 25;
		break;
	case 1872:
		base = 1;
		break;
	default:
		return;
	}

	size_t i;
	for (i = 0; i < (size / 104) + 1; ++i) {
		uint8_t* origin = &hw->eReaderDots[35 * i + 200];
		_eReaderAnchor(&origin[EREADER_DOTCODE_STRIDE * 0]);
		_eReaderAnchor(&origin[EREADER_DOTCODE_STRIDE * 35]);
		_eReaderAddress(origin, base + i);
	}
	for (i = 0; i < size / 104; ++i) {
		uint8_t block[1040];
		uint8_t* origin = &hw->eReaderDots[35 * i + 200];
		_eReaderAlignment(&origin[EREADER_DOTCODE_STRIDE * 2]);
		_eReaderAlignment(&origin[EREADER_DOTCODE_STRIDE * 37]);

		int b;
		for (b = 0; b < 104; ++b) {
			const int* nybble5;
			nybble5 = EREADER_NYBBLE_5BIT[((const uint8_t*) data)[i * 104 + b] >> 4];
			block[b * 10 + 0] = nybble5[0];
			block[b * 10 + 1] = nybble5[1];
			block[b * 10 + 2] = nybble5[2];
			block[b * 10 + 3] = nybble5[3];
			block[b * 10 + 4] = nybble5[4];
			nybble5 = EREADER_NYBBLE_5BIT[((const uint8_t*) data)[i * 104 + b] & 0xF];
			block[b * 10 + 5] = nybble5[0];
			block[b * 10 + 6] = nybble5[1];
			block[b * 10 + 7] = nybble5[2];
			block[b * 10 + 8] = nybble5[3];
			block[b * 10 + 9] = nybble5[4];
		}

		b = 0;
		int y;
		for (y = 0; y < 3; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (4 + y) + 7], &block[b], 26);
			b += 26;
		}
		for (y = 0; y < 26; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (7 + y) + 3], &block[b], 34);
			b += 34;
		}
		for (y = 0; y < 3; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (33 + y) + 7], &block[b], 26);
			b += 26;
		}
	}
	hw->eReaderX = -24;
}

void _eReaderReset(struct GBACartridgeHardware* hw) {
	memset(hw->eReaderData, 0, sizeof(hw->eReaderData));
	hw->eReaderRegisterUnk = 0;
	hw->eReaderRegisterReset = 4;
	hw->eReaderRegisterControl0 = 0;
	hw->eReaderRegisterControl1 = 0x80;
	hw->eReaderRegisterLed = 0;
	hw->eReaderState = 0;
	hw->eReaderActiveRegister = 0;
}

void _eReaderWriteControl0(struct GBACartridgeHardware* hw, uint8_t value) {
	EReaderControl0 control = value & 0x7F;
	EReaderControl0 oldControl = hw->eReaderRegisterControl0;
	if (hw->eReaderState == EREADER_SERIAL_INACTIVE) {
		if (EReaderControl0IsClock(oldControl) && EReaderControl0IsData(oldControl) && !EReaderControl0IsData(control)) {
			hw->eReaderState = EREADER_SERIAL_STARTING;
		}
	} else if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsData(oldControl) && EReaderControl0IsData(control)) {
		hw->eReaderState = EREADER_SERIAL_INACTIVE;

	} else if (hw->eReaderState == EREADER_SERIAL_STARTING) {
		if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsData(oldControl) && !EReaderControl0IsClock(control)) {
			hw->eReaderState = EREADER_SERIAL_BIT_0;
			hw->eReaderCommand = EREADER_COMMAND_IDLE;
		}
	} else if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsClock(control)) {
		mLOG(GBA_HW, DEBUG, "[e-Reader] Serial falling edge: %c %i", EReaderControl0IsDirection(control) ? '>' : '<', EReaderControl0GetData(control));
		// TODO: Improve direction control
		if (EReaderControl0IsDirection(control)) {
			hw->eReaderByte |= EReaderControl0GetData(control) << (7 - (hw->eReaderState - EREADER_SERIAL_BIT_0));
			++hw->eReaderState;
			if (hw->eReaderState == EREADER_SERIAL_END_BIT) {
				mLOG(GBA_HW, DEBUG, "[e-Reader] Wrote serial byte: %02x", hw->eReaderByte);
				switch (hw->eReaderCommand) {
				case EREADER_COMMAND_IDLE:
					hw->eReaderCommand = hw->eReaderByte;
					break;
				case EREADER_COMMAND_SET_INDEX:
					hw->eReaderActiveRegister = hw->eReaderByte;
					hw->eReaderCommand = EREADER_COMMAND_WRITE_DATA;
					break;
				case EREADER_COMMAND_WRITE_DATA:
					switch (hw->eReaderActiveRegister & 0x7F) {
					case 0:
					case 0x57:
					case 0x58:
					case 0x59:
					case 0x5A:
						// Read-only
						mLOG(GBA_HW, GAME_ERROR, "Writing to read-only e-Reader serial register: %02X", hw->eReaderActiveRegister);
						break;
					default:
						if ((hw->eReaderActiveRegister & 0x7F) > 0x5A) {
							mLOG(GBA_HW, GAME_ERROR, "Writing to non-existent e-Reader serial register: %02X", hw->eReaderActiveRegister);
							break;
						}
						hw->eReaderSerial[hw->eReaderActiveRegister & 0x7F] = hw->eReaderByte;
						break;
					}
					++hw->eReaderActiveRegister;
					break;
				default:
					mLOG(GBA_HW, ERROR, "Hit undefined state %02X in e-Reader state machine", hw->eReaderCommand);
					break;
				}
				hw->eReaderState = EREADER_SERIAL_BIT_0;
				hw->eReaderByte = 0;
			}
		} else if (hw->eReaderCommand == EREADER_COMMAND_READ_DATA) {
			int bit = hw->eReaderSerial[hw->eReaderActiveRegister & 0x7F] >> (7 - (hw->eReaderState - EREADER_SERIAL_BIT_0));
			control = EReaderControl0SetData(control, bit);
			++hw->eReaderState;
			if (hw->eReaderState == EREADER_SERIAL_END_BIT) {
				++hw->eReaderActiveRegister;
				mLOG(GBA_HW, DEBUG, "[e-Reader] Read serial byte: %02x", hw->eReaderSerial[hw->eReaderActiveRegister & 0x7F]);
			}
		}
	} else if (!EReaderControl0IsDirection(control)) {
		// Clear the error bit
		control = EReaderControl0ClearData(control);
	}
	hw->eReaderRegisterControl0 = control;
	if (!EReaderControl0IsScan(oldControl) && EReaderControl0IsScan(control)) {
		hw->eReaderX = 0;
		hw->eReaderY = 0;
	} else if (EReaderControl0IsLedEnable(control) && EReaderControl0IsScan(control) && !EReaderControl1IsScanline(hw->eReaderRegisterControl1)) {
		_eReaderReadData(hw);
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader Control0 write: %02X", value);
}

void _eReaderWriteControl1(struct GBACartridgeHardware* hw, uint8_t value) {
	EReaderControl1 control = (value & 0x32) | 0x80;
	hw->eReaderRegisterControl1 = control;
	if (EReaderControl0IsScan(hw->eReaderRegisterControl0) && !EReaderControl1IsScanline(control)) {
		++hw->eReaderY;
		if (hw->eReaderY == (hw->eReaderSerial[0x15] | (hw->eReaderSerial[0x14] << 8))) {
			hw->eReaderY = 0;
			if (hw->eReaderX < 3400) {
				hw->eReaderX += 220;
			}
		}
		_eReaderReadData(hw);
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader Control1 write: %02X", value);
}

void _eReaderReadData(struct GBACartridgeHardware* hw) {
	memset(hw->eReaderData, 0, EREADER_BLOCK_SIZE);
	if (hw->eReaderDots) {
		int y = hw->eReaderY - 10;
		if (y < 0 || y >= 120) {
			memset(hw->eReaderData, 0, EREADER_BLOCK_SIZE);
		} else {
			int i;
			uint8_t* origin = &hw->eReaderDots[EREADER_DOTCODE_STRIDE * (y / 3) + 16];
			for (i = 0; i < 20; ++i) {
				uint16_t word = 0;
				int x = hw->eReaderX + i * 16;
				word |= origin[(x +  0) / 3] << 8;
				word |= origin[(x +  1) / 3] << 9;
				word |= origin[(x +  2) / 3] << 10;
				word |= origin[(x +  3) / 3] << 11;
				word |= origin[(x +  4) / 3] << 12;
				word |= origin[(x +  5) / 3] << 13;
				word |= origin[(x +  6) / 3] << 14;
				word |= origin[(x +  7) / 3] << 15;
				word |= origin[(x +  8) / 3];
				word |= origin[(x +  9) / 3] << 1;
				word |= origin[(x + 10) / 3] << 2;
				word |= origin[(x + 11) / 3] << 3;
				word |= origin[(x + 12) / 3] << 4;
				word |= origin[(x + 13) / 3] << 5;
				word |= origin[(x + 14) / 3] << 6;
				word |= origin[(x + 15) / 3] << 7;
				STORE_16(word, (19 - i) << 1, hw->eReaderData);
			}
		}
	}
	hw->eReaderRegisterControl1 = EReaderControl1FillScanline(hw->eReaderRegisterControl1);
	if (EReaderControl0IsLedEnable(hw->eReaderRegisterControl0)) {
		uint16_t led = 2754; // TODO: Figure out why this breaks if using the LED register
		GBARaiseIRQ(hw->p, IRQ_GAMEPAK, -led);
	}
}
