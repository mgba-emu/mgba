/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/extra/extensions.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>


enum HWEX_RETURN_CODES {
	HWEX_RET_OK = 0,
	HWEX_RET_WAIT = 1,

	// Errors
	HWEX_RET_ERR_UNKNOWN = 0x100,
	HWEX_RET_ERR_BAD_ADDRESS = 0x101,
	HWEX_RET_ERR_INVALID_PARAMETERS = 0x102,
	HWEX_RET_ERR_WRITE_TO_ROM = 0x103,
	HWEX_RET_ERR_ABORTED = 0x104,
	HWEX_RET_ERR_DISABLED = 0x105,
	HWEX_RET_ERR_DISABLED_BY_USER = 0x106
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

static uint16_t _getExtensionIdFromAddress(uint32_t address) {
	return extensionIdByRegister[SIMPLIFY_HWEX_REG_ADDRESS(address)];
}

#undef SIMPLIFY_HWEX_REG_ADDRESS

// CNT flags
// Writable
#define HWEX_CNT_FLAG_CALL 1
#define HWEX_CNT_ALL_WRITABLE (HWEX_CNT_FLAG_CALL)
// Read only
#define HWEX_CNT_FLAG_PROCESSING (1 << 15)

struct GBAExtensionHandlers {
	uint16_t (*onCall)(struct GBA* gba);
	bool (*onAbort)(void);
};

static uint16_t _GBAExtensionMoreRAM(struct GBA* gba);

static const struct GBAExtensionHandlers extensionHandlers[] = {
	[HWEX_ID_MORE_RAM] = { .onCall = _GBAExtensionMoreRAM, .onAbort = NULL }
};

void GBAExtensionsInit(struct GBAExtensions* hwExtensions) {
	hwExtensions->enabled = false;

	hwExtensions->userEnabled = false;
	memset(hwExtensions->userEnabledFlags, 0, sizeof(hwExtensions->userEnabledFlags));

	memset(hwExtensions->io, 0, sizeof(hwExtensions->io));
}

static uint16_t* _getHwExIOPointer(struct GBA* gba, uint32_t address) {
	return gba->extensions.io + ((address - REG_HWEX_ENABLE_FLAGS_0) >> 1);
}

static uint16_t _GBAExtensionsIORead(struct GBA* gba, uint32_t address) {
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
		return *_getHwExIOPointer(gba, address) & gba->extensions.userEnabledFlags[index];
	}
		
	default:
		return *_getHwExIOPointer(gba, address);
	}
}

uint16_t GBAExtensionsIORead(struct GBA* gba, uint32_t address) {
	if (gba->extensions.enabled && gba->extensions.userEnabled) {
		return _GBAExtensionsIORead(gba, address);
	}
	mLOG(GBA_IO, GAME_ERROR, "Read from unused I/O register: %03X", address);
	return GBALoadBad(gba->cpu);
}

static uint32_t _GBAExtensionsIORead32(struct GBA* gba, uint32_t address) {
	return (_GBAExtensionsIORead(gba, address + 2) << 16) | _GBAExtensionsIORead(gba, address);
};

static void _GBAExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	*_getHwExIOPointer(gba, address) = value;
}

static bool GBAExtensionsIsExtensionEnabled(struct GBA* gba, uint32_t extensionId) {
	uint32_t index = extensionId / 16;
	uint32_t bit = extensionId % 16;
	return (_GBAExtensionsIORead(gba, REG_HWEX_ENABLE_FLAGS_0 + index * 2) & (1 << bit)) != 0;
}

static void GBAExtensionsHandleCntWrite(struct GBA* gba, uint32_t cntAddress, uint32_t value) {
	uint16_t* cnt = _getHwExIOPointer(gba, cntAddress);
	uint16_t* returnCode = cnt + 1;
	uint16_t currentValue = *cnt;
	uint32_t extensionId = _getExtensionIdFromAddress(cntAddress);

	if (!GBAExtensionsIsExtensionEnabled(gba, extensionId)) {
		*returnCode = HWEX_RET_ERR_DISABLED;
		return;
	}

	const struct GBAExtensionHandlers* handlers = extensionHandlers + extensionId;
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
}

void GBAExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	case REG_HWEX_ENABLE:
		gba->extensions.enabled = value == 0xC0DE;
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
			_GBAExtensionsIOWrite(gba, address, value);
		}
		break;
	}
	// Return codes
	case REG_HWEX0_RET_CODE:
		mLOG(GBA_IO, GAME_ERROR, "Write to read-only hardware extensions I/O register: %06X", address);
		break;

	// CNT
	case REG_HWEX0_CNT:
		GBAExtensionsHandleCntWrite(gba, address, value);
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
		if (!(_GBAExtensionsIORead(gba, REG_HWEX0_CNT) & HWEX_CNT_FLAG_PROCESSING)) {
			_GBAExtensionsIOWrite(gba, address, value);
		}
		break;
	default:
		mLOG(GBA_IO, GAME_ERROR, "Write non hardware extensions I/O register: %06X", address);
	}
}

void GBAExtensionsIOWrite8(struct GBA* gba, uint32_t address, uint8_t value) {
	uint32_t address16 = address & 0xFFFFFE;
	uint16_t* reg = _getHwExIOPointer(gba, address16);
	GBAExtensionsIOWrite(gba, address16, address & 1 ? (value << 8) | (*reg & 0xFF) : (value | (*reg & 0xFF00)));
}

bool GBAExtensionsSerialize(struct GBA* gba, struct GBAExtensionsState* state) {
	state->enabled = gba->extensions.enabled;
	state->version = REG_HWEX_VERSION_VALUE;
	memcpy(state->memory, gba->extensions.io, sizeof(gba->extensions.io));
	memcpy(state->moreRam, gba->extensions.moreRam, sizeof(gba->extensions.moreRam));
	return true;
}

bool GBAExtensionsDeserialize(struct GBA* gba, const struct GBAExtensionsState* state, size_t size) {
	if (size < sizeof(*state)) {
		return false;
	}
	gba->extensions.enabled = state->enabled;
	memcpy(gba->extensions.io, state->memory, sizeof(gba->extensions.io));
	memcpy(gba->extensions.moreRam, state->moreRam, sizeof(gba->extensions.moreRam));

	return true;
}

// Extension handlers
static void* GBAGetMemoryPointer(struct GBA* gba, uint32_t address, uint32_t* memoryMaxSize, bool* isRom) {
	uint8_t* pointer = NULL;
	*isRom = false;

	switch (address >> 24) {
	case REGION_WORKING_RAM:
		if ((address & 0xFFFFFF) < SIZE_WORKING_RAM) {
			pointer = (uint8_t*) gba->memory.wram;
			pointer += address & 0xFFFFFF;
			*memoryMaxSize = SIZE_WORKING_RAM - (address & 0xFFFFFF);
		}
		break;
	case REGION_WORKING_IRAM:
		if ((address & 0xFFFFFF) < SIZE_WORKING_IRAM) {
			pointer = (uint8_t*) gba->memory.iwram;
			pointer += address & 0xFFFFFF;
			*memoryMaxSize = SIZE_WORKING_IRAM - (address & 0xFFFFFF);
		}
		break;
	case REGION_CART0:
	case REGION_CART0_EX:
		if ((address & 0x1FFFFFF) < gba->memory.romSize) {
			*isRom = true;
			pointer = (uint8_t*) gba->memory.rom;
			pointer += address & 0x1FFFFFF;
			*memoryMaxSize = gba->memory.romSize - (address & 0x1FFFFFF);
		}
	}

	return pointer;
}

enum GBAEX_MORE_RAM_COMMANDS {
	GBAEX_MORE_RAM_CMD_WRITE = 0,
	GBAEX_MORE_RAM_CMD_READ = 1,
	GBAEX_MORE_RAM_CMD_SWAP = 2
};

static uint16_t _GBAExtensionMoreRAM(struct GBA* gba) {
	uint32_t command = _GBAExtensionsIORead32(gba, REG_HWEX0_P0_LO);
	uint32_t index = _GBAExtensionsIORead32(gba, REG_HWEX0_P1_LO);
	uint32_t dataPointer = _GBAExtensionsIORead32(gba, REG_HWEX0_P2_LO);
	uint32_t dataSize = _GBAExtensionsIORead32(gba, REG_HWEX0_P3_LO);
	void* data;
	// Check if the pointer is valid
	bool isRom;
	uint32_t memoryMaxSize;
	data = GBAGetMemoryPointer(gba, dataPointer, &memoryMaxSize, &isRom);
	if (data == NULL) {
		return HWEX_RET_ERR_BAD_ADDRESS;
	}

	// Check if index and size are valid
	if (index >= HWEX_MORE_RAM_SIZE || (index + dataSize) >= HWEX_MORE_RAM_SIZE || dataSize >= memoryMaxSize) {
		return HWEX_RET_ERR_INVALID_PARAMETERS;
	}
	
	switch (command) {
	case GBAEX_MORE_RAM_CMD_WRITE:
		memcpy(((uint8_t*)gba->extensions.moreRam) + index, data, dataSize);
		break;
	case GBAEX_MORE_RAM_CMD_READ:
		if (isRom) {
			return HWEX_RET_ERR_WRITE_TO_ROM;
		}
		memcpy(data, ((uint8_t*)gba->extensions.moreRam) + index, dataSize);
		break;
	case GBAEX_MORE_RAM_CMD_SWAP:
		if (isRom) {
			return HWEX_RET_ERR_WRITE_TO_ROM;
		}
		// TODO: make this more efficient
		uint8_t* data1 = data;
		uint8_t* data2 = (uint8_t*) gba->extensions.moreRam;
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
