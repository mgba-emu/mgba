
#include <mgba/internal/gba/hardware-extensions.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>

static enum HWEX_RETURN_CODES {
    HWEX_RET_OK = 0,
    HWEX_RET_WAIT = 1,

    // Errors
    HWEX_RET_ERR_UNKNOWN = 0x100,
    HWEX_RET_ERR_BAD_ADDRESS = 0x101,
    HWEX_RET_ERR_INVALID_PARAMETERS = 0x102,
    HWEX_RET_ERR_WRITE_TO_ROM = 0x103,
};

static bool GetMemoryDataFromAddress(struct GBA* gba, uint32_t address, uint32_t** memory, uint32_t* index, uint32_t* memoryLength, bool* isRom) {
    uint addressPrefix = (address >> 24) & 0xF;
    *memory = NULL;
    *isRom = false;

    if (address & 0b11) {
        return false;
    }
    *index = (address & 0xFFFFFF) >> 2;
    if (addressPrefix == 2) {
        *memory = gba->memory.wram;
        *memoryLength = SIZE_WORKING_RAM;
    } else if (addressPrefix == 3) {
        *memory = gba->memory.iwram;
        *memoryLength = SIZE_WORKING_IRAM;
    } else if (addressPrefix == 8) {
        *isRom = true;
        *memory = gba->memory.rom;
        *memoryLength = gba->memory.romSize;
    } else if (addressPrefix == 9) {
        *isRom = true;
        *memory = gba->memory.rom;
        *index += 0x1000000 >> 2;
        *memoryLength = gba->memory.romSize;
    } else {
        return false;
    }

    if (*isRom && *index > gba->memory.romSize) {
        return false;
    }


    return true;
}

static enum HwExtMoreRAMActions {
    HwExtMoreRAMMemoryWrite = 0,
    HwExtMoreRAMMemoryRead = 1,
    HwExtMoreRAMMemorySwap = 2,
};

static uint16_t MGBAHwExtMoreRAM(struct GBA* gba, uint32_t* memory, uint32_t index, uint32_t memoryLength, bool isRom) {

    uint32_t action = memory[index] >> 28;
    uint32_t dataLength = memory[index] & 0xFFFFFFF;
    uint32_t moreRamIndex = memory[index + 1];
    mLOG(GBA_IO, GAME_ERROR, "Index: %03X", moreRamIndex);
    mLOG(GBA_IO, GAME_ERROR, "Length: %03X", dataLength);

    if (action <= HwExtMoreRAMMemorySwap) {
        if (action == HwExtMoreRAMMemoryWrite) {
            memcpy(gba->extMoreRam + moreRamIndex, memory + 2 + index, sizeof(uint32_t) * dataLength);
        } else if (isRom) {
            return HWEX_RET_ERR_WRITE_TO_ROM;
        } else if (action == HwExtMoreRAMMemoryRead) {
            memcpy(memory + 2 + index, gba->extMoreRam + moreRamIndex, sizeof(uint32_t) * dataLength);
        } else if (action == HwExtMoreRAMMemorySwap) {
            uint32_t aux;
            for (uint32_t i = 0; i < dataLength; i++) {
                aux = memory[i + index + 2];
                memory[i + index + 2] = gba->extMoreRam[i + moreRamIndex];
                gba->extMoreRam[i + moreRamIndex] = aux;
            }
        }
        return HWEX_RET_OK;
    } 

    return HWEX_RET_ERR_INVALID_PARAMETERS;
}

typedef uint16_t (*HwExtensionHandler)(struct GBA* gba, uint32_t* memory, uint32_t index, uint32_t memoryLength, bool isRom);

static const HwExtensionHandler HwExtensionHandlers[HWEX_EXTENSIONS_COUNT] = {
    MGBAHwExtMoreRAM
};

uint16_t GetRegMgbaHwExtensionsEnabled(void) {
    return 0xEC57;
}

uint16_t GetRegMgbaHwExtensionsCnt(struct GBA* gba, uint32_t address) {
    return 1;
}

uint16_t GetRegMgbaHwExtensionValue(struct GBA* gba, uint32_t address) {
    uint32_t index = (address - REG_MGBA_EXTENSION_0) >> 2;
    uint32_t value = gba->extIORegisters[index];
    return (uint16_t) (address & 0b10 ? value >> 16 : value & 0xFFFF);
}

void SetRegMgbaHwExtensionValue(struct GBA* gba, uint32_t address, uint32_t value) {
    if (value & 0x80000000) {
        uint32_t extension = (address - REG_MGBA_EXTENSION_0) >> 2;
        if (extension < HWEX_EXTENSIONS_COUNT) {
            uint32_t* memory;
            uint32_t index, memoryLenght;
            bool isRom;

            if (GetMemoryDataFromAddress(gba, value & 0x0FFFFFFF, &memory, &index, &memoryLenght, &isRom)) {
                gba->extIORegisters[extension] = HwExtensionHandlers[extension](gba, memory, index, memoryLenght, isRom);
            } else {
                gba->extIORegisters[extension] = HWEX_RET_ERR_BAD_ADDRESS;
            }
        }
    }
}

