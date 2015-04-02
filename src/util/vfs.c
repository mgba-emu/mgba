/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vfs.h"

ssize_t VFileReadline(struct VFile* vf, char* buffer, size_t size) {
	size_t bytesRead = 0;
	while (bytesRead < size - 1) {
		size_t newRead = vf->read(vf, &buffer[bytesRead], 1);
		bytesRead += newRead;
		if (!newRead || buffer[bytesRead] == '\n') {
			break;
		}
	}
	return buffer[bytesRead] = '\0';
}
