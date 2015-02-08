/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cheats.h"

#include "gba/gba.h"
#include "gba/io.h"

const uint32_t GBA_CHEAT_DEVICE_ID = 0xABADC0DE;

DEFINE_VECTOR(GBACheatList, struct GBACheat);

static const uint32_t _gsa1S[4] = { 0x09F4FBBD, 0x9681884A, 0x352027E9, 0xF3DEE5A7 };
static const uint32_t _gsa3S[4] = { 0x7AA9648F, 0x7FAE6994, 0xC0EFAAD5, 0x42712C57 };

static int32_t _readMem(struct ARMCore* cpu, uint32_t address, int width) {
	switch (width) {
	case 1:
		return cpu->memory.load8(cpu, address, 0);
	case 2:
		return cpu->memory.load16(cpu, address, 0);
	case 4:
		return cpu->memory.load32(cpu, address, 0);
	}
	return 0;
}

static void _writeMem(struct ARMCore* cpu, uint32_t address, int width, int32_t value) {
	switch (width) {
	case 1:
		cpu->memory.store8(cpu, address, value, 0);
		break;
	case 2:
		cpu->memory.store16(cpu, address, value, 0);
		break;
	case 4:
		cpu->memory.store32(cpu, address, value, 0);
		break;
	}
}

static int _hexDigit(char digit) {
	switch (digit) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return digit - '0';

	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		return digit - 'a' + 10;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		return digit - 'A' + 10;

	default:
		return -1;
	}
}

static const char* _hex32(const char* line, uint32_t* out) {
	uint32_t value = 0;
	int i;
	for (i = 0; i < 8; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = _hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

static const char* _hex16(const char* line, uint16_t* out) {
	uint16_t value = 0;
	int i;
	for (i = 0; i < 4; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = _hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

// http://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
static void _decryptGameShark(uint32_t* op1, uint32_t* op2, const uint32_t* seeds) {
	uint32_t sum = 0xC6EF3720;
	int i;
	for (i = 0; i < 32; ++i) {
		*op2 -= ((*op1 << 4) + seeds[2]) ^ (*op1 + sum) ^ ((*op1 >> 5) + seeds[3]);
		*op1 -= ((*op2 << 4) + seeds[0]) ^ (*op2 + sum) ^ ((*op2 >> 5) + seeds[1]);
		sum -= 0x9E3779B9;
	}
}

static bool _addGSA1(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	enum GBAGameSharkType type = op1 >> 28;
	struct GBACheat* cheat = 0;

	if (cheats->incompleteCheat) {
		if (cheats->remainingAddresses > 0) {
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_ASSIGN;
			cheat->width = 4;
			cheat->address = op1;
			cheat->operand = cheats->incompleteCheat->operand;
			cheat->repeat = 1;
			--cheats->remainingAddresses;
		}
		if (cheats->remainingAddresses > 0) {
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_ASSIGN;
			cheat->width = 4;
			cheat->address = op2;
			cheat->operand = cheats->incompleteCheat->operand;
			cheat->repeat = 1;
			--cheats->remainingAddresses;
		}
		if (cheats->remainingAddresses == 0) {
			cheats->incompleteCheat = 0;
		}
		return true;
	}

	switch (type) {
	case GSA_ASSIGN_1:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_4:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 4;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_LIST:
		cheats->remainingAddresses = (op1 & 0xFFFF) - 1;
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 4;
		cheat->address = op2;
		cheats->incompleteCheat = cheat;
		break;
	case GSA_PATCH:
		if (cheats->nRomPatches >= MAX_ROM_PATCHES) {
			return false;
		}
		cheats->romPatches[cheats->nRomPatches].address = (op1 & 0xFFFFFF) << 1;
		cheats->romPatches[cheats->nRomPatches].newValue = op2;
		cheats->romPatches[cheats->nRomPatches].applied = false;
		++cheats->nRomPatches;
		return true;
	case GSA_BUTTON:
		// TODO: Implement button
		return false;
	case GSA_IF_EQ:
		if (op1 == 0xDEADFACE) {
			// TODO: Change seeds
			return false;
		}
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_IF_EQ_RANGE:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		cheat->address = op2 & 0x0FFFFFFF;
		cheat->operand = op1 & 0xFFFF;
		cheat->repeat = (op1 >> 16) & 0xFF;
		return true;
	case GSA_HOOK:
		if (cheats->hookAddress) {
			return false;
		}
		cheats->hookAddress = BASE_CART0 | (op1 & (SIZE_CART0 - 1));
		cheats->hookMode = MODE_THUMB;
		return true;
	default:
		return false;
	}
	cheat->operand = op2;
	cheat->repeat = 1;
	return false;
}

static bool _addGSA3(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	// TODO
	UNUSED(cheats);
	UNUSED(op1);
	UNUSED(op2);
	return false;
}

static void _addBreakpoint(struct GBACheatDevice* device) {
	if (!device->cheats || !device->p) {
		return;
	}
	GBASetBreakpoint(device->p, &device->d, device->cheats->hookAddress, device->cheats->hookMode, &device->cheats->patchedOpcode);
}

static void _removeBreakpoint(struct GBACheatDevice* device) {
	if (!device->cheats || !device->p) {
		return;
	}
	GBAClearBreakpoint(device->p, device->cheats->hookAddress, device->cheats->hookMode, device->cheats->patchedOpcode);
}

static void _patchROM(struct GBACheatDevice* device) {
	if (!device->cheats || !device->p) {
		return;
	}
	int i;
	for (i = 0; i < device->cheats->nRomPatches; ++i) {
		if (device->cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, device->cheats->romPatches[i].address, device->cheats->romPatches[i].newValue, &device->cheats->romPatches[i].oldValue);
		device->cheats->romPatches[i].applied = true;
	}
}

static void _unpatchROM(struct GBACheatDevice* device) {
	if (!device->cheats || !device->p) {
		return;
	}
	int i;
	for (i = 0; i < device->cheats->nRomPatches; ++i) {
		if (!device->cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, device->cheats->romPatches[i].address, device->cheats->romPatches[i].oldValue, 0);
		device->cheats->romPatches[i].applied = false;
	}
}

static void GBACheatDeviceInit(struct ARMCore*, struct ARMComponent*);
static void GBACheatDeviceDeinit(struct ARMComponent*);

void GBACheatDeviceCreate(struct GBACheatDevice* device) {
	device->d.id = GBA_CHEAT_DEVICE_ID;
	device->d.init = GBACheatDeviceInit;
	device->d.deinit = GBACheatDeviceDeinit;
}

void GBACheatSetInit(struct GBACheatSet* set) {
	set->hookAddress = 0;
	set->hookMode = MODE_THUMB;
	GBACheatListInit(&set->list, 4);
	set->incompleteCheat = 0;
	set->patchedOpcode = 0;
	set->gsaVersion = 0;
	set->remainingAddresses = 0;
}

void GBACheatSetDeinit(struct GBACheatSet* set) {
	GBACheatListDeinit(&set->list);
}

void GBACheatAttachDevice(struct GBA* gba, struct GBACheatDevice* device) {
	if (gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE]) {
		ARMHotplugDetach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
	}
	gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE] = &device->d;
	ARMHotplugAttach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
}

void GBACheatInstallSet(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	_removeBreakpoint(device);
	_unpatchROM(device);
	device->cheats = cheats;
	_addBreakpoint(device);
	_patchROM(device);
}

bool GBACheatAddCodeBreaker(struct GBACheatSet* cheats, uint32_t op1, uint16_t op2) {
	enum GBACodeBreakerType type = op1 >> 28;
	struct GBACheat* cheat = 0;

	if (cheats->incompleteCheat) {
		cheats->incompleteCheat->repeat = op1 & 0xFFFF;
		cheats->incompleteCheat->addressOffset = op2;
		cheats->incompleteCheat->operandOffset = 0;
		cheats->incompleteCheat = 0;
		return true;
	}

	switch (type) {
	case CB_GAME_ID:
		// TODO: Run checksum
		return true;
	case CB_HOOK:
		if (cheats->hookAddress) {
			return false;
		}
		cheats->hookAddress = BASE_CART0 | (op1 & (SIZE_CART0 - 1));
		cheats->hookMode = MODE_THUMB;
		return true;
	case CB_OR_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_OR;
		cheat->width = 2;
		break;
	case CB_ASSIGN_1:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		break;
	case CB_FILL:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheats->incompleteCheat = cheat;
		break;
	case CB_FILL_8:
		GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker code %08X %04X not supported", op1, op2);
		return false;
	case CB_AND_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_AND;
		cheat->width = 2;
		break;
	case CB_IF_EQ:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		break;
	case CB_ASSIGN_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		break;
	case CB_ENCRYPT:
		GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker encryption not supported");
		return false;
	case CB_IF_NE:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_NE;
		cheat->width = 2;
		break;
	case CB_IF_GT:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_GT;
		cheat->width = 2;
		break;
	case CB_IF_LT:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_LT;
		cheat->width = 2;
		break;
	case CB_IF_SPECIAL:
		switch (op1 & 0x0FFFFFFF) {
		case 0x20:
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_IF_AND;
			cheat->width = 2;
			cheat->address = BASE_IO | REG_JOYSTAT;
			cheat->operand = op2;
			cheat->repeat = 1;
			return true;
		default:
			GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker code %08X %04X not supported", op1, op2);
			return false;
		}
	case CB_ADD_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ADD;
		cheat->width = 2;
		break;
	case CB_IF_AND:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_AND;
		cheat->width = 2;
		break;
	}

	cheat->address = op1 & 0x0FFFFFFF;
	cheat->operand = op2;
	cheat->repeat = 1;
	return true;
}

bool GBACheatAddCodeBreakerLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = _hex16(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddCodeBreaker(cheats, op1, op2);
}

bool GBACheatAddGameShark(struct GBACheatSet* set, uint32_t op1, uint32_t op2) {
	uint32_t o1 = op1;
	uint32_t o2 = op2;
	switch (set->gsaVersion) {
	case 0:
		// Try to detect GameShark version
		_decryptGameShark(&o1, &o2, _gsa1S);
		if ((o1 & 0xF0000000) == 0xF0000000 && !(o2 & 0xFFFFFCFE)) {
			set->gsaVersion = 1;
			memcpy(set->gsaSeeds, _gsa1S, 4 * sizeof(uint32_t));
			return _addGSA1(set, o1, o2);
		}
		o1 = op1;
		o2 = op2;
		_decryptGameShark(&o1, &o2, _gsa3S);
		if ((o1 & 0xFE000000) == 0xC4000000 && !(o2 & 0xFFFF0000)) {
			set->gsaVersion = 3;
			memcpy(set->gsaSeeds, _gsa3S, 4 * sizeof(uint32_t));
			return _addGSA3(set, o1, o2);
		}
		break;
	case 1:
		_decryptGameShark(&o1, &o2, set->gsaSeeds);
		return _addGSA1(set, o1, o2);
	case 3:
		_decryptGameShark(&o1, &o2, set->gsaSeeds);
		return _addGSA3(set, o1, o2);
	}
	return false;
}

bool GBACheatAddGameSharkLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint32_t op2;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = _hex32(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddGameShark(cheats, op1, op2);
}

bool GBACheatAddLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	uint16_t op3;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (isspace(line[0])) {
		++line;
	}
	line = _hex16(line, &op2);
	if (!line) {
		return false;
	}
	if (isspace(line[0])) {
		return GBACheatAddCodeBreaker(cheats, op1, op2);
	}
	line = _hex16(line, &op3);
	if (!line) {
		return false;
	}
	uint32_t realOp2 = op2;
	realOp2 <<= 16;
	realOp2 |= op3;
	return GBACheatAddGameShark(cheats, op1, realOp2);
}

void GBACheatRefresh(struct GBACheatDevice* device) {
	bool condition = true;
	int conditionRemaining = 0;
	_patchROM(device);

	size_t nCodes = GBACheatListSize(&device->cheats->list);
	size_t i;
	for (i = 0; i < nCodes; ++i) {
		if (conditionRemaining > 0) {
			--conditionRemaining;
			if (!condition) {
				continue;
			}
		} else {
			condition = true;
		}
		struct GBACheat* cheat = GBACheatListGetPointer(&device->cheats->list, i);
		int32_t value = 0;
		int32_t operand = cheat->operand;
		uint32_t operationsRemaining = cheat->repeat;
		uint32_t address = cheat->address;
		bool performAssignment = false;
		for (; operationsRemaining; --operationsRemaining) {
			switch (cheat->type) {
			case CHEAT_ASSIGN:
				value = operand;
				performAssignment = true;
				break;
			case CHEAT_AND:
				value = _readMem(device->p->cpu, address, cheat->width) & operand;
				performAssignment = true;
				break;
			case CHEAT_ADD:
				value = _readMem(device->p->cpu, address, cheat->width) + operand;
				performAssignment = true;
				break;
			case CHEAT_OR:
				value = _readMem(device->p->cpu, address, cheat->width) | operand;
				performAssignment = true;
				break;
			case CHEAT_IF_EQ:
				condition = _readMem(device->p->cpu, address, cheat->width) == operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_NE:
				condition = _readMem(device->p->cpu, address, cheat->width) != operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_LT:
				condition = _readMem(device->p->cpu, address, cheat->width) < operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_GT:
				condition = _readMem(device->p->cpu, address, cheat->width) > operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_AND:
				condition = _readMem(device->p->cpu, address, cheat->width) & operand;
				conditionRemaining = cheat->repeat;
				break;
			}

			if (performAssignment) {
				_writeMem(device->p->cpu, address, cheat->width, value);
			}

			address += cheat->addressOffset;
			operand += cheat->operandOffset;
		}
	}
}

void GBACheatDeviceInit(struct ARMCore* cpu, struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	device->p = (struct GBA*) cpu->master;
	_addBreakpoint(device);
}

void GBACheatDeviceDeinit(struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	_removeBreakpoint(device);
}
