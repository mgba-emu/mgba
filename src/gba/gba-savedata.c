#include "gba-savedata.h"

#include "gba.h"

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
	if (savedata->fd < 0) {
		GBALog(GBA_LOG_WARN, "Cannot open savedata file %s", savedata->filename);
		return;
	}
	// mmap enough so that we can expand the file if we need to
	savedata->data = mmap(0, SIZE_CART_FLASH1M, PROT_READ | PROT_WRITE, MAP_SHARED, savedata->fd, 0);

	off_t end = lseek(savedata->fd, 0, SEEK_END);
	if (end < SIZE_CART_FLASH512) {
		ftruncate(savedata->fd, SIZE_CART_SRAM);
		memset(&savedata->data[end], 0xFF, SIZE_CART_SRAM - end);
	}
}

void GBASavedataInitEEPROM(struct GBASavedata* savedata) {
	savedata->type = SAVEDATA_EEPROM;
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	if (savedata->fd < 0) {
		GBALog(GBA_LOG_WARN, "Cannot open savedata file %s", savedata->filename);
		return;
	}
	savedata->data = mmap(0, SIZE_CART_EEPROM, PROT_READ | PROT_WRITE, MAP_SHARED, savedata->fd, 0);

	off_t end = lseek(savedata->fd, 0, SEEK_END);
	if (end < SIZE_CART_EEPROM) {
		ftruncate(savedata->fd, SIZE_CART_EEPROM);
		memset(&savedata->data[end], 0xFF, SIZE_CART_EEPROM - end);
	}
}

void GBASavedataInitSRAM(struct GBASavedata* savedata) {
	savedata->type = SAVEDATA_SRAM;
	savedata->fd = open(savedata->filename, O_RDWR | O_CREAT, 0666);
	off_t end;
	int flags = MAP_SHARED;
	if (savedata->fd < 0) {
		GBALog(GBA_LOG_WARN, "Cannot open savedata file %s", savedata->filename);
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
	GBALog(GBA_LOG_STUB, "Flash memory unimplemented");
}

void GBASavedataWriteEEPROM(struct GBASavedata* savedata, uint16_t value) {
	(void)(savedata);
	(void)(value);
	GBALog(GBA_LOG_STUB, "EEPROM unimplemented");
}

uint16_t GBASavedataReadEEPROM(struct GBASavedata* savedata) {
	(void)(savedata);
	GBALog(GBA_LOG_STUB, "EEPROM unimplemented");
	return 0;
}
