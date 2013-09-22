#include "gba-savedata.h"

#include "gba.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void GBASavedataInit(struct GBASavedata* savedata, const char* filename) {
	savedata->type = SAVEDATA_NONE;
	savedata->data = 0;
	savedata->fd = -1;
	savedata->filename = filename;
}

void GBASavedataDeinit(struct GBASavedata* savedata) {
	switch (savedata->type) {
	case SAVEDATA_SRAM:
		munmap(savedata->data, SIZE_CART_SRAM);
		break;
	case SAVEDATA_FLASH512:
		munmap(savedata->data, SIZE_CART_FLASH512);
		break;
	case SAVEDATA_FLASH1M:
		munmap(savedata->data, SIZE_CART_FLASH1M);
		break;
	case SAVEDATA_EEPROM:
		munmap(savedata->data, SIZE_CART_EEPROM);
		break;
	default:
		break;
	}
	close(savedata->fd);
	savedata->type = SAVEDATA_NONE;
}

void GBASavedataInitFlash(struct GBASavedata* savedata) {
	savedata->type = SAVEDATA_FLASH512;
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	int flags = MAP_SHARED;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		flags |= MAP_ANON;
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_FLASH512) {
			ftruncate(savedata->fd, SIZE_CART_FLASH1M);
		}
	}
	// mmap enough so that we can expand the file if we need to
	savedata->data = mmap(0, SIZE_CART_FLASH1M, PROT_READ | PROT_WRITE, flags, savedata->fd, 0);
	if (end < SIZE_CART_FLASH512) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_FLASH512 - end);
	}
}

void GBASavedataInitEEPROM(struct GBASavedata* savedata) {
	savedata->type = SAVEDATA_EEPROM;
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	int flags = MAP_SHARED;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		flags |= MAP_ANON;
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_EEPROM) {
			ftruncate(savedata->fd, SIZE_CART_EEPROM);
		}
	}
	savedata->data = mmap(0, SIZE_CART_EEPROM, PROT_READ | PROT_WRITE, flags, savedata->fd, 0);
	if (end < SIZE_CART_EEPROM) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_EEPROM - end);
	}
}

void GBASavedataInitSRAM(struct GBASavedata* savedata) {
	savedata->type = SAVEDATA_SRAM;
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	int flags = MAP_SHARED;
	if (savedata->fd < 0) {
		GBALog(0, GBA_LOG_ERROR, "Cannot open savedata file %s (errno: %d)", savedata->filename, errno);
		end = 0;
		flags |= MAP_ANON;
	} else {
		end = lseek(savedata->fd, 0, SEEK_END);
		if (end < SIZE_CART_SRAM) {
			ftruncate(savedata->fd, SIZE_CART_SRAM);
		}
	}
	savedata->data = mmap(0, SIZE_CART_SRAM, PROT_READ | PROT_WRITE, flags, savedata->fd, 0);
	if (end < SIZE_CART_SRAM) {
		memset(&savedata->data[end], 0xFF, SIZE_CART_SRAM - end);
	}
}


void GBASavedataWriteFlash(struct GBASavedata* savedata, uint8_t value) {
	(void)(savedata);
	(void)(value);
	GBALog(0, GBA_LOG_STUB, "Flash memory unimplemented");
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
