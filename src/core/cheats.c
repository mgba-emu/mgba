/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/cheats.h>

#include <mgba/core/core.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#define MAX_LINE_LENGTH 512
#define MAX_CHEATS 1000

const uint32_t M_CHEAT_DEVICE_ID = 0xABADC0DE;

mLOG_DEFINE_CATEGORY(CHEATS, "Cheats", "core.cheats");

DEFINE_VECTOR(mCheatList, struct mCheat);
DEFINE_VECTOR(mCheatSets, struct mCheatSet*);
DEFINE_VECTOR(mCheatPatchList, struct mCheatPatch);

struct mCheatPatchedMem {
	uint32_t originalValue;
	int refs;
	bool dirty;
};

static uint32_t _patchMakeKey(struct mCheatPatch* patch) {
	// NB: This assumes patches have only one valid size per platform
	uint32_t patchKey = patch->address;
	switch (patch->width) {
	case 2:
		patchKey >>= 1;
		break;
	case 4:
		patchKey >>= 2;
		break;
	default:
		break;	
	}
	// TODO: More than one segment
	if (patch->segment > 0) {
		patchKey |= patch->segment << 16;
	}
	return patchKey;
}

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

static int32_t _readMemSegment(struct mCore* core, uint32_t address, int segment, int width) {
	switch (width) {
	case 1:
		return core->rawRead8(core, address, segment);
	case 2:
		return core->rawRead16(core, address, segment);
	case 4:
		return core->rawRead32(core, address, segment);
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

static void _patchMem(struct mCore* core, uint32_t address, int segment, int width, int32_t value) {
	switch (width) {
	case 1:
		core->rawWrite8(core, address, segment, value);
		break;
	case 2:
		core->rawWrite16(core, address, segment, value);
		break;
	case 4:
		core->rawWrite32(core, address, segment, value);
		break;
	}
}

static void _patchROM(struct mCheatDevice* device, struct mCheatSet* cheats) {
	if (!device->p) {
		return;
	}
	size_t i;
	for (i = 0; i < mCheatPatchListSize(&cheats->romPatches); ++i) {
		struct mCheatPatch* patch = mCheatPatchListGetPointer(&cheats->romPatches, i);
		int segment = -1;
		if (patch->check && patch->segment < 0) {
			const struct mCoreMemoryBlock* block = mCoreGetMemoryBlockInfo(device->p, patch->address);
			if (!block) {
				continue;
			}
			for (segment = 0; segment < block->maxSegment; ++segment) {
				uint32_t value = _readMemSegment(device->p, patch->address, segment, patch->width);
				if (value == patch->checkValue) {
					break;
				}
			}
			if (segment == block->maxSegment) {
				continue;
			}
		}
		patch->segment = segment;

		uint32_t patchKey = _patchMakeKey(patch);
		struct mCheatPatchedMem* patchData = TableLookup(&device->unpatchedMemory, patchKey);
		if (!patchData) {
			patchData = malloc(sizeof(*patchData));
			patchData->originalValue = _readMemSegment(device->p, patch->address, segment, patch->width);
			patchData->refs = 1;
			patchData->dirty = false;
			TableInsert(&device->unpatchedMemory, patchKey, patchData);
		} else if (!patch->applied) {
			++patchData->refs;
			patchData->dirty = true;
		} else if (!patchData->dirty) {
			continue;
		}
		_patchMem(device->p, patch->address, segment, patch->width, patch->value);
		patch->applied = true;
	}
}

static void _unpatchROM(struct mCheatDevice* device, struct mCheatSet* cheats) {
	if (!device->p) {
		return;
	}
	size_t i;
	for (i = 0; i < mCheatPatchListSize(&cheats->romPatches); ++i) {
		struct mCheatPatch* patch = mCheatPatchListGetPointer(&cheats->romPatches, i);
		if (!patch->applied) {
			continue;
		}
		uint32_t patchKey = _patchMakeKey(patch);
		struct mCheatPatchedMem* patchData = TableLookup(&device->unpatchedMemory, patchKey);
		--patchData->refs;
		patchData->dirty = true;
		if (patchData->refs <= 0) {
			_patchMem(device->p, patch->address, patch->segment, patch->width, patchData->originalValue);
			TableRemove(&device->unpatchedMemory, patchKey);
		}
		patch->applied = false;
	}
}

static void mCheatDeviceInit(void*, struct mCPUComponent*);
static void mCheatDeviceDeinit(struct mCPUComponent*);

void mCheatDeviceCreate(struct mCheatDevice* device) {
	device->d.id = M_CHEAT_DEVICE_ID;
	device->d.init = mCheatDeviceInit;
	device->d.deinit = mCheatDeviceDeinit;
	device->autosave = false;
	device->buttonDown = false;
	mCheatSetsInit(&device->cheats, 4);
	TableInit(&device->unpatchedMemory, 4, free);
}

void mCheatDeviceDestroy(struct mCheatDevice* device) {
	mCheatDeviceClear(device);
	mCheatSetsDeinit(&device->cheats);
	TableDeinit(&device->unpatchedMemory);
	free(device);
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
	mCheatPatchListInit(&set->romPatches, 4);
	if (name) {
		set->name = strdup(name);
	} else {
		set->name = 0;
	}
	set->enabled = true;
}

void mCheatSetDeinit(struct mCheatSet* set) {
	size_t i;
	for (i = 0; i < StringListSize(&set->lines); ++i) {
		free(*StringListGetPointer(&set->lines, i));
	}
	mCheatListDeinit(&set->list);
	if (set->name) {
		free(set->name);
	}
	StringListDeinit(&set->lines);
	mCheatPatchListDeinit(&set->romPatches);
	if (set->deinit) {
		set->deinit(set);
	}
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
	if (cheats->add) {
		cheats->add(cheats, device);
	}
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
	if (cheats->remove) {
		cheats->remove(cheats, device);
	}
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
				if (strncmp(cheat, "cheats = ", 9) == 0) {
					// This is in libretro format, switch over to that parser
					vf->seek(vf, 0, SEEK_SET);
					StringListDeinit(&directives);
					return mCheatParseLibretroFile(device, vf);
				}
				if (cheat[0] == '[') {
					// This is in EZ Flash CHT format, switch over to that parser
					vf->seek(vf, 0, SEEK_SET);
					StringListDeinit(&directives);
					return mCheatParseEZFChtFile(device, vf);
				}
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

bool mCheatParseLibretroFile(struct mCheatDevice* device, struct VFile* vf) {
	char cheat[MAX_LINE_LENGTH];
	char parsed[MAX_LINE_LENGTH];
	struct mCheatSet* set = NULL;
	unsigned long i = 0;
	bool startFound = false;

	while (true) {
		ssize_t bytesRead = vf->readline(vf, cheat, sizeof(cheat));
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < 0) {
			return false;
		}
		if (cheat[0] == '\n') {
			continue;
		}
		if (strncmp(cheat, "cheat", 5) != 0) {
			return false;
		}
		char* underscore = strchr(&cheat[5], '_');
		if (!underscore) {
			if (!startFound && cheat[5] == 's') {
				startFound = true;
				char* eq = strchr(&cheat[6], '=');
				if (!eq) {
					return false;
				}
				++eq;
				while (isspace((int) eq[0])) {
					if (eq[0] == '\0') {
						return false;
					}
					++eq;
				}

				char* end;
				unsigned long nCheats = strtoul(eq, &end, 10);
				if (end[0] != '\0' && !isspace(end[0])) {
					return false;
				}

				if (nCheats > MAX_CHEATS) {
					return false;
				}

				while (nCheats > mCheatSetsSize(&device->cheats)) {
					struct mCheatSet* newSet = device->createSet(device, NULL);
					if (!newSet) {
						return false;
					}
					mCheatAddSet(device, newSet);
				}
				continue;
			}
			return false;
		}
		char* underscore2;
		i = strtoul(&cheat[5], &underscore2, 10);
		if (underscore2 != underscore) {
			return false;
		}
		++underscore;
		char* eq = strchr(underscore, '=');
		if (!eq) {
			return false;
		}
		++eq;
		while (isspace((int) eq[0])) {
			if (eq[0] == '\0') {
				return false;
			}
			++eq;
		}

		if (i >= mCheatSetsSize(&device->cheats)) {
			return false;
		}
		set = *mCheatSetsGetPointer(&device->cheats, i);

		if (strncmp(underscore, "desc", 4) == 0) {
			parseQuotedString(eq, strlen(eq), parsed, sizeof(parsed));
			mCheatSetRename(set, parsed);
		} else if (strncmp(underscore, "enable", 6) == 0) {
			set->enabled = strncmp(eq, "true\n", 5) == 0;
		} else if (strncmp(underscore, "code", 4) == 0) {
			parseQuotedString(eq, strlen(eq), parsed, sizeof(parsed));
			char* cur = parsed;
			char* next;
			while ((next = strchr(cur, '+'))) {
				next[0] = '\0';
				mCheatAddLine(set, cur, 0);
				cur = &next[1];
			}
			mCheatAddLine(set, cur, 0);

			for (++i; i < mCheatSetsSize(&device->cheats); ++i) {
				struct mCheatSet* newSet = *mCheatSetsGetPointer(&device->cheats, i);
				newSet->copyProperties(newSet, set);
			}
		}
	}
	return true;
}

bool mCheatParseEZFChtFile(struct mCheatDevice* device, struct VFile* vf) {
	char cheat[MAX_LINE_LENGTH];
	char cheatName[MAX_LINE_LENGTH];
	char miniline[32];
	size_t cheatNameLength = 0;
	struct mCheatSet* set = NULL;

	cheatName[MAX_LINE_LENGTH - 1] = '\0';
	while (true) {
		ssize_t bytesRead = vf->readline(vf, cheat, sizeof(cheat));
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < 0) {
			return false;
		}
		if (cheat[0] == '\n' || (bytesRead >= 2 && cheat[0] == '\r' && cheat[1] == '\n')) {
			continue;
		}

		if (cheat[0] == '[') {
			if (strncmp(cheat, "[GameInfo]", 10) == 0) {
				break;
			}
			char* end = strchr(cheat, ']');
			if (!end) {
				return false;
			}
			char* name = gbkToUtf8(&cheat[1], end - cheat - 1);
			if (name) {
				strncpy(cheatName, name, sizeof(cheatName) - 1);
				free(name);
			}
			cheatNameLength = strlen(cheatName);
			continue;
		}

		char* eq = strchr(cheat, '=');
		if (!eq) {
			continue;
		}
		if (strncmp(cheat, "ON", eq - cheat) != 0) {
			char* subname = gbkToUtf8(cheat, eq - cheat);
			if (subname) {
				snprintf(&cheatName[cheatNameLength], sizeof(cheatName) - cheatNameLength - 1, ": %s", subname);
				free(subname);
			}
		}
		set = device->createSet(device, cheatName);
		set->enabled = false;
		mCheatAddSet(device, set);
		cheatName[cheatNameLength] = '\0';
		++eq;

		uint32_t gameptr = 0;
		uint32_t hexval = 0;
		int digit;
		while (eq[0] != '\r' && eq[1] != '\n') {
			if (cheat + bytesRead == eq || eq[0] == '\0') {
				bytesRead = vf->readline(vf, cheat, sizeof(cheat));
				eq = cheat;
				if (bytesRead == 0) {
					break;
				}
				if (bytesRead < 0) {
					return false;
				}
			}
			switch (eq[0]) {
			case ',':
				if (!gameptr) {
					gameptr = hexval;
					if (hexval < 0x40000) {
						gameptr += 0x02000000;
					} else {
						gameptr += 0x03000000 - 0x40000;
					}
				} else {
					if (hexval > 0xFF) {
						return false;
					}
					snprintf(miniline, sizeof(miniline) - 1, "%08X:%02X", gameptr, hexval);
					mCheatAddLine(set, miniline, 0);
					++gameptr;
				}
				hexval = 0;
				break;
			case ';':
				if (hexval > 0xFF) {
					return false;
				}
				snprintf(miniline, sizeof(miniline) - 1, "%08X:%02X", gameptr, hexval);
				mCheatAddLine(set, miniline, 0);
				hexval = 0;
				gameptr = 0;
				break;
			default:
				digit = hexDigit(eq[0]);
				if (digit < 0) {
					return false;
				}
				hexval <<= 4;
				hexval |= digit;
				break;
			}
			++eq;
		}
		if (gameptr) {
			if (hexval > 0xFF) {
				return false;
			}
			snprintf(miniline, sizeof(miniline) - 1, "%08X:%02X", gameptr, hexval);
			mCheatAddLine(set, miniline, 0);
		}
	}
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
				vf->write(vf, directive, (size_t) len > sizeof(directive) ? sizeof(directive) : (size_t) len);
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

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
void mCheatAutosave(struct mCheatDevice* device) {
	if (!device->autosave) {
		return;
	}
	struct VFile* vf = mDirectorySetOpenSuffix(&device->p->dirs, device->p->dirs.cheats, ".cheats", O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return;
	}
	mCheatSaveFile(device, vf);
	vf->close(vf);
}
#endif

void mCheatRefresh(struct mCheatDevice* device, struct mCheatSet* cheats) {
	if (cheats->enabled) {
		_patchROM(device, cheats);
	}
	if (cheats->refresh) {
		cheats->refresh(cheats, device);
	}
	if (!cheats->enabled) {
		_unpatchROM(device, cheats);
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
				address = _readMem(device->p, address, 4) + cheat->addressOffset;
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
			case CHEAT_IF_BUTTON:
				condition = device->buttonDown;
				conditionRemaining = cheat->repeat;
				negativeConditionRemaining = cheat->negativeRepeat;
				operationsRemaining = 1;
				break;
			case CHEAT_NEVER:
				condition = false;
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

void mCheatPressButton(struct mCheatDevice* device, bool down) {
	device->buttonDown = down;
}

void mCheatDeviceInit(void* cpu, struct mCPUComponent* component) {
	UNUSED(cpu);
	struct mCheatDevice* device = (struct mCheatDevice*) component;
	size_t i;
	for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
		struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
		if (cheats->add) {
			cheats->add(cheats, device);
		}
	}
}

void mCheatDeviceDeinit(struct mCPUComponent* component) {
	struct mCheatDevice* device = (struct mCheatDevice*) component;
	size_t i;
	for (i = mCheatSetsSize(&device->cheats); i--;) {
		struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
		if (cheats->remove) {
			cheats->remove(cheats, device);
		}
	}
}
