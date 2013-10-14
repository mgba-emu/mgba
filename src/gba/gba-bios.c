#include "gba-bios.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-memory.h"

#include <math.h>
#include <stdlib.h>

static void _unLz77(struct GBAMemory* memory, uint32_t source, uint8_t* dest);
static void _unHuffman(struct GBAMemory* memory, uint32_t source, uint32_t* dest);
static void _unRl(struct GBAMemory* memory, uint32_t source, uint8_t* dest);

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu.gprs[0];
	(void)(registers);
	GBALog(gba, GBA_LOG_STUB, "RegisterRamReset unimplemented");
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
			int32_t word = gba->memory.d.load32(&gba->memory.d, source, &gba->cpu.cycles);
			for (i = 0; i < count; ++i) {
				gba->memory.d.store32(&gba->memory.d, dest + (i << 2), word, &gba->cpu.cycles);
				gba->board.d.processEvents(&gba->board.d);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			uint16_t word = gba->memory.d.load16(&gba->memory.d, source, &gba->cpu.cycles);
			for (i = 0; i < count; ++i) {
				gba->memory.d.store16(&gba->memory.d, dest + (i << 1), word, &gba->cpu.cycles);
				gba->board.d.processEvents(&gba->board.d);
			}
		}
	} else {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			for (i = 0; i < count; ++i) {
				int32_t word = gba->memory.d.load32(&gba->memory.d, source + (i << 2), &gba->cpu.cycles);
				gba->memory.d.store32(&gba->memory.d, dest + (i << 2), word, &gba->cpu.cycles);
				gba->board.d.processEvents(&gba->board.d);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			for (i = 0; i < count; ++i) {
				uint16_t word = gba->memory.d.load16(&gba->memory.d, source + (i << 1), &gba->cpu.cycles);
				gba->memory.d.store16(&gba->memory.d, dest + (i << 1), word, &gba->cpu.cycles);
				gba->board.d.processEvents(&gba->board.d);
			}
		}
	}
}

static void _FastCpuSet(struct GBA* gba) {
	uint32_t source = gba->cpu.gprs[0] & 0xFFFFFFFC;
	uint32_t dest = gba->cpu.gprs[1] & 0xFFFFFFFC;
	uint32_t mode = gba->cpu.gprs[2];
	int count = mode & 0x000FFFFF;
	int storeCycles = gba->memory.d.waitMultiple(&gba->memory.d, dest, 4);
	count = ((count + 7) >> 3) << 3;
	int i;
	if (mode & 0x01000000) {
		int32_t word = gba->memory.d.load32(&gba->memory.d, source, &gba->cpu.cycles);
		for (i = 0; i < count; i += 4) {
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 0) << 2), word, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 1) << 2), word, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 2) << 2), word, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 3) << 2), word, 0);
			gba->cpu.cycles += storeCycles;
			gba->board.d.processEvents(&gba->board.d);
		}
	} else {
		int loadCycles = gba->memory.d.waitMultiple(&gba->memory.d, source, 4);
		for (i = 0; i < count; i += 4) {
			int32_t word0 = gba->memory.d.load32(&gba->memory.d, source + ((i + 0) << 2), 0);
			int32_t word1 = gba->memory.d.load32(&gba->memory.d, source + ((i + 1) << 2), 0);
			int32_t word2 = gba->memory.d.load32(&gba->memory.d, source + ((i + 2) << 2), 0);
			int32_t word3 = gba->memory.d.load32(&gba->memory.d, source + ((i + 3) << 2), 0);
			gba->cpu.cycles += loadCycles;
			gba->board.d.processEvents(&gba->board.d);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 0) << 2), word0, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 1) << 2), word1, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 2) << 2), word2, 0);
			gba->memory.d.store32(&gba->memory.d, dest + ((i + 3) << 2), word3, 0);
			gba->cpu.cycles += storeCycles;
			gba->board.d.processEvents(&gba->board.d);
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
		ox = gba->memory.d.load32(&gba->memory.d, offset, 0) / 256.f;
		oy = gba->memory.d.load32(&gba->memory.d, offset + 4, 0) / 256.f;
		cx = gba->memory.d.load16(&gba->memory.d, offset + 8, 0);
		cy = gba->memory.d.load16(&gba->memory.d, offset + 10, 0);
		sx = gba->memory.d.load16(&gba->memory.d, offset + 12, 0) / 256.f;
		sy = gba->memory.d.load16(&gba->memory.d, offset + 14, 0) / 256.f;
		theta = (gba->memory.d.loadU16(&gba->memory.d, offset + 16, 0) >> 8) / 128.f * M_PI;
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
		gba->memory.d.store16(&gba->memory.d, destination, a * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + 2, b * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + 4, c * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + 6, d * 256, 0);
		gba->memory.d.store32(&gba->memory.d, destination + 8, rx * 256, 0);
		gba->memory.d.store32(&gba->memory.d, destination + 12, ry * 256, 0);
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
		sx = gba->memory.d.load16(&gba->memory.d, offset, 0) / 256.f;
		sy = gba->memory.d.load16(&gba->memory.d, offset + 2, 0) / 256.f;
		theta = (gba->memory.d.loadU16(&gba->memory.d, offset + 4, 0) >> 8) / 128.f * M_PI;
		offset += 6;
		// Rotation
		a = d = cosf(theta);
		b = c = sinf(theta);
		// Scale
		a *= sx;
		b *= -sx;
		c *= sy;
		d *= sy;
		gba->memory.d.store16(&gba->memory.d, destination, a * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + diff, b * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + diff * 2, c * 256, 0);
		gba->memory.d.store16(&gba->memory.d, destination + diff * 3, d * 256, 0);
		destination += diff * 4;
	}
}

static void _MidiKey2Freq(struct GBA* gba) {
	uint32_t key = gba->memory.d.load32(&gba->memory.d, gba->cpu.gprs[0] + 4, 0);
	gba->cpu.gprs[0] = key / powf(2, (180.f - gba->cpu.gprs[1] - gba->cpu.gprs[2] / 256.f) / 12.f);
}

void GBASwi16(struct ARMBoard* board, int immediate) {
	struct GBA* gba = ((struct GBABoard*) board)->p;
	if (gba->memory.fullBios) {
		ARMRaiseSWI(&gba->cpu);
		return;
	}
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
	case 0xA:
		gba->cpu.gprs[0] = atan2f(gba->cpu.gprs[1] / 16384.f, gba->cpu.gprs[0] / 16384.f) / (2 * M_PI) * 0x10000;
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
		if (gba->cpu.gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 source");
			break;
		}
		switch (gba->cpu.gprs[1] >> BASE_OFFSET) {
			case REGION_WORKING_RAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.wram)[(gba->cpu.gprs[1] & (SIZE_WORKING_RAM - 1))]);
				break;
			case REGION_WORKING_IRAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.iwram)[(gba->cpu.gprs[1] & (SIZE_WORKING_IRAM - 1))]);
				break;
			case REGION_VRAM:
				_unLz77(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->video.renderer->vram)[(gba->cpu.gprs[1] & 0x0001FFFF)]);
				break;
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 destination");
				break;
		}
		break;
	case 0x13:
		if (gba->cpu.gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman source");
			break;
		}
		switch (gba->cpu.gprs[1] >> BASE_OFFSET) {
			case REGION_WORKING_RAM:
				_unHuffman(&gba->memory, gba->cpu.gprs[0], &((uint32_t*) gba->memory.wram)[(gba->cpu.gprs[1] & (SIZE_WORKING_RAM - 3)) >> 2]);
				break;
			case REGION_WORKING_IRAM:
				_unHuffman(&gba->memory, gba->cpu.gprs[0], &((uint32_t*) gba->memory.iwram)[(gba->cpu.gprs[1] & (SIZE_WORKING_IRAM - 3)) >> 2]);
				break;
			case REGION_VRAM:
				_unHuffman(&gba->memory, gba->cpu.gprs[0], &((uint32_t*) gba->video.renderer->vram)[(gba->cpu.gprs[1] & 0x0001FFFC) >> 2]);
				break;
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman destination");
				break;
		}
		break;
	case 0x14:
	case 0x15:
		if (gba->cpu.gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL source");
			break;
		}
		switch (gba->cpu.gprs[1] >> BASE_OFFSET) {
			case REGION_WORKING_RAM:
				_unRl(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.wram)[(gba->cpu.gprs[1] & (SIZE_WORKING_RAM - 1))]);
				break;
			case REGION_WORKING_IRAM:
				_unRl(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->memory.iwram)[(gba->cpu.gprs[1] & (SIZE_WORKING_IRAM - 1))]);
				break;
			case REGION_VRAM:
				_unRl(&gba->memory, gba->cpu.gprs[0], &((uint8_t*) gba->video.renderer->vram)[(gba->cpu.gprs[1] & 0x0001FFFF)]);
				break;
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL destination");
				break;
		}
		break;
	case 0x1F:
		_MidiKey2Freq(gba);
		break;
	default:
		GBALog(gba, GBA_LOG_STUB, "Stub software interrupt: %02x", immediate);
	}
}

void GBASwi32(struct ARMBoard* board, int immediate) {
	GBASwi16(board, immediate >> 16);
}

static void _unLz77(struct GBAMemory* memory, uint32_t source, uint8_t* dest) {
	int remaining = (memory->d.load32(&memory->d, source, 0) & 0xFFFFFF00) >> 8;
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
				block = memory->d.loadU8(&memory->d, sPointer, 0) | (memory->d.loadU8(&memory->d, sPointer + 1, 0) << 8);
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
				*dPointer = memory->d.loadU8(&memory->d, sPointer++, 0);
				++dPointer;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = memory->d.loadU8(&memory->d, sPointer++, 0);
			blocksRemaining = 8;
		}
	}
}

static void _unHuffman(struct GBAMemory* memory, uint32_t source, uint32_t* dest) {
	source = source & 0xFFFFFFFC;
	uint32_t header = memory->d.load32(&memory->d, source, 0);
	int remaining = header >> 8;
	int bits = header & 0xF;
	if (32 % bits) {
		GBALog(memory->p, GBA_LOG_STUB, "Unimplemented unaligned Huffman");
		return;
	}
	int padding = (4 - remaining) & 0x3;
	remaining &= 0xFFFFFFFC;
	// We assume the signature byte (0x20) is correct
	//var tree = [];
	int treesize = (memory->d.loadU8(&memory->d, source + 4, 0) << 1) + 1;
	int block = 0;
	uint32_t treeBase = source + 5;
	uint32_t sPointer = source + 5 + treesize;
	uint32_t* dPointer = dest;
	uint32_t nPointer = treeBase;
	union HuffmanNode {
		struct {
			unsigned offset : 6;
			unsigned rTerm : 1;
			unsigned lTerm : 1;
		};
		uint8_t packed;
	} node;
	int bitsRemaining;
	int readBits;
	int bitsSeen = 0;
	node.packed = memory->d.load8(&memory->d, nPointer, 0);
	while (remaining > 0) {
		uint32_t bitstream = memory->d.load32(&memory->d, sPointer, 0);
		sPointer += 4;
		for (bitsRemaining = 32; bitsRemaining > 0; --bitsRemaining, bitstream <<= 1) {
			uint32_t next = (nPointer & ~1) + node.offset * 2 + 2;
			if (bitstream & 0x80000000) {
				// Go right
				if (node.rTerm) {
					readBits = memory->d.load8(&memory->d, next + 1, 0);
				} else {
					nPointer = next + 1;
					node.packed = memory->d.load8(&memory->d, nPointer, 0);
					continue;
				}
			} else {
				// Go left
				if (node.lTerm) {
					readBits = memory->d.load8(&memory->d, next, 0);
				} else {
					nPointer = next;
					node.packed = memory->d.load8(&memory->d, nPointer, 0);
					continue;
				}
			}

			block |= (readBits & ((1 << bits) - 1)) << bitsSeen;
			bitsSeen += bits;
			nPointer = treeBase;
			node.packed = memory->d.load8(&memory->d, nPointer, 0);
			if (bitsSeen == 32) {
				bitsSeen = 0;
				*dPointer = block;
				++dPointer;
				remaining -= 4;
				block = 0;
			}
		}

	}
	if (padding) {
		*dPointer = block;
	}
}

static void _unRl(struct GBAMemory* memory, uint32_t source, uint8_t* dest) {
	source = source & 0xFFFFFFFC;
	int remaining = (memory->d.load32(&memory->d, source, 0) & 0xFFFFFF00) >> 8;
	int padding = (4 - remaining) & 0x3;
	// We assume the signature byte (0x30) is correct
	int blockheader;
	int block;
	uint32_t sPointer = source + 4;
	uint8_t* dPointer = dest;
	while (remaining > 0) {
		blockheader = memory->d.loadU8(&memory->d, sPointer++, 0);
		if (blockheader & 0x80) {
			// Compressed
			blockheader &= 0x7F;
			blockheader += 3;
			block = memory->d.loadU8(&memory->d, sPointer++, 0);
			while (blockheader-- && remaining) {
				--remaining;
				*dPointer = block;
				++dPointer;
			}
		} else {
			// Uncompressed
			blockheader++;
			while (blockheader-- && remaining) {
				--remaining;
				*dPointer = memory->d.loadU8(&memory->d, sPointer++, 0);
				++dPointer;
			}
		}
	}
	while (padding--) {
		*dPointer = 0;
		++dPointer;
	}
}
