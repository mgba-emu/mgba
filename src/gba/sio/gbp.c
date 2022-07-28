/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/formatting.h>
#include <mgba-util/hash.h>
#include <mgba-util/memory.h>

static uint16_t _gbpRead(struct mKeyCallback*);
static uint16_t _gbpSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static void _gbpSioProcessEvents(struct mTiming* timing, void* user, uint32_t cyclesLate);

static const uint8_t _logoPalette[] = {
	0xDF, 0xFF, 0x0C, 0x64, 0x0C, 0xE4, 0x2D, 0xE4, 0x4E, 0x64, 0x4E, 0xE4, 0x6E, 0xE4, 0xAF, 0x68,
	0xB0, 0xE8, 0xD0, 0x68, 0xF0, 0x68, 0x11, 0x69, 0x11, 0xE9, 0x32, 0x6D, 0x32, 0xED, 0x73, 0xED,
	0x93, 0x6D, 0x94, 0xED, 0xB4, 0x6D, 0xD5, 0xF1, 0xF5, 0x71, 0xF6, 0xF1, 0x16, 0x72, 0x57, 0x72,
	0x57, 0xF6, 0x78, 0x76, 0x78, 0xF6, 0x99, 0xF6, 0xB9, 0xF6, 0xD9, 0x76, 0xDA, 0xF6, 0x1B, 0x7B,
	0x1B, 0xFB, 0x3C, 0xFB, 0x5C, 0x7B, 0x7D, 0x7B, 0x7D, 0xFF, 0x9D, 0x7F, 0xBE, 0x7F, 0xFF, 0x7F,
	0x2D, 0x64, 0x8E, 0x64, 0x8F, 0xE8, 0xF1, 0xE8, 0x52, 0x6D, 0x73, 0x6D, 0xB4, 0xF1, 0x16, 0xF2,
	0x37, 0x72, 0x98, 0x76, 0xFA, 0x7A, 0xFA, 0xFA, 0x5C, 0xFB, 0xBE, 0xFF, 0xDE, 0x7F, 0xFF, 0xFF,
	0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint32_t _logoHash = 0xEEDA6963;

static const uint32_t _gbpTxData[] = {
	0x0000494E, 0x0000494E,
	0xB6B1494E, 0xB6B1544E,
	0xABB1544E, 0xABB14E45,
	0xB1BA4E45, 0xB1BA4F44,
	0xB0BB4F44, 0xB0BB8002,
	0x10000010, 0x20000013,
	0x30000003
};

void GBASIOPlayerInit(struct GBASIOPlayer* gbp) {
	gbp->callback.d.readKeys = _gbpRead;
	gbp->callback.d.requireOpposingDirections = true;
	gbp->callback.p = gbp;
	gbp->d.init = 0;
	gbp->d.deinit = 0;
	gbp->d.load = 0;
	gbp->d.unload = 0;
	gbp->d.writeRegister = _gbpSioWriteRegister;
	gbp->event.context = gbp;
	gbp->event.name = "GBA SIO Game Boy Player";
	gbp->event.callback = _gbpSioProcessEvents;
	gbp->event.priority = 0x80;
}

void GBASIOPlayerReset(struct GBASIOPlayer* gbp) {
	if (gbp->p->sio.drivers.normal == &gbp->d) {
		GBASIOSetDriver(&gbp->p->sio, NULL, SIO_NORMAL_32);
	}
}

bool GBASIOPlayerCheckScreen(const struct GBAVideo* video) {
	if (memcmp(video->palette, _logoPalette, sizeof(_logoPalette)) != 0) {
		return false;
	}
	uint32_t hash = hash32(&video->renderer->vram[0x4000], 0x4000, 0);
	return hash == _logoHash;
}

void GBASIOPlayerUpdate(struct GBA* gba) {
	if (gba->memory.hw.devices & HW_GB_PLAYER) {
		if (GBASIOPlayerCheckScreen(&gba->video)) {
			++gba->sio.gbp.inputsPosted;
			gba->sio.gbp.inputsPosted %= 3;
		} else {
			gba->keyCallback = gba->sio.gbp.oldCallback;
		}
		gba->sio.gbp.txPosition = 0;
		return;
	}
	if (gba->keyCallback) {
		return;
	}
	if (GBASIOPlayerCheckScreen(&gba->video)) {
		gba->memory.hw.devices |= HW_GB_PLAYER;
		gba->sio.gbp.inputsPosted = 0;
		gba->sio.gbp.oldCallback = gba->keyCallback;
		gba->keyCallback = &gba->sio.gbp.callback.d;
		// TODO: Check if the SIO driver is actually used first
		GBASIOSetDriver(&gba->sio, &gba->sio.gbp.d, SIO_NORMAL_32);
	}
}

uint16_t _gbpRead(struct mKeyCallback* callback) {
	struct GBASIOPlayerKeyCallback* gbpCallback = (struct GBASIOPlayerKeyCallback*) callback;
	if (gbpCallback->p->inputsPosted == 2) {
		return 0xF0;
	}
	return 0;
}

uint16_t _gbpSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOPlayer* gbp = (struct GBASIOPlayer*) driver;
	if (address == REG_SIOCNT) {
		if (value & 0x0080) {
			uint32_t rx = gbp->p->memory.io[REG_SIODATA32_LO >> 1] | (gbp->p->memory.io[REG_SIODATA32_HI >> 1] << 16);
			if (gbp->txPosition < 12 && gbp->txPosition > 0) {
				// TODO: Check expected
			} else if (gbp->txPosition >= 12) {
				uint32_t mask = 0x33;
				// 0x00 = Stop
				// 0x11 = Hard Stop
				// 0x22 = Start
				if (gbp->p->rumble) {
					gbp->p->rumble->setRumble(gbp->p->rumble, (rx & mask) == 0x22);
				}
			}
			mTimingDeschedule(&gbp->p->timing, &gbp->event);
			mTimingSchedule(&gbp->p->timing, &gbp->event, 2048);
		}
		value &= 0x78FB;
	}
	return value;
}

void _gbpSioProcessEvents(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GBASIOPlayer* gbp = user;
	uint32_t tx = 0;
	int txPosition = gbp->txPosition;
	if (txPosition > 16) {
		gbp->txPosition = 0;
		txPosition = 0;
	} else if (txPosition > 12) {
		txPosition = 12;
	}
	tx = _gbpTxData[txPosition];
	++gbp->txPosition;
	gbp->p->memory.io[REG_SIODATA32_LO >> 1] = tx;
	gbp->p->memory.io[REG_SIODATA32_HI >> 1] = tx >> 16;
	if (GBASIONormalIsIrq(gbp->d.p->siocnt)) {
		GBARaiseIRQ(gbp->p, GBA_IRQ_SIO, cyclesLate);
	}
	gbp->d.p->siocnt = GBASIONormalClearStart(gbp->d.p->siocnt);
	gbp->p->memory.io[REG_SIOCNT >> 1] = gbp->d.p->siocnt & ~0x0080;
}
