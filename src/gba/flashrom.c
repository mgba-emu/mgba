#include <mgba/internal/arm/macros.h>
#include <mgba/internal/defines.h>
#include <mgba/internal/gba/flashrom.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/gba.h>

#include <mgba-util/vfs.h>

static bool _programWord(struct GBAMemory* memory, uint32_t address, uint16_t value);
static bool _eraseBlock(struct GBAMemory* memory, uint32_t address);
static void _eraseChip(struct GBAMemory* memory);

void GBAFlashROMInit(struct GBAFlashROM* flashrom, enum FlashROMType type) {
	flashrom->type = type;
	flashrom->state = FLASHROM_IDLE;
	flashrom->status = 0x0000;
	flashrom->dirty = 0;
	
	if (type == FLASHROM_22XX) {
		flashrom->manufacturerId = 0x0001;
		flashrom->deviceId = 0x2258;
	} else if (type == FLASHROM_INTEL) {
		flashrom->manufacturerId = 0x008a;
		flashrom->deviceId = 0x8815;
	}
}

bool GBAFlashROMRead22xx(struct GBAMemory* memory, uint32_t address, uint32_t* value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	switch (flashrom->state) {
	case FLASHROM_IDLE:
	case FLASHROM_22XX_CMD_1:
	case FLASHROM_22XX_CMD_READY:
	case FLASHROM_22XX_UNLOCKED:
	case FLASHROM_22XX_UNLOCKED_READY:
	case FLASHROM_22XX_LOCK_READY:
		return false;
	case FLASHROM_22XX_AUTO_SELECT:
		if ((address & 0x06) == 0) {
			*value = flashrom->manufacturerId;
			return true;
		} else if ((address & 0x06) == 2) { 
		   *value = flashrom->deviceId;
		   return true;
		}
		mLOG(GBA_MEM, GAME_ERROR, "Unexpected FlashROM register: 0x%08X", address);
		return false;
	case FLASHROM_22XX_PROGRAM_ERR:
	case FLASHROM_22XX_ERASE_ERR:
		*value = flashrom->status;
		return true;
	default:
		mLOG(GBA_MEM, GAME_ERROR, "Unhandled FlashROM read: 0x%08X", address);
		return false;
	}
}

bool GBAFlashROMWrite22xx(struct GBAMemory* memory, uint32_t address, uint16_t value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	
	if (value == 0xF0) {
		switch (flashrom->state) {
		case FLASHROM_IDLE:
		case FLASHROM_22XX_CMD_1:
		case FLASHROM_22XX_CMD_READY:
		case FLASHROM_22XX_AUTO_SELECT:
		case FLASHROM_22XX_PROGRAM_ERR:
		case FLASHROM_22XX_ERASE_1:
		case FLASHROM_22XX_ERASE_2:
		case FLASHROM_22XX_ERASE_ERR:
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			break;
		}
	}
	
	switch (flashrom->state) {
	case FLASHROM_IDLE:
		if (address == 0x08000AAA && value == 0x00A9) {
			flashrom->state = FLASHROM_22XX_CMD_1;
			return true;
		}
		break;
	case FLASHROM_22XX_CMD_1:
		if ((address & ~1) == 0x08000554 && value == 0x0056) {
			flashrom->state = FLASHROM_22XX_CMD_READY;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		break;
	case FLASHROM_22XX_CMD_READY:
		if (address != 0x08000AAA) {
			flashrom->state = FLASHROM_IDLE;
			break;
		}
		switch (value) {
		case 0x20:
			flashrom->state = FLASHROM_22XX_UNLOCKED;
			return true;
		case 0x80:
			flashrom->state = FLASHROM_22XX_ERASE_1;
			return true;
		case 0x90:
			flashrom->state = FLASHROM_22XX_AUTO_SELECT;
			return true;
		case 0xA0:
			flashrom->state = FLASHROM_22XX_PROGRAM_READY;
			return true;
		case 0xF0:
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			flashrom->state = FLASHROM_IDLE;
			break;
		}
		break;
	case FLASHROM_22XX_PROGRAM_READY:
		if (_programWord(memory, address, value)) {
			flashrom->dirty |= mSAVEDATA_DIRT_NEW;
			flashrom->state = FLASHROM_IDLE;
			return true;
		} else {
			flashrom->state = FLASHROM_22XX_PROGRAM_ERR;
			return true;
		}
		break;
	case FLASHROM_22XX_ERASE_1:
		if (address == 0x08000AAA && value == 0x00A9) {
			flashrom->state = FLASHROM_22XX_ERASE_2;
			return true;
		} 
		flashrom->state = FLASHROM_IDLE;
		break;
	case FLASHROM_22XX_ERASE_2:
		if ((address & ~1) == 0x08000554 && value == 0x0056) {
			flashrom->state = FLASHROM_22XX_ERASE_READY;
			return true;
		}
		flashrom->state = FLASHROM_IDLE;
		break;
	case FLASHROM_22XX_ERASE_READY:
		switch (value) {
		case 0x10:
			_eraseChip(memory);
			flashrom->dirty |= mSAVEDATA_DIRT_NEW;
			flashrom->state = FLASHROM_IDLE;
			return true;
		case 0x30:
			if(_eraseBlock(memory, address)) {
				flashrom->dirty |= mSAVEDATA_DIRT_NEW;
				flashrom->state = FLASHROM_IDLE;
			} else {
				flashrom->state = FLASHROM_22XX_ERASE_ERR;
			}
			return true;
		default:
			flashrom->state = FLASHROM_IDLE;
			break;
		}
		break;
	case FLASHROM_22XX_UNLOCKED:
		switch (value) {
			case 0x90:
				flashrom->state = FLASHROM_22XX_LOCK_READY;
				return true;
			case 0xA0:
				flashrom->state = FLASHROM_22XX_UNLOCKED_READY;
				return true;
			default:
				break;
		}
		break;
	case FLASHROM_22XX_UNLOCKED_READY:
		if (_programWord(memory, address, value)) {
			flashrom->dirty |= mSAVEDATA_DIRT_NEW;
			flashrom->state = FLASHROM_22XX_UNLOCKED;
		} else {
			flashrom->state = FLASHROM_22XX_UNLOCKED_ERR;
		}
		return true;
	case FLASHROM_22XX_LOCK_READY:
		if (value == 0x00) {
			flashrom->state = FLASHROM_IDLE;
			return true;
		} else {
			flashrom->state = FLASHROM_22XX_UNLOCKED;
		}
		break;
	default:
		mLOG(GBA_MEM, GAME_ERROR, "Write during unhandled FlashROM state");
		break;
	}
	return false;
}

bool GBAFlashROMReadIntel(struct GBAMemory* memory, uint32_t address, uint32_t* value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	switch (flashrom->state) {
	case FLASHROM_IDLE:
	case FLASHROM_INTEL_BLOCK_LOCK:
	case FLASHROM_INTEL_ERASE:
	case FLASHROM_INTEL_PROGRAM:
			return false;
	case FLASHROM_INTEL_IDENTIFY:
		switch (address & 0x0FFFFF) {
		case 0x00000:
			*value = flashrom->manufacturerId;
			return true;
		case 0x00002:
			*value = flashrom->deviceId;
			return true;
		default:
			mLOG(GBA_MEM, GAME_ERROR, "Unknown FlashROM register: 0x%08X", address);
			return false;
		}
	case FLASHROM_INTEL_STATUS:
		*value = flashrom->status;
		return true;
	default:
		mLOG(GBA_MEM, GAME_ERROR, "Unhandled FlashROM read: 0x%08X", address);
		return false;
	}
}

bool GBAFlashROMWriteIntel(struct GBAMemory* memory, uint32_t address, uint16_t value) {
	struct GBAFlashROM* flashrom = &memory->flashrom;
	switch (flashrom->state) {
	case FLASHROM_IDLE:
	case FLASHROM_INTEL_IDENTIFY:
	case FLASHROM_INTEL_STATUS:
		switch (value) {
		case 0x10:
		case 0x40:
			flashrom->state = FLASHROM_INTEL_PROGRAM;
			return true;
		case 0x20:
			flashrom->state = FLASHROM_INTEL_ERASE;
			return true;
		case 0x50:
			// clear status
			return true;
		case 0x60:
			flashrom->state = FLASHROM_INTEL_BLOCK_LOCK;
			return true;
		case 0x70:
			flashrom->state = FLASHROM_INTEL_STATUS;
			return true;
		case 0x90:
			flashrom->state = FLASHROM_INTEL_IDENTIFY;
			return true;
		case 0xFF:
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			break;
		}
		break;
	case FLASHROM_INTEL_BLOCK_LOCK:
		switch (value) {
		case 0xD0:
			// Clear block lock bits
			flashrom->state = FLASHROM_IDLE;
			return true;
		default:
			break;
		}
		flashrom->state = FLASHROM_IDLE;
		break;
	case FLASHROM_INTEL_ERASE:
		switch (value) {
		case 0xD0:
			_eraseBlock(memory, address);
			flashrom->state = FLASHROM_INTEL_STATUS;
			flashrom->status = 0x80;
			return true;
		default:
			break;
		}
		flashrom->state = FLASHROM_IDLE;
		break;
	case FLASHROM_INTEL_PROGRAM:
		_programWord(memory, address, value);
		 flashrom->state = FLASHROM_INTEL_STATUS;
		 flashrom->status = 0x80;
		 return true;
	default:
		mLOG(GBA_MEM, GAME_ERROR, "Write during unhandled FlashROM state");
		break;
	}
		
	return false;
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