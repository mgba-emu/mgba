/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "bios.h"

#include "gba/gba.h"
#include "gba/io.h"
#include "gba/memory.h"
#include "isa-inlines.h"

#ifndef M_PI
#define M_PI 3.141592654f
#endif

const uint32_t GBA_BIOS_CHECKSUM = 0xBAAE187F;
const uint32_t GBA_DS_BIOS_CHECKSUM = 0xBAAE1880;

static void _unLz77(struct GBA* gba, int width);
static void _unHuffman(struct GBA* gba);
static void _unRl(struct GBA* gba, int width);
static void _unFilter(struct GBA* gba, int inwidth, int outwidth);

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
		memset(gba->video.renderer->vram, 0, SIZE_VRAM);
	}
	if (registers & 0x10) {
		memset(gba->video.oam.raw, 0, SIZE_OAM);
	}
	if (registers & 0x20) {
		GBALog(gba, GBA_LOG_STUB, "RegisterRamReset on SIO unimplemented");
	}
	if (registers & 0x40) {
		GBALog(gba, GBA_LOG_STUB, "RegisterRamReset on Audio unimplemented");
	}
	if (registers & 0x80) {
		GBALog(gba, GBA_LOG_STUB, "RegisterRamReset on IO unimplemented");
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
	case 0x0:
		_SoftReset(gba);
		break;
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
		if (cpu->gprs[0] >> BASE_OFFSET == REGION_BIOS) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Cannot CpuSet from BIOS");
			return;
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
	case 0x11:
	case 0x12:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 destination");
			case REGION_WORKING_RAM:
			case REGION_WORKING_IRAM:
			case REGION_VRAM:
				_unLz77(gba, immediate == 0x11 ? 1 : 2);
				break;
		}
		break;
	case 0x13:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman destination");
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
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad RL destination");
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
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad UnFilter source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad UnFilter destination");
			case REGION_WORKING_RAM:
			case REGION_WORKING_IRAM:
			case REGION_VRAM:
				_unFilter(gba, immediate == 0x18 ? 2 : 1, immediate == 0x16 ? 1 : 2);
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
				while (bytes-- && remaining) {
					--remaining;
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
	uint32_t source = cpu->gprs[0] & 0xFFFFFFFC;
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
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
