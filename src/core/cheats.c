/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/cheats.h>

#include <mgba/core/core.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#define MAX_LINE_LENGTH 128

const uint32_t M_CHEAT_DEVICE_ID = 0xABADC0DE;

mLOG_DEFINE_CATEGORY(CHEATS, "Cheats", "core.cheats");

DEFINE_VECTOR(mCheatList, struct mCheat);
DEFINE_VECTOR(mCheatSets, struct mCheatSet*);

static int32_t _readMem(struct mCore* core, uint32_t address, int width) {
	switch (width) {
	case 1:
		return core->busRead8(core, address);
	case 2:
		return core->busRead16(core, address);
	case 4:
		return core->busRead32(core, address);
	}
	return 0;
}

static void _writeMem(struct mCore* core, uint32_t address, int width, int32_t value) {
	switch (width) {
	case 1:
		core->busWrite8(core, address, value);
		break;
	case 2:
		core->busWrite16(core, address, value);
		break;
	case 4:
		core->busWrite32(core, address, value);
		break;
	}
}

static void mCheatDeviceInit(void*, struct mCPUComponent*);
static void mCheatDeviceDeinit(struct mCPUComponent*);

void mCheatDeviceCreate(struct mCheatDevice* device) {
	device->d.id = M_CHEAT_DEVICE_ID;
	device->d.init = mCheatDeviceInit;
	device->d.deinit = mCheatDeviceDeinit;
	mCheatSetsInit(&device->cheats, 4);
}

void mCheatDeviceDestroy(struct mCheatDevice* device) {
	mCheatDeviceClear(device);
	mCheatSetsDeinit(&device->cheats);
}

void mCheatDeviceClear(struct mCheatDevice* device) {
	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		struct mCheatSet* set = *mCheatSetsGetPointer(&device->cheats, i);
		mCheatSetDeinit(set);
	}
	mCheatSetsClear(&device->cheats);
}

void mCheatSetInit(struct mCheatSet* set, const char* name) {
	mCheatListInit(&set->list, 4);
	StringListInit(&set->lines, 4);
	if (name) {
		set->name = strdup(name);
	} else {
		set->name = 0;
	}
	set->enabled = true;
}

void mCheatSetDeinit(struct mCheatSet* set) {
	mCheatListDeinit(&set->list);
	size_t i;
	for (i = 0; i < StringListSize(&set->lines); ++i) {
		free(*StringListGetPointer(&set->lines, i));
	}
	if (set->name) {
		free(set->name);
	}
	set->deinit(set);
	free(set);
}

void mCheatSetRename(struct mCheatSet* set, const char* name) {
	if (set->name) {
		free(set->name);
		set->name = NULL;
	}
	if (name) {
		set->name = strdup(name);
	}
}

bool mCheatAddLine(struct mCheatSet* set, const char* line, int type) {
	if (!set->addLine(set, line, type)) {
		return false;
	}
	*StringListAppend(&set->lines) = strdup(line);
	return true;
}

void mCheatAddSet(struct mCheatDevice* device, struct mCheatSet* cheats) {
	*mCheatSetsAppend(&device->cheats) = cheats;
	cheats->add(cheats, device);
}

void mCheatRemoveSet(struct mCheatDevice* device, struct mCheatSet* cheats) {
	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		if (*mCheatSetsGetPointer(&device->cheats, i) == cheats) {
			break;
		}
	}
	if (i == mCheatSetsSize(&device->cheats)) {
		return;
	}
	mCheatSetsShift(&device->cheats, i, 1);
	cheats->remove(cheats, device);
}

bool mCheatParseFile(struct mCheatDevice* device, struct VFile* vf) {
	char cheat[MAX_LINE_LENGTH];
	struct mCheatSet* set = NULL;
	struct mCheatSet* newSet;
	bool nextDisabled = false;
	struct StringList directives;
	StringListInit(&directives, 4);

	while (true) {
		size_t i = 0;
		ssize_t bytesRead = vf->readline(vf, cheat, sizeof(cheat));
		rtrim(cheat);
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < 0) {
			StringListDeinit(&directives);
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
			newSet = device->createSet(device, &cheat[i]);
			newSet->enabled = !nextDisabled;
			nextDisabled = false;
			if (set) {
				mCheatAddSet(device, set);
			}
			if (set) {
				newSet->copyProperties(newSet, set);
			}
			newSet->parseDirectives(newSet, &directives);
			set = newSet;
			break;
		case '!':
			do {
				++i;
			} while (isspace((int) cheat[i]));
			if (strcasecmp(&cheat[i], "disabled") == 0) {
				nextDisabled = true;
				break;
			}
			if (strcasecmp(&cheat[i], "reset") == 0) {
				size_t d;
				for (d = 0; d < StringListSize(&directives); ++d) {
					free(*StringListGetPointer(&directives, d));
				}
				StringListClear(&directives);
				break;
			}
			*StringListAppend(&directives) = strdup(&cheat[i]);
			break;
		default:
			if (!set) {
				set = device->createSet(device, NULL);
				set->enabled = !nextDisabled;
				nextDisabled = false;
			}
			mCheatAddLine(set, cheat, 0);
			break;
		}
	}
	if (set) {
		mCheatAddSet(device, set);
	}
	size_t d;
	for (d = 0; d < StringListSize(&directives); ++d) {
		free(*StringListGetPointer(&directives, d));
	}
	StringListClear(&directives);
	StringListDeinit(&directives);
	return true;
}

bool mCheatSaveFile(struct mCheatDevice* device, struct VFile* vf) {
	static const char lineStart[3] = "# ";
	static const char lineEnd = '\n';
	struct StringList directives;
	StringListInit(&directives, 4);

	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		struct mCheatSet* set = *mCheatSetsGetPointer(&device->cheats, i);
		set->dumpDirectives(set, &directives);
		if (!set->enabled) {
			static const char* disabledDirective = "!disabled\n";
			vf->write(vf, disabledDirective, strlen(disabledDirective));
		}
		size_t d;
		for (d = 0; d < StringListSize(&directives); ++d) {
			char directive[64];
			ssize_t len = snprintf(directive, sizeof(directive) - 1, "!%s\n", *StringListGetPointer(&directives, d));
			if (len > 1) {
				vf->write(vf, directive, (size_t) len > sizeof(directive) ? sizeof(directive) : len);
			}
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
	size_t d;
	for (d = 0; d < StringListSize(&directives); ++d) {
		free(*StringListGetPointer(&directives, d));
	}
	StringListClear(&directives);
	StringListDeinit(&directives);
	return true;
}

void mCheatRefresh(struct mCheatDevice* device, struct mCheatSet* cheats) {
	cheats->refresh(cheats, device);
	if (!cheats->enabled) {
		return;
	}

	size_t elseLoc = 0;
	size_t endLoc = 0;
	size_t nCodes = mCheatListSize(&cheats->list);
	size_t i;
	for (i = 0; i < nCodes; ++i) {
		struct mCheat* cheat = mCheatListGetPointer(&cheats->list, i);
		int32_t value = 0;
		int32_t operand = cheat->operand;
		uint32_t operationsRemaining = cheat->repeat;
		uint32_t address = cheat->address;
		bool performAssignment = false;
		bool condition = true;
		int conditionRemaining = 0;
		int negativeConditionRemaining = 0;

		for (; operationsRemaining; --operationsRemaining) {
			switch (cheat->type) {
			case CHEAT_ASSIGN:
				value = operand;
				performAssignment = true;
				break;
			case CHEAT_ASSIGN_INDIRECT:
				value = operand;
				address = _readMem(device->p, address + cheat->addressOffset, 4);
				performAssignment = true;
				break;
			case CHEAT_AND:
				value = _readMem(device->p, address, cheat->width) & operand;
				performAssignment = true;
				break;
			case CHEAT_ADD:
				value = _readMem(device->p, address, cheat->width) + operand;
				performAssignment = true;
				break;
			case CHEAT_OR:
				value = _readMem(device->p, address, cheat->width) | operand;
				performAssignment = true;
				break;
			case CHEAT_IF_EQ:
				condition = _readMem(device->p, address, cheat->width) == operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_NE:
				condition = _readMem(device->p, address, cheat->width) != operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_LT:
				condition = _readMem(device->p, address, cheat->width) < operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_GT:
				condition = _readMem(device->p, address, cheat->width) > operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_ULT:
				condition = (uint32_t) _readMem(device->p, address, cheat->width) < (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_UGT:
				condition = (uint32_t) _readMem(device->p, address, cheat->width) > (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_AND:
				condition = _readMem(device->p, address, cheat->width) & operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_LAND:
				condition = _readMem(device->p, address, cheat->width) && operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_IF_NAND:
				condition = !(_readMem(device->p, address, cheat->width) & operand);
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			}

			if (performAssignment) {
				_writeMem(device->p, address, cheat->width, value);
			}

			address += cheat->addressOffset;
			operand += cheat->operandOffset;
		}


		if (elseLoc && i == elseLoc) {
			i = endLoc;
			endLoc = 0;
		}
		if (conditionRemaining > 0 && !condition) {
			i += conditionRemaining;
		} else if (negativeConditionRemaining > 0) {
			elseLoc = i + conditionRemaining;
			endLoc = elseLoc + negativeConditionRemaining;
		}
	}
}

void mCheatDeviceInit(void* cpu, struct mCPUComponent* component) {
	UNUSED(cpu);
	struct mCheatDevice* device = (struct mCheatDevice*) component;
	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
		cheats->add(cheats, device);
	}
}

void mCheatDeviceDeinit(struct mCPUComponent* component) {
	struct mCheatDevice* device = (struct mCheatDevice*) component;
	size_t i;
	for (i = mCheatSetsSize(&device->cheats); i--;) {
		struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
		cheats->remove(cheats, device);
	}
}
