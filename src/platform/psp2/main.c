/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/moduleinfo.h>

PSP2_MODULE_INFO(0, 0, "mGBA");

int main() {
	printf("%s initializing", projectName);
	GBAPSP2Setup();
	GBAPSP2LoadROM("cache0:/VitaDefilerClient/Documents/GBA/rom.gba");
	GBAPSP2Runloop();
	GBAPSP2UnloadROM();
	GBAPSP2Teardown();

	sceKernelExitProcess(0);
	return 0;
}
