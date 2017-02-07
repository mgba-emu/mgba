/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/rewind.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/patch/fast.h>
#include <mgba-util/vfs.h>

DEFINE_VECTOR(mCoreRewindPatches, struct PatchFast);

void mCoreRewindContextInit(struct mCoreRewindContext* context, size_t entries) {
	mCoreRewindPatchesInit(&context->patchMemory, entries);
	size_t e;
	for (e = 0; e < entries; ++e) {
		initPatchFast(mCoreRewindPatchesAppend(&context->patchMemory));
	}
	context->previousState = VFileMemChunk(0, 0);
	context->currentState = VFileMemChunk(0, 0);
	context->size = 0;
	context->stateFlags = SAVESTATE_SAVEDATA;
}

void mCoreRewindContextDeinit(struct mCoreRewindContext* context) {
	context->previousState->close(context->previousState);
	context->currentState->close(context->currentState);
	size_t s;
	for (s = 0; s < mCoreRewindPatchesSize(&context->patchMemory); ++s) {
		deinitPatchFast(mCoreRewindPatchesGetPointer(&context->patchMemory, s));
	}
	mCoreRewindPatchesDeinit(&context->patchMemory);
}

void mCoreRewindAppend(struct mCoreRewindContext* context, struct mCore* core) {
	struct VFile* nextState = context->previousState;
	++context->current;
	if (context->size < mCoreRewindPatchesSize(&context->patchMemory)) {
		++context->size;
	}
	if (context->current >= mCoreRewindPatchesSize(&context->patchMemory)) {
		context->current = 0;
	}
	mCoreSaveStateNamed(core, nextState, context->stateFlags);
	struct PatchFast* patch = mCoreRewindPatchesGetPointer(&context->patchMemory, context->current);
	size_t size2 = nextState->size(nextState);
	size_t size = context->currentState->size(context->currentState);
	if (size2 > size) {
		context->currentState->truncate(context->currentState, size2);
		size = size2;
	}
	void* current = context->currentState->map(context->currentState, size, MAP_READ);
	void* next = nextState->map(nextState, size, MAP_READ);
	diffPatchFast(patch, current, next, size);
	context->currentState->unmap(context->currentState, current, size);
	nextState->unmap(next, nextState, size);
	context->previousState = context->currentState;
	context->currentState = nextState;
}

bool mCoreRewindRestore(struct mCoreRewindContext* context, struct mCore* core) {
	if (!context->size) {
		return false;
	}
	--context->size;

	struct PatchFast* patch = mCoreRewindPatchesGetPointer(&context->patchMemory, context->current);
	size_t size2 = context->previousState->size(context->previousState);
	size_t size = context->currentState->size(context->currentState);
	if (size2 < size) {
		size = size2;
	}
	void* current = context->currentState->map(context->currentState, size, MAP_READ);
	void* previous = context->previousState->map(context->previousState, size, MAP_WRITE);
	patch->d.applyPatch(&patch->d, current, size, previous, size);
	context->currentState->unmap(context->currentState, current, size);
	context->previousState->unmap(context->previousState, previous, size);
	mCoreLoadStateNamed(core, context->previousState, context->stateFlags);
	struct VFile* nextState = context->previousState;
	context->previousState = context->currentState;
	context->currentState = nextState;

	if (context->current == 0) {
		context->current = mCoreRewindPatchesSize(&context->patchMemory);
	} 
	--context->current;
	return true;
}
