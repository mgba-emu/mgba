/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/bios.h>

#include <mgba/internal/arm/arm.h>

mLOG_DEFINE_CATEGORY(DS_BIOS, "DS BIOS");

const uint32_t DS7_BIOS_CHECKSUM = 0x1280F0D5;
const uint32_t DS9_BIOS_CHECKSUM = 0x2AB23573;

void DS7Swi16(struct ARMCore* cpu, int immediate) {
	mLOG(DS_BIOS, DEBUG, "SWI7: %02X r0: %08X r1: %08X r2: %08X r3: %08X",
	    immediate, cpu->gprs[0], cpu->gprs[1], cpu->gprs[2], cpu->gprs[3]);

	ARMRaiseSWI(cpu);
}

void DS7Swi32(struct ARMCore* cpu, int immediate) {
	DS7Swi16(cpu, immediate >> 16);
}
