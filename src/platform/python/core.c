/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include <mgba/core/core.h>

struct mCoreCallbacks* mCorePythonCallbackCreate(void* pyobj) {
	struct mCoreCallbacks* callbacks = malloc(sizeof(*callbacks));
	callbacks->videoFrameStarted = _mCorePythonCallbacksVideoFrameStarted;
	callbacks->videoFrameEnded = _mCorePythonCallbacksVideoFrameEnded;
	callbacks->coreCrashed = _mCorePythonCallbacksCoreCrashed;
	callbacks->sleep = _mCorePythonCallbacksSleep;
	callbacks->keysRead = _mCorePythonCallbacksKeysRead;

	callbacks->context = pyobj;
	return callbacks;
}
