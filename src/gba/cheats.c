/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cheats.h"

#include "gba/cheats/gameshark.h"
#include "gba/cheats/parv3.h"
#include "gba/gba.h"
#include "util/string.h"
#include "util/vfs.h"

#define MAX_LINE_LENGTH 128

const uint32_t GBA_CHEAT_DEVICE_ID = 0xABADC0DE;

DEFINE_VECTOR(GBACheatList, struct GBACheat);
DEFINE_VECTOR(GBACheatSets, struct GBACheatSet*);
DEFINE_VECTOR(StringList, char*);

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

void GBACheatRegisterLine(struct GBACheatSet* cheats, const char* line) {
	*StringListAppend(&cheats->lines) = strdup(line);
}

static void _addBreakpoint(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p || !cheats->hook) {
		return;
	}
	++cheats->hook->reentries;
	if (cheats->hook->reentries > 1) {
		return;
	}
	GBASetBreakpoint(device->p, &device->d, cheats->hook->address, cheats->hook->mode, &cheats->hook->patchedOpcode);
}

static void _removeBreakpoint(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p || !cheats->hook) {
		return;
	}
	--cheats->hook->reentries;
	if (cheats->hook->reentries > 0) {
		return;
	}
	GBAClearBreakpoint(device->p, cheats->hook->address, cheats->hook->mode, cheats->hook->patchedOpcode);
}

static void _patchROM(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p) {
		return;
	}
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		if (!cheats->romPatches[i].exists || cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, cheats->romPatches[i].address, cheats->romPatches[i].newValue, &cheats->romPatches[i].oldValue);
		cheats->romPatches[i].applied = true;
	}
}

static void _unpatchROM(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p) {
		return;
	}
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		if (!cheats->romPatches[i].exists || !cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, cheats->romPatches[i].address, cheats->romPatches[i].oldValue, 0);
		cheats->romPatches[i].applied = false;
	}
}

static void GBACheatDeviceInit(struct ARMCore*, struct ARMComponent*);
static void GBACheatDeviceDeinit(struct ARMComponent*);

void GBACheatDeviceCreate(struct GBACheatDevice* device) {
	device->d.id = GBA_CHEAT_DEVICE_ID;
	device->d.init = GBACheatDeviceInit;
	device->d.deinit = GBACheatDeviceDeinit;
	GBACheatSetsInit(&device->cheats, 4);
}

void GBACheatDeviceDestroy(struct GBACheatDevice* device) {
	GBACheatDeviceClear(device);
	GBACheatSetsDeinit(&device->cheats);
}

void GBACheatDeviceClear(struct GBACheatDevice* device) {
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* set = *GBACheatSetsGetPointer(&device->cheats, i);
		GBACheatSetDeinit(set);
		free(set);
	}
	GBACheatSetsClear(&device->cheats);
}

void GBACheatSetInit(struct GBACheatSet* set, const char* name) {
	GBACheatListInit(&set->list, 4);
	StringListInit(&set->lines, 4);
	set->incompleteCheat = 0;
	set->incompletePatch = 0;
	set->currentBlock = 0;
	set->gsaVersion = 0;
	set->remainingAddresses = 0;
	set->hook = 0;
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		set->romPatches[i].exists = false;
	}
	if (name) {
		set->name = strdup(name);
	} else {
		set->name = 0;
	}
	set->enabled = true;
}

void GBACheatSetDeinit(struct GBACheatSet* set) {
	GBACheatListDeinit(&set->list);
	size_t i;
	for (i = 0; i < StringListSize(&set->lines); ++i) {
		free(*StringListGetPointer(&set->lines, i));
	}
	if (set->name) {
		free(set->name);
	}
	if (set->hook) {
		--set->hook->refs;
		if (set->hook->refs == 0) {
			free(set->hook);
		}
	}
}

void GBACheatAttachDevice(struct GBA* gba, struct GBACheatDevice* device) {
	if (gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE]) {
		ARMHotplugDetach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
	}
	gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE] = &device->d;
	ARMHotplugAttach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
}

void GBACheatAddSet(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	*GBACheatSetsAppend(&device->cheats) = cheats;
	_addBreakpoint(device, cheats);
	_patchROM(device, cheats);
}

void GBACheatRemoveSet(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		if (*GBACheatSetsGetPointer(&device->cheats, i) == cheats) {
			break;
		}
	}
	if (i == GBACheatSetsSize(&device->cheats)) {
		return;
	}
	GBACheatSetsShift(&device->cheats, i, 1);
	_unpatchROM(device, cheats);
	_removeBreakpoint(device, cheats);
}

bool GBACheatAddAutodetect(struct GBACheatSet* set, uint32_t op1, uint32_t op2) {
	uint32_t o1 = op1;
	uint32_t o2 = op2;
	char line[18] = "XXXXXXXX XXXXXXXX";
	snprintf(line, sizeof(line), "%08X %08X", op1, op2);
	GBACheatRegisterLine(set, line);

	switch (set->gsaVersion) {
	case 0:
		// Try to detect GameShark version
		GBACheatDecryptGameShark(&o1, &o2, GBACheatGameSharkSeeds);
		if ((o1 & 0xF0000000) == 0xF0000000 && !(o2 & 0xFFFFFCFE)) {
			GBACheatSetGameSharkVersion(set, 1);
			return GBACheatAddGameSharkRaw(set, o1, o2);
		}
		o1 = op1;
		o2 = op2;
		GBACheatDecryptGameShark(&o1, &o2, GBACheatProActionReplaySeeds);
		if ((o1 & 0xFE000000) == 0xC4000000 && !(o2 & 0xFFFF0000)) {
			GBACheatSetGameSharkVersion(set, 3);
			return GBACheatAddProActionReplayRaw(set, o1, o2);
		}
		break;
	case 1:
		GBACheatDecryptGameShark(&o1, &o2, set->gsaSeeds);
		return GBACheatAddGameSharkRaw(set, o1, o2);
	case 3:
		GBACheatDecryptGameShark(&o1, &o2, set->gsaSeeds);
		return GBACheatAddProActionReplayRaw(set, o1, o2);
	}
	return false;
}

bool GBACheatAutodetectLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint32_t op2;
	line = hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = hex32(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddAutodetect(cheats, op1, op2);
}

bool GBACheatParseFile(struct GBACheatDevice* device, struct VFile* vf) {
	char cheat[MAX_LINE_LENGTH];
	struct GBACheatSet* set = 0;
	struct GBACheatSet* newSet;
	int gsaVersion = 0;
	bool nextDisabled = false;
	bool reset = false;
	while (true) {
		size_t i = 0;
		ssize_t bytesRead = vf->readline(vf, cheat, sizeof(cheat));
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < 0) {
			return false;
		}
		while (isspace((int) cheat[i])) {
			++i;
		}
		switch (cheat[i]) {
		case '#':
			do {
				++i;
			} while (isspace((int) cheat[i]));
			newSet = malloc(sizeof(*set));
			GBACheatSetInit(newSet, &cheat[i]);
			newSet->enabled = !nextDisabled;
			nextDisabled = false;
			if (set) {
				GBACheatAddSet(device, set);
			}
			if (set && !reset) {
				GBACheatSetCopyProperties(newSet, set);
			} else {
				GBACheatSetGameSharkVersion(newSet, gsaVersion);
			}
			reset = false;
			set = newSet;
			break;
		case '!':
			do {
				++i;
			} while (isspace((int) cheat[i]));
			if (strncasecmp(&cheat[i], "GSAv", 4) == 0 || strncasecmp(&cheat[i], "PARv", 4) == 0) {
				i += 4;
				gsaVersion = atoi(&cheat[i]);
				break;
			}
			if (strcasecmp(&cheat[i], "disabled") == 0) {
				nextDisabled = true;
				break;
			}
			if (strcasecmp(&cheat[i], "reset") == 0) {
				reset = true;
				break;
			}
			break;
		default:
			if (!set) {
				set = malloc(sizeof(*set));
				GBACheatSetInit(set, 0);
				set->enabled = !nextDisabled;
				nextDisabled = false;
				GBACheatSetGameSharkVersion(set, gsaVersion);
			}
			GBACheatAddLine(set, cheat);
			break;
		}
	}
	if (set) {
		GBACheatAddSet(device, set);
	}
	return true;
}

bool GBACheatSaveFile(struct GBACheatDevice* device, struct VFile* vf) {
	static const char lineStart[3] = "# ";
	static const char lineEnd = '\n';

	struct GBACheatHook* lastHook = 0;

	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* set = *GBACheatSetsGetPointer(&device->cheats, i);
		if (lastHook && set->hook != lastHook) {
			static const char* resetDirective = "!reset\n";
			vf->write(vf, resetDirective, strlen(resetDirective));
		}
		switch (set->gsaVersion) {
		case 1: {
			static const char* versionDirective = "!GSAv1\n";
			vf->write(vf, versionDirective, strlen(versionDirective));
			break;
		}
		case 3: {
			static const char* versionDirective = "!PARv3\n";
			vf->write(vf, versionDirective, strlen(versionDirective));
			break;
		}
		default:
			break;
		}
		lastHook = set->hook;
		if (!set->enabled) {
			static const char* disabledDirective = "!disabled\n";
			vf->write(vf, disabledDirective, strlen(disabledDirective));
		}

		vf->write(vf, lineStart, 2);
		if (set->name) {
			vf->write(vf, set->name, strlen(set->name));
		}
		vf->write(vf, &lineEnd, 1);
		size_t c;
		for (c = 0; c < StringListSize(&set->lines); ++c) {
			const char* line = *StringListGetPointer(&set->lines, c);
			vf->write(vf, line, strlen(line));
			vf->write(vf, &lineEnd, 1);
		}
	}
	return true;
}

bool GBACheatAddVBALine(struct GBACheatSet* cheats, const char* line) {
	uint32_t address;
	uint8_t op;
	uint32_t value = 0;
	int width = 0;
	const char* lineNext = hex32(line, &address);
	if (!lineNext) {
		return false;
	}
	if (lineNext[0] != ':') {
		return false;
	}
	++lineNext;
	while (width < 4) {
		lineNext = hex8(lineNext, &op);
		if (!lineNext) {
			break;
		}
		value <<= 8;
		value |= op;
		++width;
	}
	if (width == 0 || width == 3) {
		return false;
	}

	struct GBACheat* cheat = GBACheatListAppend(&cheats->list);
	cheat->address = address;
	cheat->operandOffset = 0;
	cheat->addressOffset = 0;
	cheat->repeat = 1;
	cheat->type = CHEAT_ASSIGN;
	cheat->width = width;
	cheat->operand = value;
	GBACheatRegisterLine(cheats, line);
	return true;
}

bool GBACheatAddLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	uint16_t op3;
	const char* lineNext = hex32(line, &op1);
	if (!lineNext) {
		return false;
	}
	if (lineNext[0] == ':') {
		return GBACheatAddVBALine(cheats, line);
	}
	while (isspace((int) lineNext[0])) {
		++lineNext;
	}
	lineNext = hex16(lineNext, &op2);
	if (!lineNext) {
		return false;
	}
	if (!lineNext[0] || isspace((int) lineNext[0])) {
		return GBACheatAddCodeBreaker(cheats, op1, op2);
	}
	lineNext = hex16(lineNext, &op3);
	if (!lineNext) {
		return false;
	}
	uint32_t realOp2 = op2;
	realOp2 <<= 16;
	realOp2 |= op3;
	return GBACheatAddAutodetect(cheats, op1, realOp2);
}

void GBACheatRefresh(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!cheats->enabled) {
		return;
	}
	bool condition = true;
	int conditionRemaining = 0;
	int negativeConditionRemaining = 0;
	_patchROM(device, cheats);

	size_t nCodes = GBACheatListSize(&cheats->list);
	size_t i;
	for (i = 0; i < nCodes; ++i) {
		if (conditionRemaining > 0) {
			--conditionRemaining;
			if (!condition) {
				continue;
			}
		} else if (negativeConditionRemaining > 0) {
			conditionRemaining = negativeConditionRemaining - 1;
			negativeConditionRemaining = 0;
			condition = !condition;
			if (!condition) {
				continue;
			}
		} else {
			condition = true;
		}
		struct GBACheat* cheat = GBACheatListGetPointer(&cheats->list, i);
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
			case CHEAT_ASSIGN_INDIRECT:
				value = operand;
				address = _readMem(device->p->cpu, address + cheat->addressOffset, 4);
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
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_NE:
				condition = _readMem(device->p->cpu, address, cheat->width) != operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_LT:
				condition = _readMem(device->p->cpu, address, cheat->width) < operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_GT:
				condition = _readMem(device->p->cpu, address, cheat->width) > operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_ULT:
				condition = (uint32_t) _readMem(device->p->cpu, address, cheat->width) < (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_UGT:
				condition = (uint32_t) _readMem(device->p->cpu, address, cheat->width) > (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_AND:
				condition = _readMem(device->p->cpu, address, cheat->width) & operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_LAND:
				condition = _readMem(device->p->cpu, address, cheat->width) && operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
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

void GBACheatSetCopyProperties(struct GBACheatSet* newSet, struct GBACheatSet* set) {
	newSet->gsaVersion = set->gsaVersion;
	memcpy(newSet->gsaSeeds, set->gsaSeeds, sizeof(newSet->gsaSeeds));
	if (set->hook) {
		if (newSet->hook) {
			--newSet->hook->refs;
			if (newSet->hook->refs == 0) {
				free(newSet->hook);
			}
		}
		newSet->hook = set->hook;
		++newSet->hook->refs;
	}
}

void GBACheatDeviceInit(struct ARMCore* cpu, struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	device->p = (struct GBA*) cpu->master;
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* cheats = *GBACheatSetsGetPointer(&device->cheats, i);
		_addBreakpoint(device, cheats);
		_patchROM(device, cheats);
	}
}

void GBACheatDeviceDeinit(struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	size_t i;
	for (i = GBACheatSetsSize(&device->cheats); i--;) {
		struct GBACheatSet* cheats = *GBACheatSetsGetPointer(&device->cheats, i);
		_unpatchROM(device, cheats);
		_removeBreakpoint(device, cheats);
	}
}
