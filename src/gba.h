#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#include "arm.h"

enum GBAError {
	GBA_NO_ERROR = 0,
	GBA_OUT_OF_MEMORY = -1
};

enum GBALogLevel {
	GBA_LOG_STUB
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
	SP_BASE_SYSTEM = 0x03FFFF00,
	SP_BASE_IRQ = 0x03FFFFA0,
	SP_BASE_SUPERVISOR = 0x03FFFFE0
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
	struct ARMBoard d;
	struct GBA* p;
};

struct GBA {
	struct ARMCore cpu;
	struct GBABoard board;
	struct GBAMemory memory;

	struct ARMDebugger* debugger;

	enum GBAError errno;
	const char* errstr;
};

void GBAInit(struct GBA* gba);
void GBADeinit(struct GBA* gba);

void GBAMemoryInit(struct GBAMemory* memory);
void GBAMemoryDeinit(struct GBAMemory* memory);

void GBABoardInit(struct GBABoard* board);
void GBABoardReset(struct ARMBoard* board);

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger);

void GBALoadROM(struct GBA* gba, int fd);

int32_t GBALoad32(struct ARMMemory* memory, uint32_t address);
int16_t GBALoad16(struct ARMMemory* memory, uint32_t address);
uint16_t GBALoadU16(struct ARMMemory* memory, uint32_t address);
int8_t GBALoad8(struct ARMMemory* memory, uint32_t address);
uint8_t GBALoadU8(struct ARMMemory* memory, uint32_t address);

void GBAStore32(struct ARMMemory* memory, uint32_t address, int32_t value);
void GBAStore16(struct ARMMemory* memory, uint32_t address, int16_t value);
void GBAStore8(struct ARMMemory* memory, uint32_t address, int8_t value);

#endif
