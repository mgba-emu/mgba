#ifndef GBA_FLASHROM_H
#define GBA_FLASHROM_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum FlashROMStateMachine {
	FLASHROM_IDLE,
	FLASHROM_CMD_1,
	FLASHROM_CMD_READY,
	FLASHROM_AUTO_SELECT,
	FLASHROM_PROGRAM_READY,
	FLASHROM_PROGRAM_ERR,
	FLASHROM_ERASE_1,
	FLASHROM_ERASE_2,
	FLASHROM_ERASE_READY,
	FLASHROM_ERASE_ERR
};

struct GBAFlashROM {
	enum FlashROMStateMachine state;
	uint16_t manufacturerId;
	uint16_t deviceId;
	uint16_t status;
	int dirty;
	uint32_t dirtAge;
};

void GBAFlashROMInit(struct GBAFlashROM* flashrom);

struct GBAMemory;

bool GBAFlashROMRead(struct GBAMemory* memory, uint32_t address, uint32_t* value);
bool GBAFlashROMWrite(struct GBAMemory* memory, uint32_t address, uint16_t value);

struct GBA;

void GBAFlashROMClean(struct GBA* gba, uint32_t frameCount);

CXX_GUARD_END

#endif