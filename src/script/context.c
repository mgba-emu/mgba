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

bool mScriptInvoke(const struct mScriptValue* val, struct mScriptFrame* frame) {
	if (val->type->base != mSCRIPT_TYPE_FUNCTION) {
		return false;
	}
	const struct mScriptTypeFunction* signature = &val->type->details.function;
	if (!mScriptCoerceFrame(&signature->parameters, &frame->arguments)) {
		return false;
	}
	const struct mScriptFunction* fn = val->value.opaque;
	return fn->call(frame, fn->context);
}
