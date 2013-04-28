#ifndef GBA_SAVEDATA_H
#define GBA_SAVEDATA_H

#include <stdint.h>

enum SavedataType {
	SAVEDATA_NONE = 0,
	SAVEDATA_SRAM,
	SAVEDATA_FLASH512,
	SAVEDATA_FLASH1M,
	SAVEDATA_EEPROM
};

enum {
	SAVEDATA_FLASH_BASE = 0x0E005555
};

struct GBASavedata {
	enum SavedataType type;
	uint8_t* data;
	const char* filename;
	int fd;
};

void GBASavedataInit(struct GBASavedata* savedata, const char* filename);
void GBASavedataDeinit(struct GBASavedata* savedata);

void GBASavedataInitFlash(struct GBASavedata* savedata);
void GBASavedataInitEEPROM(struct GBASavedata* savedata);
void GBASavedataInitSRAM(struct GBASavedata* savedata);

void GBASavedataWriteFlash(struct GBASavedata* savedata, uint8_t value);

uint16_t GBASavedataReadEEPROM(struct GBASavedata* savedata);
void GBASavedataWriteEEPROM(struct GBASavedata* savedata, uint16_t value);

#endif
