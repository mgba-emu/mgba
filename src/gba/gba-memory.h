#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#include "arm.h"

#include "gba-savedata.h"

#include <string.h>

enum GBAMemoryRegion {
	REGION_BIOS = 0x0,
	REGION_WORKING_RAM = 0x2,
	REGION_WORKING_IRAM = 0x3,
	REGION_IO = 0x4,
	REGION_PALETTE_RAM = 0x5,
	REGION_VRAM = 0x6,
	REGION_OAM = 0x7,
	REGION_CART0 = 0x8,
	REGION_CART0_EX = 0x9,
	REGION_CART1 = 0xA,
	REGION_CART1_EX = 0xB,
	REGION_CART2 = 0xC,
	REGION_CART2_EX = 0xD,
	REGION_CART_SRAM = 0xE
};

enum GBAMemoryBase {
	BASE_BIOS = 0x00000000,
	BASE_WORKING_RAM = 0x02000000,
	BASE_WORKING_IRAM = 0x03000000,
	BASE_IO = 0x04000000,
	BASE_PALETTE_RAM = 0x05000000,
	BASE_VRAM = 0x06000000,
	BASE_OAM = 0x07000000,
	BASE_CART0 = 0x08000000,
	BASE_CART0_EX = 0x09000000,
	BASE_CART1 = 0x0A000000,
	BASE_CART1_EX = 0x0B000000,
	BASE_CART2 = 0x0C000000,
	BASE_CART2_EX = 0x0D000000,
	BASE_CART_SRAM = 0x0E000000
};

enum {
	SIZE_BIOS = 0x00004000,
	SIZE_WORKING_RAM = 0x00040000,
	SIZE_WORKING_IRAM = 0x00008000,
	SIZE_IO = 0x00000400,
	SIZE_PALETTE_RAM = 0x00000400,
	SIZE_VRAM = 0x00018000,
	SIZE_OAM = 0x00000400,
	SIZE_CART0 = 0x02000000,
	SIZE_CART1 = 0x02000000,
	SIZE_CART2 = 0x02000000,
	SIZE_CART_SRAM = 0x00008000,
	SIZE_CART_FLASH512 = 0x00010000,
	SIZE_CART_FLASH1M = 0x00020000,
	SIZE_CART_EEPROM = 0x00002000
};

enum {
	OFFSET_MASK = 0x00FFFFFF,
	BASE_OFFSET = 24
};

enum DMAControl {
	DMA_INCREMENT = 0,
	DMA_DECREMENT = 1,
	DMA_FIXED = 2,
	DMA_INCREMENT_RELOAD = 3
};

enum DMATiming {
	DMA_TIMING_NOW = 0,
	DMA_TIMING_VBLANK = 1,
	DMA_TIMING_HBLANK = 2,
	DMA_TIMING_CUSTOM = 3
};

struct GBADMA {
	union {
		struct {
			int : 5;
			enum DMAControl dstControl : 2;
			enum DMAControl srcControl : 2;
			unsigned repeat : 1;
			unsigned width : 1;
			unsigned drq : 1;
			enum DMATiming timing : 2;
			unsigned doIrq : 1;
			unsigned enable : 1;
		};
		uint16_t packed;
	};

	uint32_t source;
	uint32_t dest;
	int32_t count;
	uint32_t nextSource;
	uint32_t nextDest;
	int32_t nextCount;
	int32_t nextIRQ;
};

struct GBAMemory {
	struct ARMMemory d;
	struct GBA* p;

	uint32_t* bios;
	uint32_t* wram;
	uint32_t* iwram;
	uint32_t* rom;
	uint16_t io[SIZE_IO >> 1];

	struct GBASavedata savedata;
	size_t romSize;

	char waitstates32[256];
	char waitstates16[256];
	char waitstatesSeq32[256];
	char waitstatesSeq16[256];
	int activeRegion;

	struct GBADMA dma[4];
};

int32_t GBAMemoryProcessEvents(struct GBAMemory* memory, int32_t cycles);

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address);
int16_t GBALoad16(struct ARMMemory* memory, uint32_t address);
uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address);
int8_t GBALoad8(struct ARMMemory* memory, uint32_t address);
uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address);

void GBAStore32(struct ARMMemory* memory, uint32_t address, int32_t value);
void GBAStore16(struct ARMMemory* memory, uint32_t address, int16_t value);
void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value);

void GBAAdjustWaitstates(struct GBAMemory* memory, uint16_t parameters);

void GBAMemoryWriteDMASAD(struct GBAMemory* memory, int dma, uint32_t address);
void GBAMemoryWriteDMADAD(struct GBAMemory* memory, int dma, uint32_t address);
void GBAMemoryWriteDMACNT_LO(struct GBAMemory* memory, int dma, uint16_t count);
uint16_t GBAMemoryWriteDMACNT_HI(struct GBAMemory* memory, int dma, uint16_t control);

void GBAMemoryScheduleDMA(struct GBAMemory* memory, int number, struct GBADMA* info);
void GBAMemoryServiceDMA(struct GBAMemory* memory, int number, struct GBADMA* info);
void GBAMemoryRunHblankDMAs(struct GBAMemory* memory);
void GBAMemoryRunVblankDMAs(struct GBAMemory* memory);

#endif
