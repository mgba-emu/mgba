/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_CANVAS_H
#define M_SCRIPT_CANVAS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/script/context.h>
#include <mgba/script/macros.h>

struct VideoBackend;
void mScriptContextAttachCanvas(struct mScriptContext* context);
void mScriptCanvasUpdate(struct mScriptContext* context);
void mScriptCanvasUpdateBackend(struct mScriptContext* context, struct VideoBackend*);
void mScriptCanvasSetInternalScale(struct mScriptContext* context, unsigned scale);

CXX_GUARD_END

#endif
