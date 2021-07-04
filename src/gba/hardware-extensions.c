/* Copyright (c) 2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/hardware-extensions.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>

static uint16_t* GetHwExIOPointer(struct GBA* gba, uint32_t address);
static uint32_t GetHwExMemoryIndex32FromAddress(uint32_t address);

static enum HWEX_RETURN_CODES {
    HWEX_RET_OK = 0,
    HWEX_RET_WAIT = 1,

    // Errors
    HWEX_RET_ERR_UNKNOWN = 0x100,
    HWEX_RET_ERR_BAD_ADDRESS = 0x101,
    HWEX_RET_ERR_INVALID_PARAMETERS = 0x102,
    HWEX_RET_ERR_WRITE_TO_ROM = 0x103,
    HWEX_RET_ERR_ABORTED = 0x104
};

static enum {
    HWEX_ID_MORE_RAM = 0
};

void GBAHardwareExtensionsInit(struct GBAHardwareExtensions* hwExtensions) {
	hwExtensions->enabled = false;

    // TODO: read this from the config
    hwExtensions->userEnabled = true;
    memset(hwExtensions->userEnabledFlags, 0xFF, sizeof(hwExtensions->userEnabledFlags));

    memset(hwExtensions->memory, 0, sizeof(hwExtensions->memory));
}

uint16_t _GBAHardwareExtensionsIORead(struct GBA* gba, uint32_t address) {
    switch (address) {
        case REG_HWEX_ENABLE:
            return 0x1DEA;
        case REG_HWEX_VERSION:
            return REG_HWEX_VERSION_VALUE;
        case REG_HWEX_ENABLE_FLAGS_0:
        case REG_HWEX_ENABLE_FLAGS_1:
        case REG_HWEX_ENABLE_FLAGS_2:
        case REG_HWEX_ENABLE_FLAGS_3:
        case REG_HWEX_ENABLE_FLAGS_4:
        case REG_HWEX_ENABLE_FLAGS_5: {
            uint32_t index = (address - REG_HWEX_ENABLE_FLAGS_0) >> 1;
            return *GetHwExIOPointer(gba, address) & gba->hwExtensions.userEnabledFlags[index];
        }
            
        default:
            return *GetHwExIOPointer(gba, address);
    }
}

uint16_t GBAHardwareExtensionsIORead(struct GBA* gba, uint32_t address) {
    if (gba->hwExtensions.enabled && gba->hwExtensions.userEnabled) {
        return _GBAHardwareExtensionsIORead(gba, address);
    }
    mLOG(GBA_IO, GAME_ERROR, "Read from unused I/O register: %03X", address);
    return GBALoadBad(gba->cpu);
}

uint32_t GBAHardwareExtensionsIORead32(struct GBA* gba, uint32_t address) {
    return gba->hwExtensions.memory[GetHwExMemoryIndex32FromAddress(address)];
};

static void* GBAHardwareExtensionsIOReadPointer(struct GBA* gba, uint32_t address, uint32_t* memoryMaxSize, bool* isRom) {
    uint32_t addressPrefix = (address >> 24) & 0xF;
    uint8_t* pointer = NULL;
    *isRom = false;

    if (addressPrefix == 2) {
        if ((address & 0xFFFFFF) < SIZE_WORKING_RAM) {
            pointer = (uint8_t*) gba->memory.wram;
            pointer += address & 0xFFFFFF;
            *memoryMaxSize = SIZE_WORKING_RAM - (address & 0xFFFFFF);
        }
    } else if (addressPrefix == 3) {
        if ((address & 0xFFFFFF) < SIZE_WORKING_IRAM) {
            pointer = (uint8_t*) gba->memory.iwram;
            mLOG(GBA_IO, GAME_ERROR, "addr: %03X", pointer);
            pointer += address & 0xFFFFFF;
            mLOG(GBA_IO, GAME_ERROR, "addr2: %03X", pointer);
            *memoryMaxSize = SIZE_WORKING_IRAM - (address & 0xFFFFFF);
        }
    } else if (addressPrefix & 8) {
        if ((address & 0x1FFFFFF) < gba->memory.romSize) {
            *isRom = true;
            pointer = (uint8_t*) gba->memory.rom;
            pointer += address & 0x1FFFFFF;
            *memoryMaxSize = gba->memory.romSize - (address & 0x1FFFFFF);
        }
    }

    return pointer;
}

static enum HwExMoreRAMCommands {
    HwExtMoreRAMMemoryWrite = 0,
    HwExtMoreRAMMemoryRead = 1,
    HwExtMoreRAMMemorySwap = 2
};


static uint16_t MGBAHwExtMoreRAM(struct GBA* gba) {
    uint32_t command = GBAHardwareExtensionsIORead32(gba, REG_HWEX0_P0_LO);
    uint32_t index = GBAHardwareExtensionsIORead32(gba, REG_HWEX0_P1_LO);
    uint32_t dataPointer = GBAHardwareExtensionsIORead32(gba, REG_HWEX0_P2_LO);
    uint32_t dataSize = GBAHardwareExtensionsIORead32(gba, REG_HWEX0_P3_LO);
    void* data;
    // Check if the pointer is valid
    bool isRom;
    uint32_t memoryMaxSize;
    data = GBAHardwareExtensionsIOReadPointer(gba, dataPointer, &memoryMaxSize, &isRom);
    if (data == NULL) {
        mLOG(GBA_IO, GAME_ERROR, "Bad address: %03X", dataPointer);
        return HWEX_RET_ERR_BAD_ADDRESS;
    }

    // Check if index and size are valid
    if (index >= HWEX_MORE_RAM_SIZE || (index + dataSize) >= HWEX_MORE_RAM_SIZE || dataSize >= memoryMaxSize) {
        return HWEX_RET_ERR_INVALID_PARAMETERS;
    }
    
    switch (command) {
        case HwExtMoreRAMMemoryWrite:
            mLOG(GBA_IO, GAME_ERROR, "write: %03X", dataPointer);
            memcpy(((uint8_t*)gba->hwExtensions.moreRam) + index, data, dataSize);
            break;
        case HwExtMoreRAMMemoryRead:
            if (isRom) {
                return HWEX_RET_ERR_WRITE_TO_ROM;
            }
            memcpy(data, ((uint8_t*)gba->hwExtensions.moreRam) + index, dataSize);
            break;
        case HwExtMoreRAMMemorySwap:
            if (isRom) {
                return HWEX_RET_ERR_WRITE_TO_ROM;
            }
            // TODO: make this more efficient
            uint8_t* data1 = data;
            uint8_t* data2 = gba->hwExtensions.moreRam;
            data2 += index;
            for (uint32_t i = 0; i < dataSize; i++) {
                uint8_t aux = data1[i];
                data1[i] = data2[i];
                data2[i] = aux;
            }
            break;
        default:
            // invalid command
            return HWEX_RET_ERR_INVALID_PARAMETERS;
    }

    return HWEX_RET_OK;
}


static uint32_t GetHwExMemoryIndex16FromAddress(uint32_t address) {
    uint32_t ret = (address - REG_HWEX_ENABLE_FLAGS_0) >> 1;
    return ret;
}
static uint32_t GetHwExMemoryIndex32FromAddress(uint32_t address) {
    return (address - REG_HWEX_ENABLE_FLAGS_0) >> 2;
}

static uint16_t* GetHwExIOPointer(struct GBA* gba, uint32_t address) {
    return ((uint16_t*) &gba->hwExtensions.memory) + GetHwExMemoryIndex16FromAddress(address);
}

static void _GBAHardwareExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
    *GetHwExIOPointer(gba, address) = value;
}

// CNT flags
// Writable
#define HWEX_CNT_FLAG_CALL 1
#define HWEX_CNT_ALL_WRITABLE (HWEX_CNT_FLAG_CALL)
// Read only
#define HWEX_CNT_FLAG_PROCESSING (1 << 15)

struct HardwareExtensionHandlers {
    uint16_t (*onCall)(struct GBA* gba);
    bool (*onAbort)(void);
};

static const struct HardwareExtensionHandlers extensionHandlers[] = {
    [HWEX_ID_MORE_RAM] = { .onCall = MGBAHwExtMoreRAM, .onAbort = NULL }
};

#define SIMPLIFY_HWEX_REG_ADDRESS(address) ((address - REG_HWEX0_CNT) >> 1)

const uint16_t extensionIdByRegister[] = {
    // More RAM
    [SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_CNT)] = HWEX_ID_MORE_RAM,
    [SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_RET_CODE)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P0_LO)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P0_HI)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P1_LO)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P1_HI)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P2_LO)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P2_HI)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P3_LO)] = HWEX_ID_MORE_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P3_HI)] = HWEX_ID_MORE_RAM,
};

static uint16_t GetExtensionIdFromAddress(uint32_t address) {
    return extensionIdByRegister[SIMPLIFY_HWEX_REG_ADDRESS(address)];
}

#undef SIMPLIFY_HWEX_REG_ADDRESS


static void GBAHardwareExtensionsHandleCntWrite(struct GBA* gba, uint32_t cntAddress, uint32_t value) {
    uint16_t* cnt = GetHwExIOPointer(gba, cntAddress);
    uint16_t* returnCode = cnt + 1;
    uint16_t currentValue = *cnt;
    const struct HardwareExtensionHandlers* handlers = extensionHandlers + GetExtensionIdFromAddress(cntAddress);
    value &= HWEX_CNT_ALL_WRITABLE; // delete non-writable flags
    
    if (value != (currentValue & HWEX_CNT_ALL_WRITABLE)) { // check if value changed
        if (currentValue & HWEX_CNT_FLAG_PROCESSING) { // check if extension is running
            if (!(currentValue & HWEX_CNT_FLAG_CALL)) {
                // call flag set to 0
                // abort
                if (handlers->onAbort) {
                    handlers->onAbort();
                    *cnt = currentValue;
                    *returnCode = HWEX_RET_ERR_ABORTED;
                }
            }
        } else {
            if (value & HWEX_CNT_FLAG_CALL) { // check call the extension
                *returnCode = handlers->onCall(gba);
                if (*returnCode == HWEX_RET_OK || *returnCode >= HWEX_RET_ERR_UNKNOWN) { // execution finished
                    *cnt = 0;
                } else { // processing
                    *cnt = currentValue | HWEX_CNT_FLAG_CALL | HWEX_CNT_FLAG_PROCESSING;
                }
            }
        }
    }
    
    return;
}


void GBAHardwareExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
    switch (address) {
        case REG_HWEX_ENABLE:
            gba->hwExtensions.enabled = value == 0xC0DE;
            break;
        // Enable flags
        case REG_HWEX_ENABLE_FLAGS_0:
        case REG_HWEX_ENABLE_FLAGS_1:
        case REG_HWEX_ENABLE_FLAGS_2:
        case REG_HWEX_ENABLE_FLAGS_3:
        case REG_HWEX_ENABLE_FLAGS_4:
        case REG_HWEX_ENABLE_FLAGS_5: {
            uint32_t index = (address - REG_HWEX_ENABLE_FLAGS_0) >> 1;
            if (index <= (HWEX_EXTENSIONS_COUNT >> 4)) { // check if the extension exists
                if (index == (HWEX_EXTENSIONS_COUNT >> 4)) {
                    // delete the flags of the non-existant extensions
                    value = value & (0xFFFF >> (16 - (HWEX_EXTENSIONS_COUNT & 0xF)));
                }
                _GBAHardwareExtensionsIOWrite(gba, address, value);
            }
            break;
        }
        // Return codes
        case REG_HWEX0_RET_CODE:
            mLOG(GBA_IO, GAME_ERROR, "Write to read-only hardware extensions I/O register: %06X", address);
		    break;

        // CNT
        case REG_HWEX0_CNT:
            GBAHardwareExtensionsHandleCntWrite(gba, address, value);
            break;

        // Parameters
        case REG_HWEX0_P0_LO:
        case REG_HWEX0_P0_HI:
        case REG_HWEX0_P1_LO:
        case REG_HWEX0_P1_HI:
        case REG_HWEX0_P2_LO:
        case REG_HWEX0_P2_HI:
        case REG_HWEX0_P3_LO:
        case REG_HWEX0_P3_HI:
            if (!(_GBAHardwareExtensionsIORead(gba, REG_HWEX0_CNT) & HWEX_CNT_FLAG_PROCESSING)) {
                _GBAHardwareExtensionsIOWrite(gba, address, value);
            }
            break;
        default:
            mLOG(GBA_IO, GAME_ERROR, "Write non hardware extensions I/O register: %06X", address);
    }
}

void GBAHardwareExtensionsIOWrite8(struct GBA* gba, uint32_t address, uint8_t value) {
    uint32_t address16 = address & 0xFFFFFE;
    uint16_t* reg = GetHwExIOPointer(gba, address16);
    GBAHardwareExtensionsIOWrite(gba, address16, address & 1 ? (value << 8) | (*reg & 0xFF) : (value | (*reg & 0xFF00)));
}

void GBAHardwareExtensionsIOWrite32(struct GBA* gba, uint32_t address, uint32_t value) {
    GBAHardwareExtensionsIOWrite(gba, address, value & 0xFFFF);
    GBAHardwareExtensionsIOWrite(gba, address + 2, value >> 16);
}

