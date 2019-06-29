/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/common.h>

#include "pycommon.h"

struct mCoreCallbacks* mCorePythonCallbackCreate(void* pyobj);

PYEXPORT void _mCorePythonCallbacksVideoFrameStarted(void* user);
PYEXPORT void _mCorePythonCallbacksVideoFrameEnded(void* user);
PYEXPORT void _mCorePythonCallbacksCoreCrashed(void* user);
PYEXPORT void _mCorePythonCallbacksSleep(void* user);
PYEXPORT void _mCorePythonCallbacksKeysRead(void* user);
