/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/bios.h>

#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/memory.h>

const uint32_t GBA_BIOS_CHECKSUM = 0xBAAE187F;
const uint32_t GBA_DS_BIOS_CHECKSUM = 0xBAAE1880;

mLOG_DEFINE_CATEGORY(GBA_BIOS, "GBA BIOS", "gba.bios");

static void _unLz77(struct GBA* gba, int width);
static void _unHuffman(struct GBA* gba);
static void _unRl(struct GBA* gba, int width);
static void _unFilter(struct GBA* gba, int inwidth, int outwidth);
static void _unBitPack(struct GBA* gba);

static void _SoftReset(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->spsr.packed = 0;
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->spsr.packed = 0;
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;
	int8_t flag = ((int8_t*) gba->memory.iwram)[0x7FFA];
	memset(((int8_t*) gba->memory.iwram) + SIZE_WORKING_IRAM - 0x200, 0, 0x200);
	if (flag) {
		cpu->gprs[ARM_PC] = BASE_WORKING_RAM;
	} else {
		cpu->gprs[ARM_PC] = BASE_CART0;
	}
	_ARMSetMode(cpu, MODE_ARM);
	int currentCycles = 0;
	ARM_WRITE_PC;
}

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu->gprs[0];
	struct ARMCore* cpu = gba->cpu;
	cpu->memory.store16(cpu, BASE_IO | REG_DISPCNT, 0x0080, 0);
	if (registers & 0x01) {
		memset(gba->memory.wram, 0, SIZE_WORKING_RAM);
	}
	if (registers & 0x02) {
		memset(gba->memory.iwram, 0, SIZE_WORKING_IRAM - 0x200);
	}
	if (registers & 0x04) {
		memset(gba->video.palette, 0, SIZE_PALETTE_RAM);
	}
	if (registers & 0x08) {
		memset(gba->video.vram, 0, SIZE_VRAM);
	}
	if (registers & 0x10) {
		memset(gba->video.oam.raw, 0, SIZE_OAM);
	}
	if (registers & 0x20) {
		cpu->memory.store16(cpu, BASE_IO | REG_SIOCNT, 0x0000, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_RCNT, RCNT_INITIAL, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SIOMLT_SEND, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_JOYCNT, 0, 0);
		cpu->memory.store32(cpu, BASE_IO | REG_JOY_RECV_LO, 0, 0);
		cpu->memory.store32(cpu, BASE_IO | REG_JOY_TRANS_LO, 0, 0);
	}
	if (registers & 0x40) {
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND1CNT_LO, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND1CNT_HI, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND1CNT_X, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND2CNT_LO, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND2CNT_HI, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND3CNT_LO, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND3CNT_HI, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND3CNT_X, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND4CNT_LO, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUND4CNT_HI, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUNDCNT_LO, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUNDCNT_HI, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUNDCNT_X, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_SOUNDBIAS, 0x200, 0);
		memset(gba->audio.psg.ch3.wavedata32, 0, sizeof(gba->audio.psg.ch3.wavedata32));
	}
	if (registers & 0x80) {
		cpu->memory.store16(cpu, BASE_IO | 0x04, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x06, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x08, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x0A, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x0C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x0E, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x10, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x12, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x14, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x16, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x18, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x1A, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x1C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x1E, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG2PA, 0x100, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG2PB, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG2PC, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG2PD, 0x100, 0);
		cpu->memory.store32(cpu, BASE_IO | 0x28, 0, 0);
		cpu->memory.store32(cpu, BASE_IO | 0x2C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG3PA, 0x100, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG3PB, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG3PC, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | REG_BG3PD, 0x100, 0);
		cpu->memory.store32(cpu, BASE_IO | 0x38, 0, 0);
		cpu->memory.store32(cpu, BASE_IO | 0x3C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x40, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x42, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x44, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x46, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x48, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x4A, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x4C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x50, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x52, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x54, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xB0, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xB2, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xB4, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xB6, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xB8, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xBA, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xBC, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xBE, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xC0, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xC2, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xC4, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xC6, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xC8, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xCA, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xCC, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xCE, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xD0, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xD2, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xD4, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xD6, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xD8, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xDA, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xDC, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0xDE, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x100, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x102, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x104, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x106, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x108, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x10A, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x10C, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x10E, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x200, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x202, 0xFFFF, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x204, 0, 0);
		cpu->memory.store16(cpu, BASE_IO | 0x208, 0, 0);
	}
}

static void _BgAffineSet(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	int i = cpu->gprs[2];
	float ox, oy;
	float cx, cy;
	float sx, sy;
	float theta;
	int offset = cpu->gprs[0];
	int destination = cpu->gprs[1];
	float a, b, c, d;
	float rx, ry;
	while (i--) {
		// [ sx   0  0 ]   [ cos(theta)  -sin(theta)  0 ]   [ 1  0  cx - ox ]   [ A B rx ]
		// [  0  sy  0 ] * [ sin(theta)   cos(theta)  0 ] * [ 0  1  cy - oy ] = [ C D ry ]
		// [  0   0  1 ]   [     0            0       1 ]   [ 0  0     1    ]   [ 0 0  1 ]
		ox = (int32_t) cpu->memory.load32(cpu, offset, 0) / 256.f;
		oy = (int32_t) cpu->memory.load32(cpu, offset + 4, 0) / 256.f;
		cx = (int16_t) cpu->memory.load16(cpu, offset + 8, 0);
		cy = (int16_t) cpu->memory.load16(cpu, offset + 10, 0);
		sx = (int16_t) cpu->memory.load16(cpu, offset + 12, 0) / 256.f;
		sy = (int16_t) cpu->memory.load16(cpu, offset + 14, 0) / 256.f;
		theta = (cpu->memory.load16(cpu, offset + 16, 0) >> 8) / 128.f * M_PI;
		offset += 20;
		// Rotation
		a = d = cosf(theta);
		b = c = sinf(theta);
		// Scale
		a *= sx;
		b *= -sx;
		c *= sy;
		d *= sy;
		// Translate
		rx = ox - (a * cx + b * cy);
		ry = oy - (c * cx + d * cy);
		cpu->memory.store16(cpu, destination, a * 256, 0);
		cpu->memory.store16(cpu, destination + 2, b * 256, 0);
		cpu->memory.store16(cpu, destination + 4, c * 256, 0);
		cpu->memory.store16(cpu, destination + 6, d * 256, 0);
		cpu->memory.store32(cpu, destination + 8, rx * 256, 0);
		cpu->memory.store32(cpu, destination + 12, ry * 256, 0);
		destination += 16;
	}
}

static void _ObjAffineSet(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	int i = cpu->gprs[2];
	float sx, sy;
	float theta;
	int offset = cpu->gprs[0];
	int destination = cpu->gprs[1];
	int diff = cpu->gprs[3];
	float a, b, c, d;
	while (i--) {
		// [ sx   0 ]   [ cos(theta)  -sin(theta) ]   [ A B ]
		// [  0  sy ] * [ sin(theta)   cos(theta) ] = [ C D ]
		sx = (int16_t) cpu->memory.load16(cpu, offset, 0) / 256.f;
		sy = (int16_t) cpu->memory.load16(cpu, offset + 2, 0) / 256.f;
		theta = (cpu->memory.load16(cpu, offset + 4, 0) >> 8) / 128.f * M_PI;
		offset += 8;
		// Rotation
		a = d = cosf(theta);
		b = c = sinf(theta);
		// Scale
		a *= sx;
		b *= -sx;
		c *= sy;
		d *= sy;
		cpu->memory.store16(cpu, destination, a * 256, 0);
		cpu->memory.store16(cpu, destination + diff, b * 256, 0);
		cpu->memory.store16(cpu, destination + diff * 2, c * 256, 0);
		cpu->memory.store16(cpu, destination + diff * 3, d * 256, 0);
		destination += diff * 4;
	}
}

static void _MidiKey2Freq(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;

	int oldRegion = gba->memory.activeRegion;
	gba->memory.activeRegion = REGION_BIOS;
	uint32_t key = cpu->memory.load32(cpu, cpu->gprs[0] + 4, 0);
	gba->memory.activeRegion = oldRegion;

	cpu->gprs[0] = key / powf(2, (180.f - cpu->gprs[1] - cpu->gprs[2] / 256.f) / 12.f);
}

static void _Div(struct GBA* gba, int32_t num, int32_t denom) {
	struct ARMCore* cpu = gba->cpu;
	if (denom != 0 && (denom != -1 || num != INT32_MIN)) {
		div_t result = div(num, denom);
		cpu->gprs[0] = result.quot;
		cpu->gprs[1] = result.rem;
		cpu->gprs[3] = abs(result.quot);
	} else if (denom == 0) {
		mLOG(GBA_BIOS, GAME_ERROR, "Attempting to divide %i by zero!", num);
		// If abs(num) > 1, this should hang, but that would be painful to
		// emulate in HLE, and no game will get into a state where it hangs...
		cpu->gprs[0] = (num < 0) ? -1 : 1;
		cpu->gprs[1] = num;
		cpu->gprs[3] = 1;
	} else {
		mLOG(GBA_BIOS, GAME_ERROR, "Attempting to divide INT_MIN by -1!");
		cpu->gprs[0] = INT32_MIN;
		cpu->gprs[1] = 0;
		cpu->gprs[3] = INT32_MIN;
	}
}

static int16_t _ArcTan(int32_t i, int32_t* r1, int32_t* r3) {
	int32_t a = -((i * i) >> 14);
	int32_t b = ((0xA9 * a) >> 14) + 0x390;
	b = ((b * a) >> 14) + 0x91C;
	b = ((b * a) >> 14) + 0xFB6;
	b = ((b * a) >> 14) + 0x16AA;
	b = ((b * a) >> 14) + 0x2081;
	b = ((b * a) >> 14) + 0x3651;
	b = ((b * a) >> 14) + 0xA2F9;
	if (r1) {
		*r1 = a;
	}
	if (r3) {
		*r3 = b;
	}
	return (i * b) >> 16;
}

static int16_t _ArcTan2(int32_t x, int32_t y, int32_t* r1) {
	if (!y) {
		if (x >= 0) {
			return 0;
		}
		return 0x8000;
	}
	if (!x) {
		if (y >= 0) {
			return 0x4000;
		}
		return 0xC000;
	}
	if (y >= 0) {
		if (x >= 0) {
			if (x >= y) {
				return _ArcTan((y << 14) / x, r1, NULL);
			}
		} else if (-x >= y) {
			return _ArcTan((y << 14) / x, r1, NULL) + 0x8000;
		}
		return 0x4000 - _ArcTan((x << 14) / y, r1, NULL);
	} else {
		if (x <= 0) {
			if (-x > -y) {
				return _ArcTan((y << 14) / x, r1, NULL) + 0x8000;
			}
		} else if (x >= -y) {
			return _ArcTan((y << 14) / x, r1, NULL) + 0x10000;
		}
		return 0xC000 - _ArcTan((x << 14) / y, r1, NULL);
	}
}

void GBASwi16(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
	mLOG(GBA_BIOS, DEBUG, "SWI: %02X r0: %08X r1: %08X r2: %08X r3: %08X",
	    immediate, cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3]);

	switch (immediate) {
	case 0xFA:
		GBAPrintFlush(gba);
		return;
	}

	if (gba->memory.fullBios) {
		ARMRaiseSWI(cpu);
		return;
	}
	switch (immediate) {
	case 0x0:
		_SoftReset(gba);
		break;
	case 0x1:
		_RegisterRamReset(gba);
		break;
	case 0x2:
		GBAHalt(gba);
		break;
	case 0x3:
		GBAStop(gba);
		break;
	case 0x05:
	// VBlankIntrWait
	// Fall through:
	case 0x04:
		// IntrWait
		ARMRaiseSWI(cpu);
		break;
	case 0x6:
		_Div(gba, cpu->gprs[0], cpu->gprs[1]);
		break;
	case 0x7:
		_Div(gba, cpu->gprs[1], cpu->gprs[0]);
		break;
	case 0x8:
		cpu->gprs[0] = sqrt((uint32_t) cpu->gprs[0]);
		break;
	case 0x9:
		cpu->gprs[0] = _ArcTan(cpu->gprs[0], &cpu->gprs[1], &cpu->gprs[3]);
		break;
	case 0xA:
		cpu->gprs[0] = (uint16_t) _ArcTan2(cpu->gprs[0], cpu->gprs[1], &cpu->gprs[1]);
		cpu->gprs[3] = 0x170;
		break;
	case 0xB:
	case 0xC:
		if (cpu->gprs[0] >> BASE_OFFSET < REGION_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Cannot CpuSet from BIOS");
			break;
		}
		if (cpu->gprs[0] & (cpu->gprs[2] & (1 << 26) ? 3 : 1)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Misaligned CpuSet source");
		}
		if (cpu->gprs[1] & (cpu->gprs[2] & (1 << 26) ? 3 : 1)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Misaligned CpuSet destination");
		}
		ARMRaiseSWI(cpu);
		break;
	case 0xD:
		cpu->gprs[0] = GBA_BIOS_CHECKSUM;
		cpu->gprs[1] = 1;
		cpu->gprs[3] = SIZE_BIOS;
		break;
	case 0xE:
		_BgAffineSet(gba);
		break;
	case 0xF:
		_ObjAffineSet(gba);
		break;
	case 0x10:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack destination");
		// Fall through
		case REGION_WORKING_RAM:
		case REGION_WORKING_IRAM:
		case REGION_VRAM:
			_unBitPack(gba);
			break;
		}
		break;
	case 0x11:
	case 0x12:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad LZ77 source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad LZ77 destination");
		// Fall through
		case REGION_WORKING_RAM:
		case REGION_WORKING_IRAM:
		case REGION_VRAM:
			_unLz77(gba, immediate == 0x11 ? 1 : 2);
			break;
		}
		break;
	case 0x13:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad Huffman source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad Huffman destination");
		// Fall through
		case REGION_WORKING_RAM:
		case REGION_WORKING_IRAM:
		case REGION_VRAM:
			_unHuffman(gba);
			break;
		}
		break;
	case 0x14:
	case 0x15:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad RL source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad RL destination");
		// Fall through
		case REGION_WORKING_RAM:
		case REGION_WORKING_IRAM:
		case REGION_VRAM:
			_unRl(gba, immediate == 0x14 ? 1 : 2);
			break;
		}
		break;
	case 0x16:
	case 0x17:
	case 0x18:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad UnFilter source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad UnFilter destination");
		// Fall through
		case REGION_WORKING_RAM:
		case REGION_WORKING_IRAM:
		case REGION_VRAM:
			_unFilter(gba, immediate == 0x18 ? 2 : 1, immediate == 0x16 ? 1 : 2);
			break;
		}
		break;
	case 0x19:
		// SoundBias is mostly meaningless here
		mLOG(GBA_BIOS, STUB, "Stub software interrupt: SoundBias (19)");
		break;
	case 0x1F:
		_MidiKey2Freq(gba);
		break;
	default:
		mLOG(GBA_BIOS, STUB, "Stub software interrupt: %02X", immediate);
	}
	gba->memory.biosPrefetch = 0xE3A02004;
}

void GBASwi32(struct ARMCore* cpu, int immediate) {
	GBASwi16(cpu, immediate >> 16);
}

uint32_t GBAChecksum(uint32_t* memory, size_t size) {
	size_t i;
	uint32_t sum = 0;
	for (i = 0; i < size; i += 4) {
		sum += memory[i >> 2];
	}
	return sum;
}

static void _unLz77(struct GBA* gba, int width) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0];
	uint32_t dest = cpu->gprs[1];
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
	// We assume the signature byte (0x10) is correct
	int blockheader = 0; // Some compilers warn if this isn't set, even though it's trivially provably always set
	source += 4;
	int blocksRemaining = 0;
	uint32_t disp;
	int bytes;
	int byte;
	int halfword = 0;
	while (remaining > 0) {
		if (blocksRemaining) {
			if (blockheader & 0x80) {
				// Compressed
				int block = cpu->memory.load8(cpu, source + 1, 0) | (cpu->memory.load8(cpu, source, 0) << 8);
				source += 2;
				disp = dest - (block & 0x0FFF) - 1;
				bytes = (block >> 12) + 3;
				while (bytes--) {
					if (remaining) {
						--remaining;
					}
					if (width == 2) {
						byte = (int16_t) cpu->memory.load16(cpu, disp & ~1, 0);
						if (dest & 1) {
							byte >>= (disp & 1) * 8;
							halfword |= byte << 8;
							cpu->memory.store16(cpu, dest ^ 1, halfword, 0);
						} else {
							byte >>= (disp & 1) * 8;
							halfword = byte & 0xFF;
						}
					} else {
						byte = cpu->memory.load8(cpu, disp, 0);
						cpu->memory.store8(cpu, dest, byte, 0);
					}
					++disp;
					++dest;
				}
			} else {
				// Uncompressed
				byte = cpu->memory.load8(cpu, source, 0);
				++source;
				if (width == 2) {
					if (dest & 1) {
						halfword |= byte << 8;
						cpu->memory.store16(cpu, dest ^ 1, halfword, 0);
					} else {
						halfword = byte;
					}
				} else {
					cpu->memory.store8(cpu, dest, byte, 0);
				}
				++dest;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = cpu->memory.load8(cpu, source, 0);
			++source;
			blocksRemaining = 8;
		}
	}
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
	cpu->gprs[3] = 0;
}

DECL_BITFIELD(HuffmanNode, uint8_t);
DECL_BITS(HuffmanNode, Offset, 0, 6);
DECL_BIT(HuffmanNode, RTerm, 6);
DECL_BIT(HuffmanNode, LTerm, 7);

static void _unHuffman(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0] & 0xFFFFFFFC;
	uint32_t dest = cpu->gprs[1];
	uint32_t header = cpu->memory.load32(cpu, source, 0);
	int remaining = header >> 8;
	unsigned bits = header & 0xF;
	if (bits == 0) {
		mLOG(GBA_BIOS, GAME_ERROR, "Invalid Huffman bits");
		bits = 8;
	}
	if (32 % bits || bits == 1) {
		mLOG(GBA_BIOS, STUB, "Unimplemented unaligned Huffman");
		return;
	}
	// We assume the signature byte (0x20) is correct
	int treesize = (cpu->memory.load8(cpu, source + 4, 0) << 1) + 1;
	int block = 0;
	uint32_t treeBase = source + 5;
	source += 5 + treesize;
	uint32_t nPointer = treeBase;
	HuffmanNode node;
	int bitsRemaining;
	int readBits;
	int bitsSeen = 0;
	node = cpu->memory.load8(cpu, nPointer, 0);
	while (remaining > 0) {
		uint32_t bitstream = cpu->memory.load32(cpu, source, 0);
		source += 4;
		for (bitsRemaining = 32; bitsRemaining > 0 && remaining > 0; --bitsRemaining, bitstream <<= 1) {
			uint32_t next = (nPointer & ~1) + HuffmanNodeGetOffset(node) * 2 + 2;
			if (bitstream & 0x80000000) {
				// Go right
				if (HuffmanNodeIsRTerm(node)) {
					readBits = cpu->memory.load8(cpu, next + 1, 0);
				} else {
					nPointer = next + 1;
					node = cpu->memory.load8(cpu, nPointer, 0);
					continue;
				}
			} else {
				// Go left
				if (HuffmanNodeIsLTerm(node)) {
					readBits = cpu->memory.load8(cpu, next, 0);
				} else {
					nPointer = next;
					node = cpu->memory.load8(cpu, nPointer, 0);
					continue;
				}
			}

			block |= (readBits & ((1 << bits) - 1)) << bitsSeen;
			bitsSeen += bits;
			nPointer = treeBase;
			node = cpu->memory.load8(cpu, nPointer, 0);
			if (bitsSeen == 32) {
				bitsSeen = 0;
				cpu->memory.store32(cpu, dest, block, 0);
				dest += 4;
				remaining -= 4;
				block = 0;
			}
		}
	}
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
}

static void _unRl(struct GBA* gba, int width) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0];
	int remaining = (cpu->memory.load32(cpu, source & 0xFFFFFFFC, 0) & 0xFFFFFF00) >> 8;
	int padding = (4 - remaining) & 0x3;
	// We assume the signature byte (0x30) is correct
	int blockheader;
	int block;
	source += 4;
	uint32_t dest = cpu->gprs[1];
	int halfword = 0;
	while (remaining > 0) {
		blockheader = cpu->memory.load8(cpu, source, 0);
		++source;
		if (blockheader & 0x80) {
			// Compressed
			blockheader &= 0x7F;
			blockheader += 3;
			block = cpu->memory.load8(cpu, source, 0);
			++source;
			while (blockheader-- && remaining) {
				--remaining;
				if (width == 2) {
					if (dest & 1) {
						halfword |= block << 8;
						cpu->memory.store16(cpu, dest ^ 1, halfword, 0);
					} else {
						halfword = block;
					}
				} else {
					cpu->memory.store8(cpu, dest, block, 0);
				}
				++dest;
			}
		} else {
			// Uncompressed
			blockheader++;
			while (blockheader-- && remaining) {
				--remaining;
				int byte = cpu->memory.load8(cpu, source, 0);
				++source;
				if (width == 2) {
					if (dest & 1) {
						halfword |= byte << 8;
						cpu->memory.store16(cpu, dest ^ 1, halfword, 0);
					} else {
						halfword = byte;
					}
				} else {
					cpu->memory.store8(cpu, dest, byte, 0);
				}
				++dest;
			}
		}
	}
	if (width == 2) {
		if (dest & 1) {
			--padding;
			++dest;
		}
		for (; padding > 0; padding -= 2, dest += 2) {
			cpu->memory.store16(cpu, dest, 0, 0);
		}
	} else {
		while (padding--) {
			cpu->memory.store8(cpu, dest, 0, 0);
			++dest;
		}
	}
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
}

static void _unFilter(struct GBA* gba, int inwidth, int outwidth) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0] & 0xFFFFFFFC;
	uint32_t dest = cpu->gprs[1];
	uint32_t header = cpu->memory.load32(cpu, source, 0);
	int remaining = header >> 8;
	// We assume the signature nybble (0x8) is correct
	uint16_t halfword = 0;
	uint16_t old = 0;
	source += 4;
	while (remaining > 0) {
		uint16_t new;
		if (inwidth == 1) {
			new = cpu->memory.load8(cpu, source, 0);
		} else {
			new = cpu->memory.load16(cpu, source, 0);
		}
		new += old;
		if (outwidth > inwidth) {
			halfword >>= 8;
			halfword |= (new << 8);
			if (source & 1) {
				cpu->memory.store16(cpu, dest, halfword, 0);
				dest += outwidth;
				remaining -= outwidth;
			}
		} else if (outwidth == 1) {
			cpu->memory.store8(cpu, dest, new, 0);
			dest += outwidth;
			remaining -= outwidth;
		} else {
			cpu->memory.store16(cpu, dest, new, 0);
			dest += outwidth;
			remaining -= outwidth;
		}
		old = new;
		source += inwidth;
	}
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
}

static void _unBitPack(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0];
	uint32_t dest = cpu->gprs[1];
	uint32_t info = cpu->gprs[2];
	unsigned sourceLen = cpu->memory.load16(cpu, info, 0);
	unsigned sourceWidth = cpu->memory.load8(cpu, info + 2, 0);
	unsigned destWidth = cpu->memory.load8(cpu, info + 3, 0);
	switch (sourceWidth) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack source width: %u", sourceWidth);
		return;
	}
	switch (destWidth) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
		break;
	default:
		mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack destination width: %u", destWidth);
		return;
	}
	uint32_t bias = cpu->memory.load32(cpu, info + 4, 0);
	uint8_t in = 0;
	uint32_t out = 0;
	int bitsRemaining = 0;
	int bitsEaten = 0;
	while (sourceLen > 0) {
		if (!bitsRemaining) {
			in = cpu->memory.load8(cpu, source, 0);
			bitsRemaining = 8;
			++source;
			--sourceLen;
		}
		unsigned scaled = in & ((1 << sourceWidth) - 1);
		in >>= sourceWidth;
		if (scaled || bias & 0x80000000) {
			scaled += bias & 0x7FFFFFFF;
			scaled &= (1 << destWidth) - 1;
		}
		bitsRemaining -= sourceWidth;
		out |= scaled << bitsEaten;
		bitsEaten += destWidth;
		if (bitsEaten == 32) {
			cpu->memory.store32(cpu, dest, out, 0);
			bitsEaten = 0;
			out = 0;
			dest += 4;
		}
	}
}
