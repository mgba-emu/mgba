#ifndef GBA_BIOS_H
#define GBA_BIOS_H

#include "arm.h"

void GBASwi16(struct ARMBoard* board, int immediate);
void GBASwi32(struct ARMBoard* board, int immediate);

#endif
