#ifndef GBA_FLASHROM_H
#define GBA_FLASHROM_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum FlashROMType {
	FLASHROM_NONE,
	FLASHROM_22XX,
	FLASHROM_INTEL
};

enum FlashROMStateMachine {
	FLASHROM_IDLE,
	
	FLASHROM_22XX_CMD_1,
	FLASHROM_22XX_CMD_READY,
	FLASHROM_22XX_AUTO_SELECT,
	FLASHROM_22XX_PROGRAM_READY,
	FLASHROM_22XX_PROGRAM_ERR,
	FLASHROM_22XX_ERASE_1,
	FLASHROM_22XX_ERASE_2,
	FLASHROM_22XX_ERASE_READY,
	FLASHROM_22XX_ERASE_ERR,
	FLASHROM_22XX_UNLOCKED,
	FLASHROM_22XX_UNLOCKED_READY,
	FLASHROM_22XX_UNLOCKED_ERR,
	FLASHROM_22XX_LOCK_READY,
	
	FLASHROM_INTEL_IDENTIFY,
	FLASHROM_INTEL_BLOCK_LOCK,
	FLASHROM_INTEL_ERASE,
	FLASHROM_INTEL_PROGRAM,
	FLASHROM_INTEL_STATUS
};

struct GBAFlashROM {
	enum FlashROMType type;
	enum FlashROMStateMachine state;
	uint16_t manufacturerId;
	uint16_t deviceId;
	uint16_t status;
	int dirty;
	uint32_t dirtAge;
};

void GBAFlashROMInit(struct GBAFlashROM* flashrom, enum FlashROMType type);

struct GBAMemory;

bool GBAFlashROMRead22xx(struct GBAMemory* memory, uint32_t address, uint32_t* value);
bool GBAFlashROMWrite22xx(struct GBAMemory* memory, uint32_t address, uint16_t value);

bool GBAFlashROMReadIntel(struct GBAMemory* memory, uint32_t address, uint32_t* value);
bool GBAFlashROMWriteIntel(struct GBAMemory* memory, uint32_t address, uint16_t value);

struct GBA;

void GBAFlashROMClean(struct GBA* gba, uint32_t frameCount);

CXX_GUARD_END

#endif