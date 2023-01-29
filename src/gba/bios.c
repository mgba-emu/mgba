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
#include <mgba-util/math.h>

const uint32_t GBA_BIOS_CHECKSUM = 0xBAAE187F;
const uint32_t GBA_DS_BIOS_CHECKSUM = 0xBAAE1880;

mLOG_DEFINE_CATEGORY(GBA_BIOS, "GBA BIOS", "gba.bios");

static void _unLz77(struct GBA* gba, int width);
static void _unHuffman(struct GBA* gba);
static void _unRl(struct GBA* gba, int width);
static void _unFilter(struct GBA* gba, int inwidth, int outwidth);
static void _unBitPack(struct GBA* gba);

static int _mulWait(int32_t r) {
	if ((r & 0xFFFFFF00) == 0xFFFFFF00 || !(r & 0xFFFFFF00)) {
		return 1;
	} else if ((r & 0xFFFF0000) == 0xFFFF0000 || !(r & 0xFFFF0000)) {
		return 2;
	} else if ((r & 0xFF000000) == 0xFF000000 || !(r & 0xFF000000)) {
		return 3;
	} else {
		return 4;
	}
}

static void _SoftReset(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->spsr.packed = 0;
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = GBA_SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->spsr.packed = 0;
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = GBA_SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_LR] = 0;
	cpu->gprs[ARM_SP] = GBA_SP_BASE_SYSTEM;
	int8_t flag = ((int8_t*) gba->memory.iwram)[0x7FFA];
	memset(((int8_t*) gba->memory.iwram) + GBA_SIZE_IWRAM - 0x200, 0, 0x200);
	if (flag) {
		cpu->gprs[ARM_PC] = GBA_BASE_EWRAM;
	} else {
		cpu->gprs[ARM_PC] = GBA_BASE_ROM0;
	}
	_ARMSetMode(cpu, MODE_ARM);
	ARMWritePC(cpu);
}

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu->gprs[0];
	struct ARMCore* cpu = gba->cpu;
	cpu->memory.store16(cpu, GBA_BASE_IO | REG_DISPCNT, 0x0080, 0);
	if (registers & 0x01) {
		memset(gba->memory.wram, 0, GBA_SIZE_EWRAM);
	}
	if (registers & 0x02) {
		memset(gba->memory.iwram, 0, GBA_SIZE_IWRAM - 0x200);
	}
	if (registers & 0x04) {
		memset(gba->video.palette, 0, GBA_SIZE_PALETTE_RAM);
	}
	if (registers & 0x08) {
		memset(gba->video.vram, 0, GBA_SIZE_VRAM);
	}
	if (registers & 0x10) {
		memset(gba->video.oam.raw, 0, GBA_SIZE_OAM);
	}
	if (registers & 0x20) {
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SIOCNT, 0x0000, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_RCNT, RCNT_INITIAL, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SIOMLT_SEND, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_JOYCNT, 0, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_JOY_RECV_LO, 0, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_JOY_TRANS_LO, 0, 0);
	}
	if (registers & 0x40) {
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND1CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND1CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND1CNT_X, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND2CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND2CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND3CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND3CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND3CNT_X, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND4CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUND4CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUNDCNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUNDCNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUNDCNT_X, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_SOUNDBIAS, 0x200, 0);
		memset(gba->audio.psg.ch3.wavedata32, 0, sizeof(gba->audio.psg.ch3.wavedata32));
	}
	if (registers & 0x80) {
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DISPSTAT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_VCOUNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG0CNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG1CNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2CNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3CNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG0HOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG0VOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG1HOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG1VOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2HOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2VOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3HOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3VOFS, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2PA, 0x100, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2PB, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2PC, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG2PD, 0x100, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_BG2X_LO, 0, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_BG2Y_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3PA, 0x100, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3PB, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3PC, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BG3PD, 0x100, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_BG3X_LO, 0, 0);
		cpu->memory.store32(cpu, GBA_BASE_IO | REG_BG3Y_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WIN0H, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WIN1H, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WIN0V, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WIN1V, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WININ, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WINOUT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_MOSAIC, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BLDCNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BLDALPHA, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_BLDY, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0SAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0SAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0DAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0DAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA0CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1SAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1SAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1DAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1DAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA1CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2SAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2SAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2DAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2DAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA2CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3SAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3SAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3DAD_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3DAD_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_DMA3CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM0CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM0CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM1CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM1CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM2CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM2CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM3CNT_LO, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_TM3CNT_HI, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_IE, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_IF, 0xFFFF, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_WAITCNT, 0, 0);
		cpu->memory.store16(cpu, GBA_BASE_IO | REG_IME, 0, 0);
	}
	if (registers & 0x9C) {
		gba->video.renderer->reset(gba->video.renderer);
		gba->video.renderer->writeVideoRegister(gba->video.renderer, REG_DISPCNT, gba->memory.io[REG_DISPCNT >> 1]);
		int i;
		for (i = REG_BG0CNT; i < REG_SOUND1CNT_LO; i += 2) {
			gba->video.renderer->writeVideoRegister(gba->video.renderer, i, gba->memory.io[i >> 1]);
		}
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
	gba->memory.activeRegion = GBA_REGION_BIOS;
	uint32_t key = cpu->memory.load32(cpu, cpu->gprs[0] + 4, 0);
	gba->memory.activeRegion = oldRegion;

	cpu->gprs[0] = key / exp2f((180.f - cpu->gprs[1] - cpu->gprs[2] / 256.f) / 12.f);
}

static void _Div(struct GBA* gba, int32_t num, int32_t denom) {
	struct ARMCore* cpu = gba->cpu;
	if (denom == 0) {
		if (num == 0 || num == -1 || num == 1) {
			mLOG(GBA_BIOS, GAME_ERROR, "Attempting to divide %i by zero!", num);
		} else {
			mLOG(GBA_BIOS, FATAL, "Attempting to divide %i by zero!", num);
		}
		// If abs(num) > 1, this should hang, but that would be painful to
		// emulate in HLE, and no game will get into a state under normal
		// operation where it hangs...
		cpu->gprs[0] = (num < 0) ? -1 : 1;
		cpu->gprs[1] = num;
		cpu->gprs[3] = 1;
	} else if (denom == -1 && num == INT32_MIN) {
		mLOG(GBA_BIOS, GAME_ERROR, "Attempting to divide INT_MIN by -1!");
		cpu->gprs[0] = INT32_MIN;
		cpu->gprs[1] = 0;
		cpu->gprs[3] = INT32_MIN;
	} else {
		div_t result = div(num, denom);
		cpu->gprs[0] = result.quot;
		cpu->gprs[1] = result.rem;
		cpu->gprs[3] = abs(result.quot);
	}
	int loops = clz32(denom) - clz32(num);
	if (loops < 1) {
		loops = 1;
	}
	gba->biosStall = 4 /* prologue */ + 13 * loops + 7 /* epilogue */;
}

static int16_t _ArcTan(int32_t i, int32_t* r1, int32_t* r3, uint32_t* cycles) {
	uint32_t currentCycles = 37;
	currentCycles += _mulWait(i * i);
	int32_t a = -((i * i) >> 14);
	currentCycles += _mulWait(0xA9 * a);
	int32_t b = ((0xA9 * a) >> 14) + 0x390;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0x91C;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0xFB6;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0x16AA;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0x2081;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0x3651;
	currentCycles += _mulWait(b * a);
	b = ((b * a) >> 14) + 0xA2F9;
	if (r1) {
		*r1 = a;
	}
	if (r3) {
		*r3 = b;
	}
	*cycles = currentCycles;
	return (i * b) >> 16;
}

static int16_t _ArcTan2(int32_t x, int32_t y, int32_t* r1, uint32_t* cycles) {
	if (!y) {
		*cycles = 11;
		if (x >= 0) {
			return 0;
		}
		return 0x8000;
	}
	if (!x) {
		*cycles = 11;
		if (y >= 0) {
			return 0x4000;
		}
		return 0xC000;
	}
	if (y >= 0) {
		if (x >= 0) {
			if (x >= y) {
				return _ArcTan((y << 14) / x, r1, NULL, cycles);
			}
		} else if (-x >= y) {
			return _ArcTan((y << 14) / x, r1, NULL, cycles) + 0x8000;
		}
		return 0x4000 - _ArcTan((x << 14) / y, r1, NULL, cycles);
	} else {
		if (x <= 0) {
			if (-x > -y) {
				return _ArcTan((y << 14) / x, r1, NULL, cycles) + 0x8000;
			}
		} else if (x >= -y) {
			return _ArcTan((y << 14) / x, r1, NULL, cycles) + 0x10000;
		}
		return 0xC000 - _ArcTan((x << 14) / y, r1, NULL, cycles);
	}
}

static int32_t _Sqrt(uint32_t x, uint32_t* cycles) {
	if (!x) {
		*cycles = 53;
		return 0;
	}
	int32_t currentCycles = 15;
	uint32_t lower;
	uint32_t upper = x;
	uint32_t bound = 1;
	while (bound < upper) {
		upper >>= 1;
		bound <<= 1;
		currentCycles += 6;
	}
	while (true) {
		currentCycles += 6;
		upper = x;
		uint32_t accum = 0;
		lower = bound;
		while (true) {
			currentCycles += 5;
			uint32_t oldLower = lower;
			if (lower <= upper >> 1) {
				lower <<= 1;
			}
			if (oldLower >= upper >> 1) {
				break;
			}
		}
		while (true) {
			currentCycles += 8;
			accum <<= 1;
			if (upper >= lower) {
				++accum;
				upper -= lower;
			}
			if (lower == bound) {
				break;
			}
			lower >>= 1;
		}
		uint32_t oldBound = bound;
		bound += accum;
		bound >>= 1;
		if (bound >= oldBound) {
			bound = oldBound;
			break;
		}
	}
	*cycles = currentCycles;
	return bound;
}

void GBASwi16(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
	mLOG(GBA_BIOS, DEBUG, "SWI: %02X r0: %08X r1: %08X r2: %08X r3: %08X",
	    immediate, cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3]);

	switch (immediate) {
	case 0xF0: // Used for internal stall counting
		cpu->gprs[11] = gba->biosStall;
		return;
	case 0xFA:
		GBAPrintFlush(gba);
		return;
	}

	if (gba->memory.fullBios) {
		ARMRaiseSWI(cpu);
		return;
	}

	bool useStall = false;
	switch (immediate) {
	case GBA_SWI_SOFT_RESET:
		_SoftReset(gba);
		break;
	case GBA_SWI_REGISTER_RAM_RESET:
		_RegisterRamReset(gba);
		break;
	case GBA_SWI_HALT:
		ARMRaiseSWI(cpu);
		return;
	case GBA_SWI_STOP:
		GBAStop(gba);
		break;
	case GBA_SWI_VBLANK_INTR_WAIT:
	// VBlankIntrWait
	// Fall through:
	case GBA_SWI_INTR_WAIT:
		// IntrWait
		ARMRaiseSWI(cpu);
		return;
	case GBA_SWI_DIV:
		useStall = true;
		_Div(gba, cpu->gprs[0], cpu->gprs[1]);
		break;
	case GBA_SWI_DIV_ARM:
		useStall = true;
		_Div(gba, cpu->gprs[1], cpu->gprs[0]);
		break;
	case GBA_SWI_SQRT:
		useStall = true;
		cpu->gprs[0] = _Sqrt(cpu->gprs[0], &gba->biosStall);
		break;
	case GBA_SWI_ARCTAN:
		useStall = true;
		cpu->gprs[0] = _ArcTan(cpu->gprs[0], &cpu->gprs[1], &cpu->gprs[3], &gba->biosStall);
		break;
	case GBA_SWI_ARCTAN2:
		useStall = true;
		cpu->gprs[0] = (uint16_t) _ArcTan2(cpu->gprs[0], cpu->gprs[1], &cpu->gprs[1], &gba->biosStall);
		cpu->gprs[3] = 0x170;
		break;
	case GBA_SWI_CPU_SET:
	case GBA_SWI_CPU_FAST_SET:
		if (cpu->gprs[0] >> BASE_OFFSET < GBA_REGION_EWRAM) {
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
		return;
	case GBA_SWI_GET_BIOS_CHECKSUM:
		cpu->gprs[0] = GBA_BIOS_CHECKSUM;
		cpu->gprs[1] = 1;
		cpu->gprs[3] = GBA_SIZE_BIOS;
		break;
	case GBA_SWI_BG_AFFINE_SET:
		_BgAffineSet(gba);
		break;
	case GBA_SWI_OBJ_AFFINE_SET:
		_ObjAffineSet(gba);
		break;
	case GBA_SWI_BIT_UNPACK:
		if (cpu->gprs[0] < GBA_BASE_EWRAM) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad BitUnPack destination");
		// Fall through
		case GBA_REGION_EWRAM:
		case GBA_REGION_IWRAM:
		case GBA_REGION_VRAM:
			_unBitPack(gba);
			break;
		}
		break;
	case GBA_SWI_LZ77_UNCOMP_WRAM:
	case GBA_SWI_LZ77_UNCOMP_VRAM:
		if (!(cpu->gprs[0] & 0x0E000000)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad LZ77 source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad LZ77 destination");
		// Fall through
		case GBA_REGION_EWRAM:
		case GBA_REGION_IWRAM:
		case GBA_REGION_VRAM:
			useStall = true;
			_unLz77(gba, immediate == GBA_SWI_LZ77_UNCOMP_WRAM ? 1 : 2);
			break;
		}
		break;
	case GBA_SWI_HUFFMAN_UNCOMP:
		if (!(cpu->gprs[0] & 0x0E000000)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad Huffman source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad Huffman destination");
		// Fall through
		case GBA_REGION_EWRAM:
		case GBA_REGION_IWRAM:
		case GBA_REGION_VRAM:
			_unHuffman(gba);
			break;
		}
		break;
	case GBA_SWI_RL_UNCOMP_WRAM:
	case GBA_SWI_RL_UNCOMP_VRAM:
		if (!(cpu->gprs[0] & 0x0E000000)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad RL source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad RL destination");
		// Fall through
		case GBA_REGION_EWRAM:
		case GBA_REGION_IWRAM:
		case GBA_REGION_VRAM:
			_unRl(gba, immediate == GBA_SWI_RL_UNCOMP_WRAM ? 1 : 2);
			break;
		}
		break;
	case GBA_SWI_DIFF_8BIT_UNFILTER_WRAM:
	case GBA_SWI_DIFF_8BIT_UNFILTER_VRAM:
	case GBA_SWI_DIFF_16BIT_UNFILTER:
		if (!(cpu->gprs[0] & 0x0E000000)) {
			mLOG(GBA_BIOS, GAME_ERROR, "Bad UnFilter source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
		default:
			mLOG(GBA_BIOS, GAME_ERROR, "Bad UnFilter destination");
		// Fall through
		case GBA_REGION_EWRAM:
		case GBA_REGION_IWRAM:
		case GBA_REGION_VRAM:
			_unFilter(gba, immediate == GBA_SWI_DIFF_16BIT_UNFILTER ? 2 : 1, immediate == GBA_SWI_DIFF_8BIT_UNFILTER_WRAM ? 1 : 2);
			break;
		}
		break;
	case GBA_SWI_SOUND_BIAS:
		// SoundBias is mostly meaningless here
		mLOG(GBA_BIOS, STUB, "Stub software interrupt: SoundBias (19)");
		break;
	case GBA_SWI_MIDI_KEY_2_FREQ:
		_MidiKey2Freq(gba);
		break;
	case GBA_SWI_SOUND_DRIVER_GET_JUMP_LIST:
		ARMRaiseSWI(cpu);
		return;
	default:
		mLOG(GBA_BIOS, STUB, "Stub software interrupt: %02X", immediate);
	}
	if (useStall) {
		if (gba->biosStall >= 18) {
			gba->biosStall -= 18;
			gba->cpu->cycles += gba->biosStall & 3;
			gba->biosStall &= ~3;
			ARMRaiseSWI(cpu);
		} else {
			gba->cpu->cycles += gba->biosStall;
			useStall = false;
		}
	}
	if (!useStall) {
		gba->cpu->cycles += 45 + cpu->memory.activeNonseqCycles16 /* 8 bit load for SWI # */;
		// Return cycles
		if (gba->cpu->executionMode == MODE_ARM) {
			gba->cpu->cycles += cpu->memory.activeNonseqCycles32 + cpu->memory.activeSeqCycles32;
		} else {
			gba->cpu->cycles += cpu->memory.activeNonseqCycles16 + cpu->memory.activeSeqCycles16;
		}
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
	int cycles = 20;
	int remaining = (cpu->memory.load32(cpu, source, &cycles) & 0xFFFFFF00) >> 8;
	// We assume the signature byte (0x10) is correct
	int blockheader = 0; // Some compilers warn if this isn't set, even though it's trivially provably always set
	source += 4;
	int blocksRemaining = 0;
	uint32_t disp;
	int bytes;
	int byte;
	int halfword = 0;
	while (remaining > 0) {
		cycles += 14;
		if (blocksRemaining) {
			cycles += 18;
			if (blockheader & 0x80) {
				// Compressed
				int block = cpu->memory.load8(cpu, source + 1, &cycles) | (cpu->memory.load8(cpu, source, &cycles) << 8);
				source += 2;
				disp = dest - (block & 0x0FFF) - 1;
				bytes = (block >> 12) + 3;
				while (bytes--) {
					cycles += 10;
					if (remaining) {
						--remaining;
					} else {
						mLOG(GBA_BIOS, GAME_ERROR, "Improperly compressed LZ77 data at %08X. "
						     "This will lead to a buffer overrun at %08X and may crash on hardware.",
						     cpu->gprs[0], cpu->gprs[1]);
						if (gba->vbaBugCompat) {
							break;
						}
					}
					if (width == 2) {
						byte = (int16_t) cpu->memory.load16(cpu, disp & ~1, &cycles);
						if (dest & 1) {
							byte >>= (disp & 1) * 8;
							halfword |= byte << 8;
							cpu->memory.store16(cpu, dest ^ 1, halfword, &cycles);
						} else {
							byte >>= (disp & 1) * 8;
							halfword = byte & 0xFF;
						}
						cycles += 4;
					} else {
						byte = cpu->memory.load8(cpu, disp, &cycles);
						cpu->memory.store8(cpu, dest, byte, &cycles);
					}
					++disp;
					++dest;
				}
			} else {
				// Uncompressed
				byte = cpu->memory.load8(cpu, source, &cycles);
				++source;
				if (width == 2) {
					if (dest & 1) {
						halfword |= byte << 8;
						cpu->memory.store16(cpu, dest ^ 1, halfword, &cycles);
					} else {
						halfword = byte;
					}
				} else {
					cpu->memory.store8(cpu, dest, byte, &cycles);
				}
				++dest;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = cpu->memory.load8(cpu, source, &cycles);
			++source;
			blocksRemaining = 8;
		}
	}
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
	cpu->gprs[3] = 0;
	gba->biosStall = cycles;
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
	while (sourceLen > 0 || bitsRemaining) {
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
	cpu->gprs[0] = source;
	cpu->gprs[1] = dest;
}
