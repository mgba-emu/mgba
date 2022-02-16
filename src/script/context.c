/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>

void mScriptContextInit(struct mScriptContext* context) {
	// TODO: rootScope
	HashTableInit(&context->engines, 0, NULL);
}

void mScriptContextDeinit(struct mScriptContext* context) {
	HashTableDeinit(&context->engines);
}

bool mScriptInvoke(const struct mScriptFunction* fn, struct mScriptFrame* frame) {
	if (!mScriptCoerceFrame(&fn->signature.parameters, &frame->arguments)) {
		return false;
	}
	return fn->call(frame, fn->context);
}
