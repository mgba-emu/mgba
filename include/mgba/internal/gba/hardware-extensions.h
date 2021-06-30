
#ifndef HARDWARE_EXTENSIONS_H
#define HARDWARE_EXTENSIONS_H

#include <mgba-util/common.h>

#include <mgba/core/log.h>
#include <mgba/core/timing.h>


#define HWEX_EXTENSIONS_COUNT  1 

#define HWEX_MORE_RAM_SIZE 0x100000 // 1 MB

struct GBA;
uint16_t GetRegMgbaHwExtensionsEnabled(void);
uint16_t GetRegMgbaHwExtensionsCnt(struct GBA* gba, uint32_t address);
uint16_t GetRegMgbaHwExtensionValue(struct GBA* gba, uint32_t address);
void SetRegMgbaHwExtensionValue(struct GBA* gba, uint32_t address, uint32_t value);

#endif
