#include "gba-bios.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-memory.h"

#include <math.h>
#include <stdlib.h>

static void _unLz77(struct GBAMemory* memory, uint32_t source, uint8_t* dest);

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu.gprs[0];
	(void)(registers);
	GBALog(GBA_LOG_STUB, "RegisterRamReset unimplemented");
}

static void _CpuSet(struct GBA* gba) {
	uint32_t source = gba->cpu.gprs[0];
	uint32_t dest = gba->cpu.gprs[1];
	uint32_t mode = gba->cpu.gprs[2];
	int count = mode & 0x000FFFFF;
	int fill = mode & 0x01000000;
	int wordsize = (mode & 0x04000000) ? 4 : 2;
	int i;
	if (fill) {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			int32_t word = GBALoad32(&gba->memory.d, source);
			for (i = 0; i < count; ++i) {
				GBAStore32(&gba->memory.d, dest + (i << 2), word);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			uint16_t word = GBALoad16(&gba->memory.d, source);
			for (i = 0; i < count; ++i) {
				GBAStore16(&gba->memory.d, dest + (i << 1), word);
			}
		}
	} else {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			for (i = 0; i < count; ++i) {
				int32_t word = GBALoad32(&gba->memory.d, source + (i << 2));
				GBAStore32(&gba->memory.d, dest + (i << 2), word);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			for (i = 0; i < count; ++i) {
				uint16_t word = GBALoad16(&gba->memory.d, source + (i << 1));
				GBAStore16(&gba->memory.d, dest + (i << 1), word);
			}
		}
	}
}

static void _FastCpuSet(struct GBA* gba) {
	uint32_t source = gba->cpu.gprs[0] & 0xFFFFFFFC;
	uint32_t dest = gba->cpu.gprs[1] & 0xFFFFFFFC;
	uint32_t mode = gba->cpu.gprs[2];
	int count = mode & 0x000FFFFF;
	count = ((count + 7) >> 3) << 3;
	int i;
	if (mode & 0x01000000) {
		int32_t word = GBALoad32(&gba->memory.d, source);
		for (i = 0; i < count; ++i) {
			GBAStore32(&gba->memory.d, dest + (i << 2), word);
		}
	} else {
		for (i = 0; i < count; ++i) {
			int32_t word = GBALoad32(&gba->memory.d, source + (i << 2));
			GBAStore32(&gba->memory.d, dest + (i << 2), word);
		}
	}
}

static void _BgAffineSet(struct GBA* gba) {
	int i = gba->cpu.gprs[2];
	float ox, oy;
	float cx, cy;
	float sx, sy;
	float theta;
	int offset = gba->cpu.gprs[0];
	int destination = gba->cpu.gprs[1];
	int diff = gba->cpu.gprs[3];
	(void)(diff); // Are we supposed to use this?
	float a, b, c, d;
	float rx, ry;
	while (i--) {
		// [ sx   0  0 ]   [ cos(theta)  -sin(theta)  0 ]   [ 1  0  cx - ox ]   [ A B rx ]
		// [  0  sy  0 ] * [ sin(theta)   cos(theta)  0 ] * [ 0  1  cy - oy ] = [ C D ry ]
		// [  0   0  1 ]   [     0            0       1 ]   [ 0  0     1    ]   [ 0 0  1 ]
		ox = GBALoad32(&gba->memory.d, offset) / 256.f;
		oy = GBALoad32(&gba->memory.d, offset + 4) / 256.f;
		cx = GBALoad16(&gba->memory.d, offset + 8);
		cy = GBALoad16(&gba->memory.d, offset + 10);
		sx = GBALoad16(&gba->memory.d, offset + 12) / 256.f;
		sy = GBALoad16(&gba->memory.d, offset + 14) / 256.f;
		theta = (GBALoadU16(&gba->memory.d, offset + 16) >> 8) / 128.f * M_PI;
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
		GBAStore16(&gba->memory.d, destination, a * 256);
		GBAStore16(&gba->memory.d, destination + 2, b * 256);
		GBAStore16(&gba->memory.d, destination + 4, c * 256);
		GBAStore16(&gba->memory.d, destination + 6, d * 256);
		GBAStore32(&gba->memory.d, destination + 8, rx * 256);
		GBAStore32(&gba->memory.d, destination + 12, ry * 256);
		destination += 16;
	}
}

static void _ObjAffineSet(struct GBA* gba) {
	int i = gba->cpu.gprs[2];
	float sx, sy;
	float theta;
	int offset = gba->cpu.gprs[0];
	int destination = gba->cpu.gprs[1];
	int diff = gba->cpu.gprs[3];
	float a, b, c, d;
	while (i--) {
		// [ sx   0 ]   [ cos(theta)  -sin(theta) ]   [ A B ]
		// [  0  sy ] * [ sin(theta)   cos(theta) ] = [ C D ]
		sx = GBALoad16(&gba->memory.d, offset) / 256.f;
		sy = GBALoad16(&gba->memory.d, offset + 2) / 256.f;
		theta = (GBALoadU16(&gba->memory.d, offset + 4) >> 8) / 128.f * M_PI;
		offset += 6;
		// Rotation
		a = d = cosf(theta);
		b = c = sinf(theta);
		// Scale
		a *= sx;
		b *= -sx;
		c *= sy;
		d *= sy;
		GBAStore16(&gba->memory.d, destination, a * 256);
		GBAStore16(&gba->memory.d, destination + diff, b * 256);
		GBAStore16(&gba->memory.d, destination + diff * 2, c * 256);
		GBAStore16(&gba->memory.d, destination + diff * 3, d * 256);
		destination += diff * 4;
	}
}

static void _MidiKey2Freq(struct GBA* gba) {
	uint32_t key = GBALoad32(&gba->memory.d, gba->cpu.gprs[0] + 4);
	gba->cpu.gprs[0] = key / powf(2, (180.f - gba->cpu.gprs[1] - gba->cpu.gprs[2] / 256.f) / 12.f);
}

void GBASwi16(struct ARMBoard* board, int immediate) {
	struct GBA* gba = ((struct GBABoard*) board)->p;
	switch (immediate) {
	case 0x1:
		_RegisterRamReset(gba);
		break;
	case 0x2:
		GBAHalt(gba);
		break;
	case 0x05:
		// VBlankIntrWait
		gba->cpu.gprs[0] = 1;
		gba->cpu.gprs[1] = 1;
		// Fall through:
	case 0x04:
		// IntrWait
		gba->memory.io[REG_IME >> 1] = 1;
		if (!gba->cpu.gprs[0] && gba->memory.io[REG_IF >> 1] & gba->cpu.gprs[1]) {
			break;
		}
		gba->memory.io[REG_IF >> 1] = 0;
		ARMRaiseSWI(&gba->cpu);
		break;
	case 0x6:
		{
			div_t result = div(gba->cpu.gprs[0], gba->cpu.gprs[1]);
			gba->cpu.gprs[0] = result.quot;
			gba->cpu.gprs[1] = result.rem;
			gba->cpu.gprs[3] = abs(result.quot);
		}
		break;
	case 0x7:
		{
			div_t result = div(gba->cpu.gprs[1], gba->cpu.gprs[0]);
			gba->cpu.gprs[0] = result.quot;
			gba->cpu.gprs[1] = result.rem;
			gba->cpu.gprs[3] = abs(result.quot);
		}
		break;
	case 0x8:
		gba->cpu.gprs[0] = sqrt(gba->cpu.gprs[0]);
		break;
	case 0xB:
		_CpuSet(gba);
		break;
	case 0xC:
		_FastCpuSet(gba);
		break;
	case 0xE:
		_BgAffineSet(gba);
		break;
	case 0xF:
		_ObjAffineSet(gba);
		break;
	case 0x11:
	case 0x12:
		switch (gba->cpu.gprs[1] >> BASE_OFFSET) {
			case REGION_WORKING_RAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.wram)[(gba->cpu.gprs[1] & (SIZE_WORKING_RAM - 1))]);
				break;
			case REGION_WORKING_IRAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.iwram)[(gba->cpu.gprs[1] & (SIZE_WORKING_IRAM - 1))]);
				break;
			case REGION_VRAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->video.vram)[(gba->cpu.gprs[1] & 0x0001FFFF)]);
				break;
			default:
				GBALog(GBA_LOG_WARN, "Bad LZ77 destination");
				break;
		}
		break;
	case 0x1F:
		_MidiKey2Freq(gba);
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub software interrupt: %02x", immediate);
	}
}

void GBASwi32(struct ARMBoard* board, int immediate) {
	GBASwi16(board, immediate >> 16);
}

static void _unLz77(struct GBAMemory* memory, uint32_t source, uint8_t* dest) {
	int remaining = (GBALoad32(&memory->d, source) & 0xFFFFFF00) >> 8;
	// We assume the signature byte (0x10) is correct
	int blockheader;
	uint32_t sPointer = source + 4;
	uint8_t* dPointer = dest;
	int blocksRemaining = 0;
	int block;
	uint8_t* disp;
	int bytes;
	while (remaining > 0) {
		if (blocksRemaining) {
			if (blockheader & 0x80) {
				// Compressed
				block = GBALoadU8(&memory->d, sPointer) | (GBALoadU8(&memory->d, sPointer + 1) << 8);
				sPointer += 2;
				disp = dPointer - (((block & 0x000F) << 8) | ((block & 0xFF00) >> 8)) - 1;
				bytes = ((block & 0x00F0) >> 4) + 3;
				while (bytes-- && remaining) {
					--remaining;
					*dPointer = *disp;
					++disp;
					++dPointer;
				}
			} else {
				// Uncompressed
				*dPointer = GBALoadU8(&memory->d, sPointer++);
				++dPointer;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = GBALoadU8(&memory->d, sPointer++);
			blocksRemaining = 8;
		}
	}
}
