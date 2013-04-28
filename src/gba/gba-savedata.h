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

enum SavedataCommand {
	EEPROM_COMMAND_NULL = 0,
	EEPROM_COMMAND_PENDING = 1,
	EEPROM_COMMAND_WRITE = 2,
	EEPROM_COMMAND_READ_PENDING = 3,
	EEPROM_COMMAND_READ = 4
};

enum {
	SAVEDATA_FLASH_BASE = 0x0E005555
};

struct GBASavedata {
	enum SavedataType type;
	uint8_t* data;
	const char* filename;
	enum SavedataCommand command;
	int fd;

	int readBitsRemaining;
	int readAddress;
	int writeAddress;
	int writePending;
	int addressBits;
};

void GBASavedataInit(struct GBASavedata* savedata, const char* filename);
void GBASavedataDeinit(struct GBASavedata* savedata);

void GBASavedataInitFlash(struct GBASavedata* savedata);
void GBASavedataInitEEPROM(struct GBASavedata* savedata);
void GBASavedataInitSRAM(struct GBASavedata* savedata);

void GBASavedataWriteFlash(struct GBASavedata* savedata, uint8_t value);

uint16_t GBASavedataReadEEPROM(struct GBASavedata* savedata);
void GBASavedataWriteEEPROM(struct GBASavedata* savedata, uint16_t value, uint32_t writeSize);

#endif
