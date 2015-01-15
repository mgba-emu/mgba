/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-bios.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-memory.h"

const uint32_t GBA_BIOS_CHECKSUM = 0xBAAE187F;
const uint32_t GBA_DS_BIOS_CHECKSUM = 0xBAAE1880;

static void _unLz77(struct GBA* gba, uint32_t source, uint32_t dest, int width);
static void _unHuffman(struct GBA* gba, uint32_t source, uint32_t dest);
static void _unRl(struct GBA* gba, uint32_t source, uint32_t dest, int width);

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu->gprs[0];
	UNUSED(registers);
	GBALog(gba, GBA_LOG_STUB, "RegisterRamReset unimplemented");
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
		ox = cpu->memory.load32(cpu, offset, 0) / 256.f;
		oy = cpu->memory.load32(cpu, offset + 4, 0) / 256.f;
		cx = cpu->memory.load16(cpu, offset + 8, 0);
		cy = cpu->memory.load16(cpu, offset + 10, 0);
		sx = cpu->memory.load16(cpu, offset + 12, 0) / 256.f;
		sy = cpu->memory.load16(cpu, offset + 14, 0) / 256.f;
		theta = (cpu->memory.loadU16(cpu, offset + 16, 0) >> 8) / 128.f * M_PI;
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
		sx = cpu->memory.load16(cpu, offset, 0) / 256.f;
		sy = cpu->memory.load16(cpu, offset + 2, 0) / 256.f;
		theta = (cpu->memory.loadU16(cpu, offset + 4, 0) >> 8) / 128.f * M_PI;
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
	uint32_t key = cpu->memory.load32(cpu, cpu->gprs[0] + 4, 0);
	cpu->gprs[0] = key / powf(2, (180.f - cpu->gprs[1] - cpu->gprs[2] / 256.f) / 12.f);
}

static void _Div(struct GBA* gba, int32_t num, int32_t denom) {
	struct ARMCore* cpu = gba->cpu;
	if (denom != 0) {
		div_t result = div(num, denom);
		cpu->gprs[0] = result.quot;
		cpu->gprs[1] = result.rem;
		cpu->gprs[3] = abs(result.quot);
	} else {
		GBALog(gba, GBA_LOG_GAME_ERROR, "Attempting to divide %i by zero!", num);
		// If abs(num) > 1, this should hang, but that would be painful to
		// emulate in HLE, and no game will get into a state where it hangs...
		cpu->gprs[0] = (num < 0) ? -1 : 1;
		cpu->gprs[1] = num;
		cpu->gprs[3] = 1;
	}
}

void GBASwi16(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
	GBALog(gba, GBA_LOG_SWI, "SWI: %02X r0: %08X r1: %08X r2: %08X r3: %08X",
		immediate, cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3]);

	if (gba->memory.fullBios) {
		ARMRaiseSWI(cpu);
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
		cpu->gprs[0] = sqrt(cpu->gprs[0]);
		break;
	case 0xA:
		cpu->gprs[0] = atan2f(cpu->gprs[1] / 16384.f, cpu->gprs[0] / 16384.f) / (2 * M_PI) * 0x10000;
		break;
	case 0xB:
	case 0xC:
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
	case 0x11:
	case 0x12:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 source");
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 destination");
			case REGION_WORKING_RAM:
			case REGION_WORKING_IRAM:
			case REGION_VRAM:
				_unLz77(gba, cpu->gprs[0], cpu->gprs[1], immediate == 0x11 ? 1 : 2);
				break;
		}
		break;
	case 0x13:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman source");
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman destination");
			case REGION_WORKING_RAM:
			case REGION_WORKING_IRAM:
			case REGION_VRAM:
				_unHuffman(gba, cpu->gprs[0], cpu->gprs[1]);
				break;
		}
		break;
	case 0x14:
	case 0x15:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL source");
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL destination");
			case REGION_WORKING_RAM:
			case REGION_WORKING_IRAM:
			case REGION_VRAM:
				_unRl(gba, cpu->gprs[0], cpu->gprs[1], immediate == 0x14 ? 1 : 2);
				break;
		}
		break;
	case 0x1F:
		_MidiKey2Freq(gba);
		break;
	default:
		GBALog(gba, GBA_LOG_STUB, "Stub software interrupt: %02X", immediate);
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

static void _unLz77(struct GBA* gba, uint32_t source, uint32_t dest, int width) {
	struct ARMCore* cpu = gba->cpu;
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
	// We assume the signature byte (0x10) is correct
	int blockheader;
	uint32_t sPointer = source + 4;
	uint32_t dPointer = dest;
	int blocksRemaining = 0;
	uint32_t disp;
	int bytes;
	int byte;
	int halfword;
	while (remaining > 0) {
		if (blocksRemaining) {
			if (blockheader & 0x80) {
				// Compressed
				int block = cpu->memory.loadU8(cpu, sPointer, 0) | (cpu->memory.loadU8(cpu, sPointer + 1, 0) << 8);
				sPointer += 2;
				disp = dPointer - (((block & 0x000F) << 8) | ((block & 0xFF00) >> 8)) - 1;
				bytes = ((block & 0x00F0) >> 4) + 3;
				while (bytes-- && remaining) {
					--remaining;
					if (width == 2) {
						byte = cpu->memory.loadU16(cpu, disp & ~1, 0);
						if (dPointer & 1) {
							byte >>= (disp & 1) * 8;
							halfword |= byte << 8;
							cpu->memory.store16(cpu, dPointer ^ 1, halfword, 0);
						} else {
							byte >>= (disp & 1) * 8;
							halfword = byte & 0xFF;
						}
					} else {
						byte = cpu->memory.loadU8(cpu, disp, 0);
						cpu->memory.store8(cpu, dPointer, byte, 0);
					}
					++dPointer;
					++disp;
				}
			} else {
				// Uncompressed
				byte = cpu->memory.loadU8(cpu, sPointer, 0);
				++sPointer;
				if (width == 2) {
					if (dPointer & 1) {
						halfword |= byte << 8;
						cpu->memory.store16(cpu, dPointer ^ 1, halfword, 0);
					} else {
						halfword = byte;
					}
				} else {
					cpu->memory.store8(cpu, dPointer, byte, 0);
				}
				++dPointer;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = cpu->memory.loadU8(cpu, sPointer, 0);
			++sPointer;
			blocksRemaining = 8;
		}
	}
}

DECL_BITFIELD(HuffmanNode, uint8_t);
DECL_BITS(HuffmanNode, Offset, 0, 6);
DECL_BIT(HuffmanNode, RTerm, 6);
DECL_BIT(HuffmanNode, LTerm, 7);

static void _unHuffman(struct GBA* gba, uint32_t source, uint32_t dest) {
	struct ARMCore* cpu = gba->cpu;
	source = source & 0xFFFFFFFC;
	uint32_t header = cpu->memory.load32(cpu, source, 0);
	int remaining = header >> 8;
	int bits = header & 0xF;
	if (bits == 0) {
		GBALog(gba, GBA_LOG_GAME_ERROR, "Invalid Huffman bits");
		bits = 8;
	}
	if (32 % bits || bits == 1) {
		GBALog(gba, GBA_LOG_STUB, "Unimplemented unaligned Huffman");
		return;
	}
	// We assume the signature byte (0x20) is correct
	int treesize = (cpu->memory.loadU8(cpu, source + 4, 0) << 1) + 1;
	int block = 0;
	uint32_t treeBase = source + 5;
	uint32_t sPointer = source + 5 + treesize;
	uint32_t dPointer = dest;
	uint32_t nPointer = treeBase;
	HuffmanNode node;
	int bitsRemaining;
	int readBits;
	int bitsSeen = 0;
	node = cpu->memory.load8(cpu, nPointer, 0);
	while (remaining > 0) {
		uint32_t bitstream = cpu->memory.load32(cpu, sPointer, 0);
		sPointer += 4;
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
				cpu->memory.store32(cpu, dPointer, block, 0);
				dPointer += 4;
				remaining -= 4;
				block = 0;
			}
		}
	}
}

static void _unRl(struct GBA* gba, uint32_t source, uint32_t dest, int width) {
	struct ARMCore* cpu = gba->cpu;
	source = source & 0xFFFFFFFC;
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
	int padding = (4 - remaining) & 0x3;
	// We assume the signature byte (0x30) is correct
	int blockheader;
	int block;
	uint32_t sPointer = source + 4;
	uint32_t dPointer = dest;
	int halfword;
	while (remaining > 0) {
		blockheader = cpu->memory.loadU8(cpu, sPointer++, 0);
		if (blockheader & 0x80) {
			// Compressed
			blockheader &= 0x7F;
			blockheader += 3;
			block = cpu->memory.loadU8(cpu, sPointer++, 0);
			while (blockheader-- && remaining) {
				--remaining;
				if (width == 2) {
					if (dPointer & 1) {
						halfword |= block << 8;
						cpu->memory.store16(cpu, dPointer ^ 1, halfword, 0);
					} else {
						halfword = block;
					}
				} else {
					cpu->memory.store8(cpu, dPointer, block, 0);
				}
				++dPointer;
			}
		} else {
			// Uncompressed
			blockheader++;
			while (blockheader-- && remaining) {
				--remaining;
				int byte = cpu->memory.loadU8(cpu, sPointer, 0);
				++sPointer;
				if (width == 2) {
					if (dPointer & 1) {
						halfword |= byte << 8;
						cpu->memory.store16(cpu, dPointer ^ 1, halfword, 0);
					} else {
						halfword = byte;
					}
				} else {
					cpu->memory.store8(cpu, dPointer, byte, 0);
				}
				++dPointer;
			}
		}
	}
	if (width == 2) {
		if (dPointer & 1) {
			--padding;
			++dPointer;
		}
		for (; padding > 0; padding -= 2, dPointer += 2) {
			cpu->memory.store16(cpu, dPointer, 0, 0);
		}
	} else {
		while (padding--) {
			cpu->memory.store8(cpu, dPointer, 0, 0);
			++dPointer;
		}
	}
}
