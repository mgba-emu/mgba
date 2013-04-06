#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#include "arm.h"

enum GBAError {
	GBA_NO_ERROR = 0,
	GBA_OUT_OF_MEMORY = -1
};

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
	OFFSET_MASK = 0x00FFFFFF
};

struct GBAMemory {
	struct ARMMemory d;
	struct GBA* p;

	int32_t* bios;
	int32_t* wram;
	int32_t* iwram;
	int32_t* rom;
};

struct GBABoard {
	struct ARMBoard board;
};

struct GBA {
	struct ARMCore cpu;
	struct GBABoard board;
	struct GBAMemory memory;

	enum GBAError errno;
	const char* errstr;
};

void GBAInit(struct GBA* gba);
void GBADeinit(struct GBA* gba);

void GBAMemoryInit(struct GBAMemory* memory);
void GBAMemoryDeinit(struct GBAMemory* memory);

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address);
int16_t GBALoad16(struct ARMMemory* memory, uint32_t address);
uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address);
int8_t GBALoad8(struct ARMMemory* memory, uint32_t address);
uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address);

void GBAStore32(struct ARMMemory* memory, uint32_t address, int32_t value);
void GBAStore16(struct ARMMemory* memory, uint32_t address, int16_t value);
void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value);

#endif