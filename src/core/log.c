/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "log.h"

struct mLogger* mLogGetContext(void) {
	return NULL; // TODO
}

int mLogGenerateCategory(void) {
	static int category = 0;
	++category;
	return category;
}
