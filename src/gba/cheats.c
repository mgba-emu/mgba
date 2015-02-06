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
	device->cheats = cheats;
	_addBreakpoint(device);
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

void GBACheatRefresh(struct GBACheatDevice* device) {
	bool condition = true;
	int conditionRemaining = 0;

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