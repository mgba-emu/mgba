#include "gba-bios.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-memory.h"

const uint32_t GBA_BIOS_CHECKSUM = 0xBAAE187F;
const uint32_t GBA_DS_BIOS_CHECKSUM = 0xBAAE1880;

static void _unLz77(struct GBA* gba, uint32_t source, uint8_t* dest);
static void _unHuffman(struct GBA* gba, uint32_t source, uint32_t* dest);
static void _unRl(struct GBA* gba, uint32_t source, uint8_t* dest);

static void _RegisterRamReset(struct GBA* gba) {
	uint32_t registers = gba->cpu->gprs[0];
	(void)(registers);
	GBALog(gba, GBA_LOG_STUB, "RegisterRamReset unimplemented");
}

static void _CpuSet(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0];
	uint32_t dest = cpu->gprs[1];
	uint32_t mode = cpu->gprs[2];
	int count = mode & 0x000FFFFF;
	int fill = mode & 0x01000000;
	int wordsize = (mode & 0x04000000) ? 4 : 2;
	int i;
	if (fill) {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			int32_t word = cpu->memory.load32(cpu, source, &cpu->cycles);
			for (i = 0; i < count; ++i) {
				cpu->memory.store32(cpu, dest + (i << 2), word, &cpu->cycles);
				cpu->irqh.processEvents(cpu);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			uint16_t word = cpu->memory.load16(cpu, source, &cpu->cycles);
			for (i = 0; i < count; ++i) {
				cpu->memory.store16(cpu, dest + (i << 1), word, &cpu->cycles);
				cpu->irqh.processEvents(cpu);
			}
		}
	} else {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			for (i = 0; i < count; ++i) {
				int32_t word = cpu->memory.load32(cpu, source + (i << 2), &cpu->cycles);
				cpu->memory.store32(cpu, dest + (i << 2), word, &cpu->cycles);
				cpu->irqh.processEvents(cpu);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			for (i = 0; i < count; ++i) {
				uint16_t word = cpu->memory.load16(cpu, source + (i << 1), &cpu->cycles);
				cpu->memory.store16(cpu, dest + (i << 1), word, &cpu->cycles);
				cpu->irqh.processEvents(cpu);
			}
		}
	}
}

static void _FastCpuSet(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	uint32_t source = cpu->gprs[0] & 0xFFFFFFFC;
	uint32_t dest = cpu->gprs[1] & 0xFFFFFFFC;
	uint32_t mode = cpu->gprs[2];
	int count = mode & 0x000FFFFF;
	int storeCycles = cpu->memory.waitMultiple(cpu, dest, 4);
	count = ((count + 7) >> 3) << 3;
	int i;
	if (mode & 0x01000000) {
		int32_t word = cpu->memory.load32(cpu, source, &cpu->cycles);
		for (i = 0; i < count; i += 4) {
			cpu->memory.store32(cpu, dest + ((i + 0) << 2), word, 0);
			cpu->memory.store32(cpu, dest + ((i + 1) << 2), word, 0);
			cpu->memory.store32(cpu, dest + ((i + 2) << 2), word, 0);
			cpu->memory.store32(cpu, dest + ((i + 3) << 2), word, 0);
			cpu->cycles += storeCycles;
			cpu->irqh.processEvents(cpu);
		}
	} else {
		int loadCycles = cpu->memory.waitMultiple(cpu, source, 4);
		for (i = 0; i < count; i += 4) {
			int32_t word0 = cpu->memory.load32(cpu, source + ((i + 0) << 2), 0);
			int32_t word1 = cpu->memory.load32(cpu, source + ((i + 1) << 2), 0);
			int32_t word2 = cpu->memory.load32(cpu, source + ((i + 2) << 2), 0);
			int32_t word3 = cpu->memory.load32(cpu, source + ((i + 3) << 2), 0);
			cpu->cycles += loadCycles;
			cpu->irqh.processEvents(cpu);
			cpu->memory.store32(cpu, dest + ((i + 0) << 2), word0, 0);
			cpu->memory.store32(cpu, dest + ((i + 1) << 2), word1, 0);
			cpu->memory.store32(cpu, dest + ((i + 2) << 2), word2, 0);
			cpu->memory.store32(cpu, dest + ((i + 3) << 2), word3, 0);
			cpu->cycles += storeCycles;
			cpu->irqh.processEvents(cpu);
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
	int diff = cpu->gprs[3];
	(void)(diff); // Are we supposed to use this?
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
		offset += 6;
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

void GBASwi16(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
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
		cpu->gprs[0] = 1;
		cpu->gprs[1] = 1;
		// Fall through:
	case 0x04:
		// IntrWait
		gba->memory.io[REG_IME >> 1] = 1;
		if (!cpu->gprs[0] && gba->memory.io[REG_IF >> 1] & cpu->gprs[1]) {
			break;
		}
		gba->memory.io[REG_IF >> 1] = 0;
		ARMRaiseSWI(cpu);
		break;
	case 0x6:
		{
			div_t result = div(cpu->gprs[0], cpu->gprs[1]);
			cpu->gprs[0] = result.quot;
			cpu->gprs[1] = result.rem;
			cpu->gprs[3] = abs(result.quot);
		}
		break;
	case 0x7:
		{
			div_t result = div(cpu->gprs[1], cpu->gprs[0]);
			cpu->gprs[0] = result.quot;
			cpu->gprs[1] = result.rem;
			cpu->gprs[3] = abs(result.quot);
		}
		break;
	case 0x8:
		cpu->gprs[0] = sqrt(cpu->gprs[0]);
		break;
	case 0xA:
		cpu->gprs[0] = atan2f(cpu->gprs[1] / 16384.f, cpu->gprs[0] / 16384.f) / (2 * M_PI) * 0x10000;
		break;
	case 0xB:
		_CpuSet(gba);
		break;
	case 0xC:
		_FastCpuSet(gba);
		break;
	case 0xD:
		cpu->gprs[0] = GBAChecksum(gba->memory.bios, SIZE_BIOS);
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
			case REGION_WORKING_RAM:
				_unLz77(gba, cpu->gprs[0], &((uint8_t*) gba->memory.wram)[(cpu->gprs[1] & (SIZE_WORKING_RAM - 1))]);
				break;
			case REGION_WORKING_IRAM:
				_unLz77(gba, cpu->gprs[0], &((uint8_t*) gba->memory.iwram)[(cpu->gprs[1] & (SIZE_WORKING_IRAM - 1))]);
				break;
			case REGION_VRAM:
				_unLz77(gba, cpu->gprs[0], &((uint8_t*) gba->video.renderer->vram)[(cpu->gprs[1] & 0x0001FFFF)]);
				break;
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad LZ77 destination");
				break;
		}
		break;
	case 0x13:
		if (cpu->gprs[0] < BASE_WORKING_RAM) {
			GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman source");
			break;
		}
		switch (cpu->gprs[1] >> BASE_OFFSET) {
			case REGION_WORKING_RAM:
				_unHuffman(gba, cpu->gprs[0], &((uint32_t*) gba->memory.wram)[(cpu->gprs[1] & (SIZE_WORKING_RAM - 3)) >> 2]);
				break;
			case REGION_WORKING_IRAM:
				_unHuffman(gba, cpu->gprs[0], &((uint32_t*) gba->memory.iwram)[(cpu->gprs[1] & (SIZE_WORKING_IRAM - 3)) >> 2]);
				break;
			case REGION_VRAM:
				_unHuffman(gba, cpu->gprs[0], &((uint32_t*) gba->video.renderer->vram)[(cpu->gprs[1] & 0x0001FFFC) >> 2]);
				break;
			default:
				GBALog(gba, GBA_LOG_GAME_ERROR, "Bad Huffman destination");
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
			case REGION_WORKING_RAM:
				_unRl(gba, cpu->gprs[0], &((uint8_t*) gba->memory.wram)[(cpu->gprs[1] & (SIZE_WORKING_RAM - 1))]);
				break;
			case REGION_WORKING_IRAM:
				_unRl(gba, cpu->gprs[0], &((uint8_t*) gba->memory.iwram)[(cpu->gprs[1] & (SIZE_WORKING_IRAM - 1))]);
				break;
			case REGION_VRAM:
				_unRl(gba, cpu->gprs[0], &((uint8_t*) gba->video.renderer->vram)[(cpu->gprs[1] & 0x0001FFFF)]);
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

static void _unLz77(struct GBA* gba, uint32_t source, uint8_t* dest) {
	struct ARMCore* cpu = gba->cpu;
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
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
				block = cpu->memory.loadU8(cpu, sPointer, 0) | (cpu->memory.loadU8(cpu, sPointer + 1, 0) << 8);
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
				*dPointer = cpu->memory.loadU8(cpu, sPointer++, 0);
				++dPointer;
				--remaining;
			}
			blockheader <<= 1;
			--blocksRemaining;
		} else {
			blockheader = cpu->memory.loadU8(cpu, sPointer++, 0);
			blocksRemaining = 8;
		}
	}
}

static void _unHuffman(struct GBA* gba, uint32_t source, uint32_t* dest) {
	struct ARMCore* cpu = gba->cpu;
	source = source & 0xFFFFFFFC;
	uint32_t header = cpu->memory.load32(cpu, source, 0);
	int remaining = header >> 8;
	int bits = header & 0xF;
	if (32 % bits) {
		GBALog(gba, GBA_LOG_STUB, "Unimplemented unaligned Huffman");
		return;
	}
	int padding = (4 - remaining) & 0x3;
	remaining &= 0xFFFFFFFC;
	// We assume the signature byte (0x20) is correct
	//var tree = [];
	int treesize = (cpu->memory.loadU8(cpu, source + 4, 0) << 1) + 1;
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
	node.packed = cpu->memory.load8(cpu, nPointer, 0);
	while (remaining > 0) {
		uint32_t bitstream = cpu->memory.load32(cpu, sPointer, 0);
		sPointer += 4;
		for (bitsRemaining = 32; bitsRemaining > 0; --bitsRemaining, bitstream <<= 1) {
			uint32_t next = (nPointer & ~1) + node.offset * 2 + 2;
			if (bitstream & 0x80000000) {
				// Go right
				if (node.rTerm) {
					readBits = cpu->memory.load8(cpu, next + 1, 0);
				} else {
					nPointer = next + 1;
					node.packed = cpu->memory.load8(cpu, nPointer, 0);
					continue;
				}
			} else {
				// Go left
				if (node.lTerm) {
					readBits = cpu->memory.load8(cpu, next, 0);
				} else {
					nPointer = next;
					node.packed = cpu->memory.load8(cpu, nPointer, 0);
					continue;
				}
			}

			block |= (readBits & ((1 << bits) - 1)) << bitsSeen;
			bitsSeen += bits;
			nPointer = treeBase;
			node.packed = cpu->memory.load8(cpu, nPointer, 0);
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

static void _unRl(struct GBA* gba, uint32_t source, uint8_t* dest) {
	struct ARMCore* cpu = gba->cpu;
	source = source & 0xFFFFFFFC;
	int remaining = (cpu->memory.load32(cpu, source, 0) & 0xFFFFFF00) >> 8;
	int padding = (4 - remaining) & 0x3;
	// We assume the signature byte (0x30) is correct
	int blockheader;
	int block;
	uint32_t sPointer = source + 4;
	uint8_t* dPointer = dest;
	while (remaining > 0) {
		blockheader = cpu->memory.loadU8(cpu, sPointer++, 0);
		if (blockheader & 0x80) {
			// Compressed
			blockheader &= 0x7F;
			blockheader += 3;
			block = cpu->memory.loadU8(cpu, sPointer++, 0);
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
				*dPointer = cpu->memory.loadU8(cpu, sPointer++, 0);
				++dPointer;
			}
		}
	}
	while (padding--) {
		*dPointer = 0;
		++dPointer;
	}
}
