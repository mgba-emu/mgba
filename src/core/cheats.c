/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cheats.h"

#include "core/core.h"
#include "util/string.h"
#include "util/vfs.h"

#define MAX_LINE_LENGTH 128

const uint32_t M_CHEAT_DEVICE_ID = 0xABADC0DE;

mLOG_DEFINE_CATEGORY(CHEATS, "Cheats");

DEFINE_VECTOR(mCheatList, struct mCheat);
DEFINE_VECTOR(mCheatSets, struct mCheatSet*);
DEFINE_VECTOR(StringList, char*);

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
#warning Cheat loading is currently broken
	return false;
#if 0
	char cheat[MAX_LINE_LENGTH];
	struct mCheatSet* set = NULL;
	struct mCheatSet* newSet;
	bool nextDisabled = false;
	void* directives = NULL;
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
			cheat[strlen(cheat) - 1] = '\0'; // Remove trailing newline
			newSet = device->createSet(device, &cheat[i]);
			newSet->enabled = !nextDisabled;
			nextDisabled = false;
			if (set) {
				mCheatAddSet(device, set);
			}
			if (set) {
				newSet->copyProperties(newSet, set);
			}
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
				directives = NULL;
				break;
			}
			directives = set->parseDirective(set, &cheat[i], directives);
			break;
		default:
			if (!set) {
				set = device->createSet(device, NULL);
				set->enabled = !nextDisabled;
				nextDisabled = false;
			}
			mCheatAddLine(set, cheat);
			break;
		}
	}
	if (set) {
		mCheatAddSet(device, set);
	}
	return true;
#endif
}

bool mCheatSaveFile(struct mCheatDevice* device, struct VFile* vf) {
#warning Cheat saving is currently broken
	return false;
#if 0
	static const char lineStart[3] = "# ";
	static const char lineEnd = '\n';
	void* directives = NULL;

	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		struct mCheatSet* set = *mCheatSetsGetPointer(&device->cheats, i);
		void* directives = set->dumpDirectives(set, vf, directives);
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
#endif
}

void mCheatRefresh(struct mCheatDevice* device, struct mCheatSet* cheats) {
	if (!cheats->enabled) {
		return;
	}
	bool condition = true;
	int conditionRemaining = 0;
	int negativeConditionRemaining = 0;
	cheats->refresh(cheats, device);

	size_t nCodes = mCheatListSize(&cheats->list);
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
		struct mCheat* cheat = mCheatListGetPointer(&cheats->list, i);
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
				break;
			case CHEAT_IF_NE:
				condition = _readMem(device->p, address, cheat->width) != operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_LT:
				condition = _readMem(device->p, address, cheat->width) < operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_GT:
				condition = _readMem(device->p, address, cheat->width) > operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_ULT:
				condition = (uint32_t) _readMem(device->p, address, cheat->width) < (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_UGT:
				condition = (uint32_t) _readMem(device->p, address, cheat->width) > (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_AND:
				condition = _readMem(device->p, address, cheat->width) & operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			case CHEAT_IF_LAND:
				condition = _readMem(device->p, address, cheat->width) && operand;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				break;
			}

			if (performAssignment) {
				_writeMem(device->p, address, cheat->width, value);
			}

			address += cheat->addressOffset;
			operand += cheat->operandOffset;
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
