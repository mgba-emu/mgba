#include <mgba/internal/arm/macros.h>
#include <mgba/internal/defines.h>
#include <mgba/internal/gba/cart/flashrom.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/gba.h>

#include <mgba-util/vfs.h>

static bool _programWord(struct GBAMemory* memory, uint32_t address, uint16_t value);
static bool _eraseBlock(struct GBAMemory* memory, uint32_t address);
static void _eraseChip(struct GBAMemory* memory);

void GBAFlashROMInit(struct GBAFlashROM* flashrom) {
	flashrom->state = FLASHROM_IDLE;
	flashrom->manufacturerId = 0x0001;
	flashrom->deviceId = 0x2258;
	flashrom->status = 0x0000;
	flashrom->dirty = 0;
}

bool GBAFlashROMRead(struct GBAMemory* memory, uint32_t address, uint32_t* value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	switch (flashrom->state) {
	case FLASHROM_IDLE:
	case FLASHROM_CMD_1:
	case FLASHROM_CMD_READY:
	case FLASHROM_UNLOCKED:
	case FLASHROM_UNLOCKED_READY:
	case FLASHROM_LOCK_READY:
		return false;
	case FLASHROM_AUTO_SELECT:
		if ((address & 0x06) == 0) {
			*value = flashrom->manufacturerId;
			return true;
		} else if ((address & 0x06) == 2) { 
		   *value = flashrom->deviceId;
		   return true;
		}
		mLOG(GBA_MEM, GAME_ERROR, "Unexpected FlashROM register: 0x%08X", address);
		return false;
	case FLASHROM_PROGRAM_ERR:
	case FLASHROM_ERASE_ERR:
		*value = flashrom->status;
		return true;
	default:
		mLOG(GBA_MEM, GAME_ERROR, "Unexpected read during FlashROM control: 0x%08X", address);
		return false;
	}
}

bool GBAFlashROMWrite(struct GBAMemory* memory, uint32_t address, uint16_t value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	
	if (value == 0xF0) {
		switch (flashrom->state) {
		case FLASHROM_IDLE:
		case FLASHROM_CMD_1:
		case FLASHROM_CMD_READY:
		case FLASHROM_AUTO_SELECT:
		case FLASHROM_PROGRAM_ERR:
		case FLASHROM_ERASE_1:
		case FLASHROM_ERASE_2:
		FLASHROM_ERASE_ERR:
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			break;
		}
	}
	
	switch (flashrom->state) {
	case FLASHROM_IDLE:
		if (address == 0x08000AAA && value == 0x00A9) {
			flashrom->state = FLASHROM_CMD_1;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		return false;
	case FLASHROM_CMD_1:
		if ((address & ~1) == 0x08000554 && value == 0x0056) {
			flashrom->state = FLASHROM_CMD_READY;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		return false;
	case FLASHROM_CMD_READY:
		if (address != 0x08000AAA) {
			flashrom->state = FLASHROM_IDLE;
			return false;
		}
		switch (value) {
		case 0x20:
			flashrom->state = FLASHROM_UNLOCKED;
			return true;
		case 0x80:
			flashrom->state = FLASHROM_ERASE_1;
			return true;
		case 0x90:
			flashrom->state = FLASHROM_AUTO_SELECT;
			return true;
		case 0xA0:
			flashrom->state = FLASHROM_PROGRAM_READY;
			return true;
		case 0xF0:
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			flashrom->state = FLASHROM_IDLE;
			return false;
		}
	case FLASHROM_PROGRAM_READY:
		if (!_programWord(memory, address, value)) {
			return false;
		}
	
		flashrom->dirty |= mSAVEDATA_DIRT_NEW;
		
		flashrom->state = FLASHROM_IDLE;
		return true;
	case FLASHROM_ERASE_1:
		if (address == 0x08000AAA && value == 0x00A9) {
			flashrom->state = FLASHROM_ERASE_2;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		return false;
	case FLASHROM_ERASE_2:
		if ((address & ~1) == 0x08000554 && value == 0x0056) {
			flashrom->state = FLASHROM_ERASE_READY;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		return false;
	case FLASHROM_ERASE_READY:
		switch (value) {
		case 0x10:
			_eraseChip(memory);
			flashrom->state = FLASHROM_IDLE;
			return true;
		case 0x30:
			_eraseBlock(memory, address);
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			flashrom->state = FLASHROM_IDLE;
			return false;
		}
	case FLASHROM_UNLOCKED:
		switch (value) {
			case 0x90:
				flashrom->state = FLASHROM_LOCK_READY;
				return true;
			case 0xA0:
				flashrom->state = FLASHROM_UNLOCKED_READY;
				return true;
			default:
				return false;
		}
	case FLASHROM_UNLOCKED_READY:
		if (!_programWord(memory, address, value)) {
			flashrom->state = FLASHROM_UNLOCKED;
			return false;
		}
	
		flashrom->dirty |= mSAVEDATA_DIRT_NEW;
		
		flashrom->state = FLASHROM_UNLOCKED;
		return true;
	case FLASHROM_LOCK_READY:
		if (value == 0x00) {
			flashrom->state = FLASHROM_IDLE;
			return true;
		} else {
			flashrom->state = FLASHROM_UNLOCKED;
			return false;
		}
	default:
		flashrom->state = FLASHROM_IDLE;
		return false;
	}
}
	
void GBAFlashROMClean(struct GBA* gba, uint32_t frameCount) {
	if (!gba->romVf) {
		return;
	}
	if (mSavedataClean(&gba->memory.flashrom.dirty, &gba->memory.flashrom.dirtAge, frameCount)) {
		size_t size = gba->memory.romSize;
		if (gba->romVf->sync(gba->romVf, gba->memory.rom, size)) {
			mLOG(GBA_SAVE, INFO, "FlashROM synced");
		} else {
			mLOG(GBA_SAVE, INFO, "FlashROM failed to sync!");
		}
	}
}

static bool _programWord(struct GBAMemory* memory, uint32_t address, uint16_t value) {
    uint32_t offset = address & 0x01FFFFFF;
	if (offset >= memory->romSize) {
		mLOG(GBA_MEM, GAME_ERROR, "FlashROM program out of bounds: 0x%08X", offset);
		return false;
	}
	
	uint16_t word;
	LOAD_16(word, offset, memory->rom);
	STORE_16(word & value, offset, memory->rom);
	return true;
}

static bool _eraseBlock(struct GBAMemory* memory, uint32_t address) {
    uint32_t offset = address & 0x01FF0000;	
	if (offset >= memory->romSize) {
		mLOG(GBA_MEM, GAME_ERROR, "FlashROM block erase out of bounds: 0x%08X", offset);
		return false;
	}
	
	memset(&((uint8_t*) memory->rom)[offset], 0xFF, 0x10000);
	memory->flashrom.dirty |= mSAVEDATA_DIRT_NEW;
	return true;
}

static void _eraseChip(struct GBAMemory* memory) {
    memset(memory->rom, 0xFF, memory->romSize);
	memory->flashrom.dirty |= mSAVEDATA_DIRT_NEW;
}