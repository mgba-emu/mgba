/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/extra/extensions.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>

#define ENABLE_CODE 0xC0DE
#define ENABLED_VALUE 0x1DEA

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
	HWEX_RET_ERR_DISABLED_BY_USER = 0x106,
	HWEX_RET_ERR_NOT_INITIALIZED = 0x107,
	HWEX_RET_ERR_ALREADY_INITIALIZED = 0x108,
};


#define GBAEX_EXTRA_RAM_MAX_SIZE 0x4000000 // 64 MB

#define SIMPLIFY_HWEX_REG_ADDRESS(address) ((address - REG_HWEX0_ENABLE) >> 1)

const uint16_t extensionIdByRegister[] = {
	// Extra RAM
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_ENABLE)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_CNT)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_RET_CODE)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_UNUSED)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P0_LO)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P0_HI)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P1_LO)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P1_HI)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P2_LO)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P2_HI)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P3_LO)] = GBAEX_ID_EXTRA_RAM,
	[SIMPLIFY_HWEX_REG_ADDRESS(REG_HWEX0_P3_HI)] = GBAEX_ID_EXTRA_RAM,
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
	bool (*onAbort)(struct GBA* gba);
	void (*onDisable)(struct GBAExtensions* extensions);
};

static uint16_t _GBAExtensionExtraRAM(struct GBA* gba);
static void _GBAExtensionExtraRAMDisable(struct GBAExtensions* extensions);

static const struct GBAExtensionHandlers extensionHandlers[] = {
	[GBAEX_ID_EXTRA_RAM] = { .onCall = _GBAExtensionExtraRAM, .onAbort = NULL, .onDisable = _GBAExtensionExtraRAMDisable }
};

void GBAExtensionsInit(struct GBAExtensions* extensions) {
	extensions->globalEnabled = false;
	memset(extensions->extensionsEnabled, 0, sizeof(extensions->extensionsEnabled));

	extensions->userGlobalEnabled = false;
	memset(extensions->userExtensionsEnabled, 0, sizeof(extensions->userExtensionsEnabled));

	extensions->io = NULL;
	
	extensions->extraRam = NULL;
}

static void _GBAExtensionsFreeExtensionsData(struct GBAExtensions* extensions) {
	for (uint32_t i = 0; i < GBAEX_EXTENSIONS_COUNT; i++) {
		if (extensionHandlers[i].onDisable && extensions->extensionsEnabled[i]) {
			extensionHandlers[i].onDisable(extensions);
		}
	}
}

static void _GBAExtensionsFreeIO(struct GBAExtensions* extensions) {
	if (extensions->io) {
		free(extensions->io);
		extensions->io = NULL;
	}
}

void GBAExtensionsReset(struct GBAExtensions* extensions) {
	_GBAExtensionsFreeExtensionsData(extensions);

	extensions->globalEnabled = false;
	memset(extensions->extensionsEnabled, 0, sizeof(extensions->extensionsEnabled));

	_GBAExtensionsFreeIO(extensions);
}

void GBAExtensionsDestroy(struct GBAExtensions* extensions) {
	_GBAExtensionsFreeExtensionsData(extensions);

	extensions->globalEnabled = false;
	memset(extensions->extensionsEnabled, 0, sizeof(extensions->extensionsEnabled));

	_GBAExtensionsFreeIO(extensions);
}

static uint16_t* _getHwExIOPointer(struct GBA* gba, uint32_t address) {
	return gba->extensions.io + (address - REG_HWEX0_ENABLE) / 2;
}

static bool GBAExtensionsIsExtensionEnabled(struct GBA* gba, uint32_t extensionId) {
	return gba->extensions.extensionsEnabled[extensionId] && gba->extensions.userExtensionsEnabled[extensionId];
}

static uint16_t _GBAExtensionsIORead(struct GBA* gba, uint32_t address) {
	switch (address) {
	case REG_HWEX_ENABLE:
		return ENABLED_VALUE;
	case REG_HWEX_VERSION:
		return REG_HWEX_VERSION_VALUE;
		
	default:
		return *_getHwExIOPointer(gba, address);
	}
}

uint16_t GBAExtensionsIORead(struct GBA* gba, uint32_t address) {
	if (gba->extensions.userGlobalEnabled && gba->extensions.globalEnabled) {
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
					handlers->onAbort(gba);
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
	if ((!gba->extensions.userGlobalEnabled || !gba->extensions.globalEnabled) && address != REG_HWEX_ENABLE) {
		mLOG(GBA_IO, GAME_ERROR, "Write to unused I/O register: %03X", address);
		return;
	}
	switch (address) {
	case REG_HWEX_ENABLE: {
		gba->extensions.globalEnabled = value == ENABLE_CODE;
		if (gba->extensions.globalEnabled && !gba->extensions.io) {
			gba->extensions.io = malloc(GBAEX_IO_SIZE);
			memset(gba->extensions.io, 0, GBAEX_IO_SIZE);
		} else if (!gba->extensions.globalEnabled && gba->extensions.io) {
			_GBAExtensionsFreeExtensionsData(&gba->extensions);
			_GBAExtensionsFreeIO(&gba->extensions);
		}
		break;
	}
	// Enable flags
	case REG_HWEX0_ENABLE: {
		uint32_t extensionId = _getExtensionIdFromAddress(address);
		bool enabled = value == ENABLE_CODE;
		bool wasEnabled = gba->extensions.extensionsEnabled[extensionId];
		gba->extensions.extensionsEnabled[extensionId] = enabled;
		_GBAExtensionsIOWrite(gba, address, enabled ? ENABLED_VALUE : 0);
		if (wasEnabled && !enabled && extensionHandlers[extensionId].onDisable) {
			extensionHandlers[extensionId].onDisable(&gba->extensions);
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
	if (!gba->extensions.userGlobalEnabled || !gba->extensions.globalEnabled) {
		mLOG(GBA_IO, GAME_ERROR, "Write to unused I/O register: %03X", address);
		return;
	}
	uint32_t address16 = address & 0xFFFFFE;
	uint16_t* reg = _getHwExIOPointer(gba, address16);
	GBAExtensionsIOWrite(gba, address16, address & 1 ? (value << 8) | (*reg & 0xFF) : (value | (*reg & 0xFF00)));
}

#define IO_BLOCK_HEADER_ID 0xFFFFFFFF

size_t GBAExtensionsSerialize(struct GBA* gba, void** sram) {
	if (!gba->extensions.io) {
		// Disabled
		return 0;
	}
	
	struct GBAExtensionsStateBlockHeader tmpHeaders[GBAEX_EXTENSIONS_COUNT + 1];
	void* blocksDataSrc[GBAEX_EXTENSIONS_COUNT + 1];
	uint32_t blocksCount = 1;
	size_t blocksDataSize = 0;

	// Serialize IO
	tmpHeaders[0].id = IO_BLOCK_HEADER_ID;
	tmpHeaders[0].size = GBAEX_IO_SIZE;
	blocksDataSrc[0] = gba->extensions.io;
	blocksDataSize += GBAEX_IO_SIZE;

	// Serialize extra RAM
	if (gba->extensions.extraRam) {
		tmpHeaders[blocksCount].id = GBAEX_ID_EXTRA_RAM;
		tmpHeaders[blocksCount].size = gba->extensions.extraRamSize;
		blocksDataSrc[blocksCount++] = gba->extensions.extraRam;
		blocksDataSize += gba->extensions.extraRamSize;
	}

	// Alloc state
	uint32_t offset = sizeof(struct GBAExtensionsState) + sizeof(struct GBAExtensionsStateBlockHeader) * (blocksCount - 1);
	size_t stateSize = offset + blocksDataSize;
	struct GBAExtensionsState* state = malloc(stateSize);
	struct GBAExtensionsStateBlockHeader* blockHeaders = &state->ioBlockHeader;

	// Copy data blocks
	for (size_t i = 0; i < blocksCount; i++) {
		blockHeaders[i].id = tmpHeaders[i].id;
		blockHeaders[i].size = tmpHeaders[i].size;
		blockHeaders[i].offset = offset;
		memcpy(((uint8_t*) state) + offset, blocksDataSrc[i], tmpHeaders[i].size);
		offset += tmpHeaders[i].size;
	}

	state->version = REG_HWEX_VERSION_VALUE;
	state->extensionsBlockCount = blocksCount;

	*sram = state;
	return stateSize;
}

bool GBAExtensionsDeserialize(struct GBA* gba, const struct GBAExtensionsState* state, size_t size) {
	_GBAExtensionsFreeExtensionsData(&gba->extensions);
	memset(gba->extensions.extensionsEnabled, 0, sizeof(gba->extensions.extensionsEnabled));

	if (!state || state->extensionsBlockCount == 0 || state->ioBlockHeader.id != IO_BLOCK_HEADER_ID 
		|| state->ioBlockHeader.size == 0) {
		// No data, extensions disabled
		_GBAExtensionsFreeIO(&gba->extensions);
		gba->extensions.globalEnabled = false;	
		return false;
	}

	// Deserialize IO
	if (!gba->extensions.io) {
		gba->extensions.io = malloc(GBAEX_IO_SIZE);
		memset(gba->extensions.io, 0, GBAEX_IO_SIZE);
		gba->extensions.globalEnabled = true;
	}
	uint8_t* blocksData = (uint8_t*) state;
	size_t ioSerializedSize = GBAEX_IO_SIZE;
	if (state->ioBlockHeader.size < GBAEX_IO_SIZE) {
		memset(((uint8_t*) gba->extensions.io) + state->ioBlockHeader.size, 0, GBAEX_IO_SIZE - state->ioBlockHeader.size);
		ioSerializedSize = state->ioBlockHeader.size;
	}
	memcpy(gba->extensions.io, blocksData + state->ioBlockHeader.offset, ioSerializedSize);

	// Deserialize extensions
	const struct GBAExtensionsStateBlockHeader* extBlocksHeaders = (&state->ioBlockHeader) + 1;
	for (size_t i = 0; i < (state->extensionsBlockCount - 1); i++) {
		if (extBlocksHeaders[i].size > 0 && (extBlocksHeaders[i].offset + extBlocksHeaders[i].size) <= size) {
			switch(extBlocksHeaders[i].id) {
			case GBAEX_ID_EXTRA_RAM: {
				size_t extraRAMSize = extBlocksHeaders[i].size <= GBAEX_EXTRA_RAM_MAX_SIZE ? extBlocksHeaders[i].size : GBAEX_EXTRA_RAM_MAX_SIZE;
				gba->extensions.extraRam = malloc(extraRAMSize);
				memcpy(gba->extensions.extraRam, blocksData + extBlocksHeaders[i].offset, extraRAMSize);
				gba->extensions.extraRamSize = extraRAMSize;
				gba->extensions.extraRamRealSize = extraRAMSize;
				gba->extensions.extensionsEnabled[GBAEX_ID_EXTRA_RAM] = true;
				break;
			}
			}
		}
	}

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

enum GBAEX_EXTRA_RAM_COMMANDS {
	GBAEX_EXTRA_RAM_CMD_WRITE = 0,
	GBAEX_EXTRA_RAM_CMD_READ = 1,
	GBAEX_EXTRA_RAM_CMD_SWAP = 2,
	GBAEX_EXTRA_RAM_CMD_INIT = 3,
	GBAEX_EXTRA_RAM_CMD_RESIZE = 4,
	GBAEX_EXTRA_RAM_CMD_DESTROY = 5,
};

static uint16_t _GBAExtensionExtraRAM(struct GBA* gba) {
	uint32_t command = _GBAExtensionsIORead32(gba, REG_HWEX0_P0_LO);
	uint32_t index = _GBAExtensionsIORead32(gba, REG_HWEX0_P1_LO);
	uint32_t dataPointer = _GBAExtensionsIORead32(gba, REG_HWEX0_P2_LO);
	uint32_t dataSize = _GBAExtensionsIORead32(gba, REG_HWEX0_P3_LO);
	void* data;
	bool isRom;
	uint32_t memoryMaxSize;

	// Validate parameters
	switch (command) {
	case GBAEX_EXTRA_RAM_CMD_WRITE:
	case GBAEX_EXTRA_RAM_CMD_READ:
	case GBAEX_EXTRA_RAM_CMD_SWAP:
		if (!gba->extensions.extraRam) {
			return HWEX_RET_ERR_NOT_INITIALIZED;
		}

		// Check if the pointer is valid
		data = GBAGetMemoryPointer(gba, dataPointer, &memoryMaxSize, &isRom);
		if (data == NULL) {
			return HWEX_RET_ERR_BAD_ADDRESS;
		}

		// Check if index and size are valid
		if (index >= gba->extensions.extraRamSize || (index + dataSize) >= gba->extensions.extraRamSize || dataSize >= memoryMaxSize) {
			return HWEX_RET_ERR_INVALID_PARAMETERS;
		}
		break;
	case GBAEX_EXTRA_RAM_CMD_INIT:
		if (gba->extensions.extraRam) {
			return HWEX_RET_ERR_ALREADY_INITIALIZED;
		}
		// Check if size is valid
		if (index == 0 || index > GBAEX_EXTRA_RAM_MAX_SIZE) {
			return HWEX_RET_ERR_INVALID_PARAMETERS;
		}
		break;
	case GBAEX_EXTRA_RAM_CMD_RESIZE:
		if (!gba->extensions.extraRam) {
			return HWEX_RET_ERR_NOT_INITIALIZED;
		}
		// Check if size is valid
		if (index == 0 || index > GBAEX_EXTRA_RAM_MAX_SIZE) {
			return HWEX_RET_ERR_INVALID_PARAMETERS;
		}
		break;
	case GBAEX_EXTRA_RAM_CMD_DESTROY:
		if (!gba->extensions.extraRam) {
			return HWEX_RET_ERR_NOT_INITIALIZED;
		}
		break;
	default:
		// invalid command
		return HWEX_RET_ERR_INVALID_PARAMETERS;
	}
	
	// Execute command
	switch (command) {
	case GBAEX_EXTRA_RAM_CMD_WRITE:
		memcpy(((uint8_t*)gba->extensions.extraRam) + index, data, dataSize);
		break;
	case GBAEX_EXTRA_RAM_CMD_READ:
		if (isRom) {
			return HWEX_RET_ERR_WRITE_TO_ROM;
		}
		memcpy(data, ((uint8_t*)gba->extensions.extraRam) + index, dataSize);
		break;
	case GBAEX_EXTRA_RAM_CMD_SWAP:
		if (isRom) {
			return HWEX_RET_ERR_WRITE_TO_ROM;
		}
		// TODO: make this more efficient
		uint8_t* data1 = data;
		uint8_t* data2 = (uint8_t*) gba->extensions.extraRam;
		data2 += index;
		for (uint32_t i = 0; i < dataSize; i++) {
			uint8_t aux = data1[i];
			data1[i] = data2[i];
			data2[i] = aux;
		}
		break;
	case GBAEX_EXTRA_RAM_CMD_INIT:
		gba->extensions.extraRam = malloc(index);
		gba->extensions.extraRamSize = index;
		gba->extensions.extraRamRealSize = index;
		break;
	case GBAEX_EXTRA_RAM_CMD_RESIZE:
		if (index > gba->extensions.extraRamRealSize) {
			uint8_t* newExtraRam = malloc(index);
			memcpy(newExtraRam, gba->extensions.extraRam, gba->extensions.extraRamSize);
			free(gba->extensions.extraRam);
			gba->extensions.extraRam = newExtraRam;
			gba->extensions.extraRamRealSize = index;
		}
		gba->extensions.extraRamSize = index;
		break;
	case GBAEX_EXTRA_RAM_CMD_DESTROY:
		free(gba->extensions.extraRam);
		break;
	}

	return HWEX_RET_OK;
}

static void _GBAExtensionExtraRAMDisable(struct GBAExtensions* extensions) {
	if (extensions->extraRam) {
		free(extensions->extraRam);
		extensions->extraRam = NULL;
	}
}