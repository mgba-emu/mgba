/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/debugger/symbols.h>

#include <mgba/internal/debugger/symbols.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

void GBLoadSymbols(struct mDebuggerSymbols* st, struct VFile* vf) {
	char line[512];

	while (true) {
		ssize_t bytesRead = vf->readline(vf, line, sizeof(line));
		if (bytesRead <= 0) {
			break;
		}
		if (line[bytesRead - 1] == '\n') {
			line[bytesRead - 1] = '\0';
		}
		int segment = -1;
		uint32_t address = 0;

		uint8_t byte;
		const char* buf = line;
		while (buf && bytesRead >= 2) {
			buf = hex8(buf, &byte);
			if (!buf) {
				break;
			}
			address <<= 8;
			address += byte;
			bytesRead -= 2;

			if (buf[0] == ':') {
				segment = address;
				address = 0;
				++buf;
			}
			if (isspace((int) buf[0])) {
				break;
			}
		}
		if (!buf || bytesRead < 1) {
			continue;
		}

		while (isspace((int) buf[0]) && bytesRead > 0) {
			--bytesRead;
			++buf;
		}

		if (!bytesRead) {
			continue;
		}

		mDebuggerSymbolAdd(st, buf, address, segment);
	}
}
