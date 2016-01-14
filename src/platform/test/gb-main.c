/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lr35902/lr35902.h"
#include "gb/gb.h"
#include "util/vfs.h"

int main(int argc, char* argv[]) {
	struct LR35902Core cpu;
	struct GB gb;

	GBCreate(&gb);
	LR35902SetComponents(&cpu, &gb.d, 0, 0);
	LR35902Init(&cpu);
	struct VFile* vf = VFileOpen(argv[1], O_RDONLY);
	GBLoadROM(&gb, vf, 0, argv[1]);

	LR35902Reset(&cpu);
	while (true) {
		LR35902Tick(&cpu);
	}
	vf->close(vf);
	return 0;
}
