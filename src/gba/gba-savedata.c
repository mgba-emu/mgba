#include "gba-savedata.h"

#include "gba.h"
#include "memory.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void _flashSwitchBank(struct GBASavedata* savedata, int bank);
static void _flashErase(struct GBASavedata* savedata);
static void _flashEraseSector(struct GBASavedata* savedata, uint16_t sectorStart);

void GBASavedataInit(struct GBASavedata* savedata, const char* filename) {
	savedata->type = SAVEDATA_NONE;
	savedata->data = 0;
	savedata->command = EEPROM_COMMAND_NULL;
	savedata->flashState = FLASH_STATE_RAW;
	savedata->fd = -1;
	savedata->filename = filename;
}

void GBASavedataForceType(struct GBASavedata* savedata, enum SavedataType type) {
	if (savedata->type != SAVEDATA_NONE) {
		GBALog(0, GBA_LOG_WARN, "Can't re-initialize savedata");
		return;
	}
	savedata->type = type;
}

void GBASavedataDeinit(struct GBASavedata* savedata) {
	switch (savedata->type) {
	case SAVEDATA_SRAM:
		mappedMemoryFree(savedata->data, SIZE_CART_SRAM);
		break;
	case SAVEDATA_FLASH512:
		mappedMemoryFree(savedata->data, SIZE_CART_FLASH512);
		break;
	case SAVEDATA_FLASH1M:
		mappedMemoryFree(savedata->data, SIZE_CART_FLASH1M);
		break;
	case SAVEDATA_EEPROM:
		mappedMemoryFree(savedata->data, SIZE_CART_EEPROM);
		break;
	default:
		break;
	}
	if (savedata->fd >= 0) {
		close(savedata->fd);
	}
	savedata->type = SAVEDATA_NONE;
}

void GBASavedataInitFlash(struct GBASavedata* savedata) {
	if (savedata->type == SAVEDATA_NONE) {
		savedata->type = SAVEDATA_FLASH512;
	}
	if (savedata->type != SAVEDATA_FLASH512 && savedata->type != SAVEDATA_FLASH1M) {
		GBALog(0, GBA_LOG_WARN, "Can't re-initialize savedata");
		return;
	}
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		savedata->data = anonymousMemoryMap(SIZE_CART_FLASH1M);
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_FLASH512) {
			ftruncate(savedata->fd, SIZE_CART_FLASH1M);
		}
		savedata->data = fileMemoryMap(savedata->fd, SIZE_CART_FLASH1M, MEMORY_WRITE);
	}

	savedata->currentBank = savedata->data;
	if (end < SIZE_CART_FLASH512) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_FLASH512 - end);
	}
}

void GBASavedataInitEEPROM(struct GBASavedata* savedata) {
	if (savedata->type == SAVEDATA_NONE) {
		savedata->type = SAVEDATA_EEPROM;
	} else {
		GBALog(0, GBA_LOG_WARN, "Can't re-initialize savedata");
		return;
	}
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		savedata->data = anonymousMemoryMap(SIZE_CART_EEPROM);
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_EEPROM) {
			ftruncate(savedata->fd, SIZE_CART_EEPROM);
		}
		savedata->data = fileMemoryMap(savedata->fd, SIZE_CART_EEPROM, MEMORY_WRITE);
	}
	if (end < SIZE_CART_EEPROM) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_EEPROM - end);
	}
}

void GBASavedataInitSRAM(struct GBASavedata* savedata) {
	if (savedata->type == SAVEDATA_NONE) {
		savedata->type = SAVEDATA_SRAM;
	} else {
		GBALog(0, GBA_LOG_WARN, "Can't re-initialize savedata");
		return;
	}
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		savedata->data = anonymousMemoryMap(SIZE_CART_SRAM);
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_SRAM) {
			ftruncate(savedata->fd, SIZE_CART_SRAM);
		}
		savedata->data = fileMemoryMap(savedata->fd, SIZE_CART_SRAM, MEMORY_WRITE);
	}

	if (end < SIZE_CART_SRAM) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_SRAM - end);
	}
}

uint8_t GBASavedataReadFlash(struct GBASavedata* savedata, uint16_t address) {
	if (savedata->command == FLASH_COMMAND_ID) {
		if (savedata->type == SAVEDATA_FLASH512) {
			if (address < 2) {
				return FLASH_MFG_PANASONIC >> (address * 8);
			}
		} else if (savedata->type == SAVEDATA_FLASH1M) {
			if (address < 2) {
				return FLASH_MFG_SANYO >> (address * 8);
			}
		}
	}
	return savedata->currentBank[address];
}

void GBASavedataWriteFlash(struct GBASavedata* savedata, uint16_t address, uint8_t value) {
	switch (savedata->flashState) {
	case FLASH_STATE_RAW:
		switch (savedata->command) {
		case FLASH_COMMAND_PROGRAM:
			savedata->currentBank[address] = value;
			savedata->command = FLASH_COMMAND_NONE;
			break;
		case FLASH_COMMAND_SWITCH_BANK:
			if (address == 0 && value < 2) {
				_flashSwitchBank(savedata, value);
			} else {
				GBALog(0, GBA_LOG_GAME_ERROR, "Bad flash bank switch");
				savedata->command = FLASH_COMMAND_NONE;
			}
			savedata->command = FLASH_COMMAND_NONE;
			break;
		default:
			if (address == FLASH_BASE_HI && value == FLASH_COMMAND_START) {
				savedata->flashState = FLASH_STATE_START;
			} else {
				GBALog(0, GBA_LOG_GAME_ERROR, "Bad flash write: %#04x = %#02x", address, value);
			}
			break;
		}
		break;
	case FLASH_STATE_START:
		if (address == FLASH_BASE_LO && value == FLASH_COMMAND_CONTINUE) {
			savedata->flashState = FLASH_STATE_CONTINUE;
		} else {
			GBALog(0, GBA_LOG_GAME_ERROR, "Bad flash write: %#04x = %#02x", address, value);
			savedata->flashState = FLASH_STATE_RAW;
		}
		break;
	case FLASH_STATE_CONTINUE:
		savedata->flashState = FLASH_STATE_RAW;
		if (address == FLASH_BASE_HI) {
			switch (savedata->command) {
			case FLASH_COMMAND_NONE:
				switch (value) {
				case FLASH_COMMAND_ERASE:
				case FLASH_COMMAND_ID:
				case FLASH_COMMAND_PROGRAM:
				case FLASH_COMMAND_SWITCH_BANK:
					savedata->command = value;
					break;
				default:
					GBALog(0, GBA_LOG_GAME_ERROR, "Unsupported flash operation: %#02x", value);
					break;
				}
				break;
			case FLASH_COMMAND_ERASE:
				switch (value) {
				case FLASH_COMMAND_ERASE_CHIP:
					_flashErase(savedata);
					break;
				default:
					GBALog(0, GBA_LOG_GAME_ERROR, "Unsupported flash erase operation: %#02x", value);
					break;
				}
				savedata->command = FLASH_COMMAND_NONE;
				break;
			case FLASH_COMMAND_ID:
				if (value == FLASH_COMMAND_TERMINATE) {
					savedata->command = FLASH_COMMAND_NONE;
				}
				break;
			default:
				GBALog(0, GBA_LOG_ERROR, "Flash entered bad state: %#02x", savedata->command);
				savedata->command = FLASH_COMMAND_NONE;
				break;
			}
		} else if (savedata->command == FLASH_COMMAND_ERASE) {
			if (value == FLASH_COMMAND_ERASE_SECTOR) {
				_flashEraseSector(savedata, address);
				savedata->command = FLASH_COMMAND_NONE;
			} else {
				GBALog(0, GBA_LOG_GAME_ERROR, "Unsupported flash erase operation: %#02x", value);
			}
		}
		break;
	}
}

void GBASavedataWriteEEPROM(struct GBASavedata* savedata, uint16_t value, uint32_t writeSize) {
	switch (savedata->command) {
	// Read header
	case EEPROM_COMMAND_NULL:
	default:
		savedata->command = value & 0x1;
		break;
	case EEPROM_COMMAND_PENDING:
		savedata->command <<= 1;
		savedata->command |= value & 0x1;
		if (savedata->command == EEPROM_COMMAND_WRITE) {
			savedata->addressBits = writeSize - 64 - 2;
			savedata->writeAddress = 0;
		} else {
			savedata->addressBits = writeSize - 2;
			savedata->readAddress = 0;
		}
		break;
	// Do commands
	case EEPROM_COMMAND_WRITE:
		// Write
		if (writeSize > 65) {
			savedata->writeAddress <<= 1;
			savedata->writeAddress |= (value & 0x1) << 6;
		} else if (writeSize == 1) {
			savedata->command = EEPROM_COMMAND_NULL;
			savedata->writePending = 1;
		} else {
			uint8_t current = savedata->data[savedata->writeAddress >> 3];
			current &= ~(1 << (0x7 - (savedata->writeAddress & 0x7)));
			current |= (value & 0x1) << (0x7 - (savedata->writeAddress & 0x7));
			savedata->data[savedata->writeAddress >> 3] = current;
			++savedata->writeAddress;
		}
		break;
	case EEPROM_COMMAND_READ_PENDING:
		// Read
		if (writeSize > 1) {
			savedata->readAddress <<= 1;
			if (value & 0x1) {
				savedata->readAddress |= 0x40;
			}
		} else {
			savedata->readBitsRemaining = 68;
			savedata->command = EEPROM_COMMAND_READ;
		}
		break;
	}
}

uint16_t GBASavedataReadEEPROM(struct GBASavedata* savedata) {
	if (savedata->command != EEPROM_COMMAND_READ) {
		return 1;
	}
	--savedata->readBitsRemaining;
	if (savedata->readBitsRemaining < 64) {
		int step = 63 - savedata->readBitsRemaining;
		uint8_t data = savedata->data[(savedata->readAddress + step) >> 3] >> (0x7 - (step & 0x7));
		if (!savedata->readBitsRemaining) {
			savedata->command = EEPROM_COMMAND_NULL;
		}
		return data & 0x1;
	}
	return 0;
}

void _flashSwitchBank(struct GBASavedata* savedata, int bank) {
	savedata->currentBank = &savedata->data[bank << 16];
	if (bank > 0) {
		savedata->type = SAVEDATA_FLASH1M;
		ftruncate(savedata->fd, SIZE_CART_FLASH1M);
	}
}

void _flashErase(struct GBASavedata* savedata) {
	size_t size = 0x10000;
	if (savedata->type == SAVEDATA_FLASH1M) {
		size = 0x20000;
	}
	memset(savedata->data, 0xFF, size);
}

void _flashEraseSector(struct GBASavedata* savedata, uint16_t sectorStart) {
	size_t size = 0x1000;
	if (savedata->type == SAVEDATA_FLASH1M) {
		GBALog(0, GBA_LOG_DEBUG, "Performing unknown sector-size erase at %#04x", sectorStart);
	}
	memset(&savedata->currentBank[sectorStart & ~(size - 1)], 0xFF, size);
}
