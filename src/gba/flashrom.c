#include <mgba/internal/arm/macros.h>
#include <mgba/internal/defines.h>
#include <mgba/internal/gba/flashrom.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/gba.h>

#include <mgba-util/vfs.h>

static bool _programWord(struct GBAMemory* memory, uint32_t address, uint16_t value);
static bool _eraseBlock(struct GBAMemory* memory, uint32_t address);
static void _eraseChip(struct GBAMemory* memory);
static FlashROMBlock *_findBlock(struct GBAMemory* memory, uint32_t offset);
static uint32_t* _offsetList(struct GBAFlashROM* flashrom);

void GBAFlashROMInit(struct GBAFlashROM* flashrom, enum FlashROMType type) {
	flashrom->type = type;
	flashrom->state = FLASHROM_IDLE;
	flashrom->status = 0x0000;
	flashrom->blocks = NULL;
	
	if (type == FLASHROM_22XX) {
		flashrom->manufacturerId = 0x0001;
		flashrom->deviceId = 0x2258;
	} else if (type == FLASHROM_INTEL) {
		flashrom->manufacturerId = 0x008a;
		flashrom->deviceId = 0x8815;
	}
}

bool GBAFlashROMRead22xx(struct GBAMemory* memory, uint32_t address, uint32_t* value) {
	struct GBAFlashROM* flashrom = &memory->savedata.flashrom;
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
	struct GBAFlashROM* flashrom = &memory->savedata.flashrom;
	
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
			flashrom->state = FLASHROM_IDLE;
			return true;
		case 0x30:
			if(_eraseBlock(memory, address)) {
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
	struct GBAFlashROM* flashrom = &memory->savedata.flashrom;
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
	struct GBAFlashROM* flashrom = &memory->savedata.flashrom;
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

bool GBAFlashROMLoad(struct GBA* gba) {
	if (gba->memory.savedata.flashrom.type == FLASHROM_NONE) {
		return true;
	}

	if (gba->isPristine) {
#if !defined(FIXED_ROM_BUFFER) && !defined(__wii__)
		void* newRom = anonymousMemoryMap(SIZE_CART0);
		memcpy(newRom, gba->memory.rom, gba->memory.romSize);
		memset(((uint8_t*) newRom) + gba->memory.romSize, 0xFF, SIZE_CART0 - gba->memory.romSize);
		if (gba->cpu->memory.activeRegion == gba->memory.rom) {
			gba->cpu->memory.activeRegion = newRom;
		}
		if (gba->romVf) {
			gba->romVf->unmap(gba->romVf, gba->memory.rom, gba->memory.romSize);
			gba->romVf->close(gba->romVf);
			gba->romVf = NULL;
		}
		gba->memory.rom = newRom;
		gba->memory.hw.gpioBase = &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1];
#endif
		gba->isPristine = false;
	}

	struct GBASavedata* savedata = &gba->memory.savedata;
	off_t size = savedata->vf->size(savedata->vf);
	savedata->flashrom.blockCount = size / (FLASHROM_BLOCK_SIZE + 4);
	savedata->flashrom.blocks = savedata->vf->map(savedata->vf, size, MAP_WRITE);
	if (size && !savedata->flashrom.blocks) {
		return false;
	}
	uint32_t* offsetList = _offsetList(&savedata->flashrom);
	for (int i = 0; i < savedata->flashrom.blockCount; ++i)
	{
		uint32_t offset;
		LOAD_32(offset, 4 * i, offsetList);
		if (offset != ((offset & 0x01ff0000) | 0x08000000)) {
			savedata->vf->unmap(savedata->vf, savedata->flashrom.blocks, size);
			savedata->vf->truncate(savedata->vf, 0);
			savedata->flashrom.blocks = NULL;
			savedata->flashrom.blockCount = 0;
			return true;
		}
	}
	for (int i = 0; i < savedata->flashrom.blockCount; ++i)
	{
		uint32_t offset;
		LOAD_32(offset, 4 * i, offsetList);
		offset &= 0x01ff0000;
		memcpy(((uint8_t*) gba->memory.rom) + offset, savedata->flashrom.blocks, FLASHROM_BLOCK_SIZE);
	}

	return true;
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
	FlashROMBlock* block = _findBlock(memory, offset & 0x01FF0000);
	if (block) {
		STORE_16(word & value, offset & 0x0000FFFF, block);
	}
	memory->savedata.dirty |= mSAVEDATA_DIRT_NEW;
	return true;
}

static bool _eraseBlock(struct GBAMemory* memory, uint32_t address) {
    uint32_t offset = address & 0x01FF0000;	
	if (offset >= memory->romSize) {
		mLOG(GBA_MEM, GAME_ERROR, "FlashROM block erase out of bounds: 0x%08X", offset);
		return false;
	}
	
	memset(&((uint8_t*) memory->rom)[offset], 0xFF, FLASHROM_BLOCK_SIZE);
	FlashROMBlock* block = _findBlock(memory, offset);
	if (block) {
		memset(block, 0xFF, FLASHROM_BLOCK_SIZE);
	}
	memory->savedata.dirty |= mSAVEDATA_DIRT_NEW;
	return true;
}

static void _eraseChip(struct GBAMemory* memory) {
    memset(memory->rom, 0xFF, memory->romSize);
	memory->savedata.flashrom.blockCount = memory->romSize / FLASHROM_BLOCK_SIZE;
	memory->savedata.vf->truncate(memory->savedata.vf, memory->romSize + 4 * memory->savedata.flashrom.blockCount);
	memset(memory->savedata.flashrom.blocks, 0xFF, memory->romSize);
	for (int i = 0; i < memory->savedata.flashrom.blockCount; ++i) {
		STORE_32(i << 16 | 0x08000000, 4 * i, _offsetList(&memory->savedata.flashrom));
	}
	memory->savedata.dirty |= mSAVEDATA_DIRT_NEW;
}

static FlashROMBlock* _findBlock(struct GBAMemory* memory, uint32_t offset) {
	uint32_t* offsetList = _offsetList(&memory->savedata.flashrom);
	for (int i = 0; i < memory->savedata.flashrom.blockCount; ++i)
	{
		uint32_t foundOffset;
		LOAD_32(foundOffset, 4 * i, offsetList);
		if ((offset | 0x08000000) == foundOffset) {
			return &memory->savedata.flashrom.blocks[i];
		}
	}
	memory->savedata.vf->truncate(memory->savedata.vf, (memory->savedata.flashrom.blockCount + 1) * (FLASHROM_BLOCK_SIZE + 4));
	memory->savedata.vf->unmap(memory->savedata.vf, memory->savedata.flashrom.blocks, memory->savedata.flashrom.blockCount * (FLASHROM_BLOCK_SIZE + 4));
	memory->savedata.flashrom.blocks = memory->savedata.vf->map(memory->savedata.vf, (memory->savedata.flashrom.blockCount + 1) * (FLASHROM_BLOCK_SIZE + 4), MAP_WRITE);
	offsetList = _offsetList(&memory->savedata.flashrom);
	memcpy(offsetList + (FLASHROM_BLOCK_SIZE / 4), offsetList, memory->savedata.flashrom.blockCount * 4);
	memcpy(offsetList, ((uint8_t*) memory->rom) + offset, FLASHROM_BLOCK_SIZE);
	++memory->savedata.flashrom.blockCount;
	offsetList = _offsetList(&memory->savedata.flashrom);
	STORE_32(offset | 0x08000000, 4 * (memory->savedata.flashrom.blockCount - 1), offsetList);
	return &memory->savedata.flashrom.blocks[memory->savedata.flashrom.blockCount - 1];
}

// The list of offsets of modified blocks is appended after all modified blocks.
// Ideally this means flashrom saves are usable as regular saves just by truncating the file.
static uint32_t* _offsetList(struct GBAFlashROM* flashrom) {
	if (!flashrom->blocks) {
		return NULL;
	}
	return (uint32_t*) &flashrom->blocks[flashrom->blockCount];
}