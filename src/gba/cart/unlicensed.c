/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/unlicensed.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/vfs.h>

#define MULTI_SETTLE 300
#define MULTI_BLOCK 0x80000
#define MULTI_BANK 0x2000000

enum GBMulticartCfgOffset {
	GBA_MULTICART_CFG_BANK = 0x2,
	GBA_MULTICART_CFG_OFFSET = 0x3,
	GBA_MULTICART_CFG_SIZE = 0x4,
	GBA_MULTICART_CFG_SRAM = 0x5,
	GBA_MULTICART_CFG_UNK = 0x6,
};

static_assert(sizeof(((struct GBASerializedState*)(NULL))->vfame.writeSequence) ==
              sizeof(((struct GBAVFameCart*)(NULL))->writeSequence), "GBA savestate vfame writeSequence size mismatch");

static void _multicartSettle(struct mTiming* timing, void* context, uint32_t cyclesLate);

void GBAUnlCartInit(struct GBA* gba) {
	memset(&gba->memory.unl, 0, sizeof(gba->memory.unl));
}

void GBAUnlCartDetect(struct GBA* gba) {
	if (!gba->memory.rom) {
		return;
	}

	struct GBACartridge* cart = (struct GBACartridge*) gba->memory.rom;
	if (GBAVFameDetect(&gba->memory.unl.vfame, gba->memory.rom, gba->memory.romSize, gba->romCrc32)) {
		gba->memory.unl.type = GBA_UNL_CART_VFAME;
		return;
	}

	if (memcmp(&cart->id, "AXVJ01", 6) == 0 || memcmp(&cart->id, "BI3P52", 6) == 0) {
		if (gba->romVf && gba->romVf->size(gba->romVf) >= 0x04000000) {
			// Bootleg multicart
			// TODO: Identify a bit more precisely
			gba->isPristine = false;
			GBASavedataInitSRAM(&gba->memory.savedata);
			gba->memory.unl.type = GBA_UNL_CART_MULTICART;

			gba->romVf->unmap(gba->romVf, gba->memory.rom, gba->memory.romSize);
			gba->memory.unl.multi.fullSize = gba->romVf->size(gba->romVf);
			gba->memory.unl.multi.rom = gba->romVf->map(gba->romVf, gba->memory.unl.multi.fullSize, MAP_READ);
			gba->memory.rom = gba->memory.unl.multi.rom;
			gba->memory.hw.gpioBase = NULL;

			gba->memory.unl.multi.settle.context = gba;
			gba->memory.unl.multi.settle.callback = _multicartSettle;
			gba->memory.unl.multi.settle.name = "GBA Unlicensed Multicart Settle";
			gba->memory.unl.multi.settle.priority = 0x71;
		}
	}
}

void GBAUnlCartReset(struct GBA* gba) {
	if (gba->memory.unl.type == GBA_UNL_CART_MULTICART) {
		gba->memory.unl.multi.bank = 0;
		gba->memory.unl.multi.offset = 0;
		gba->memory.unl.multi.size = 0;
		gba->memory.unl.multi.locked = false;
		gba->memory.rom = gba->memory.unl.multi.rom;
		gba->memory.romSize = GBA_SIZE_ROM0;
	}
}

void GBAUnlCartUnload(struct GBA* gba) {
	if (gba->memory.unl.type == GBA_UNL_CART_MULTICART && gba->romVf) {
		gba->romVf->unmap(gba->romVf, gba->memory.unl.multi.rom, gba->memory.unl.multi.size);
		gba->memory.unl.multi.rom = NULL;
		gba->memory.rom = NULL;
	}
}

void GBAUnlCartWriteSRAM(struct GBA* gba, uint32_t address, uint8_t value) {
	struct GBAUnlCart* unl = &gba->memory.unl;

	switch (unl->type) {
	case GBA_UNL_CART_VFAME:
		GBAVFameSramWrite(&unl->vfame, address, value, gba->memory.savedata.data);
		return;
	case GBA_UNL_CART_MULTICART:
		mLOG(GBA_MEM, DEBUG, "Multicart writing SRAM %06X:%02X", address, value);
		switch (address) {
		case GBA_MULTICART_CFG_BANK:
			if (!unl->multi.locked) {
				unl->multi.bank = value >> 4;
				mTimingDeschedule(&gba->timing, &unl->multi.settle);
				mTimingSchedule(&gba->timing, &unl->multi.settle, MULTI_SETTLE);
			}
			break;
		case GBA_MULTICART_CFG_OFFSET:
			if (!unl->multi.locked) {
				unl->multi.offset = value;
				mTimingDeschedule(&gba->timing, &unl->multi.settle);
				mTimingSchedule(&gba->timing, &unl->multi.settle, MULTI_SETTLE);
				if (unl->multi.offset & 0x80) {
					unl->multi.locked = true;
				}
			}
			break;
		case GBA_MULTICART_CFG_SIZE:
			unl->multi.size = 0x40 - (value & 0x3F);
			if (!unl->multi.locked) {
				mTimingDeschedule(&gba->timing, &unl->multi.settle);
				mTimingSchedule(&gba->timing, &unl->multi.settle, MULTI_SETTLE);
			}
			break;
		case GBA_MULTICART_CFG_SRAM:
			if (value == 0 && unl->multi.sramActive) {
				unl->multi.sramActive = false;
			} else if (value == 1 && !unl->multi.sramActive) {
				unl->multi.sramActive = true;
			}
			break;
		case GBA_MULTICART_CFG_UNK:
			// TODO: What does this do?
			unl->multi.unk = value;
			break;
		default:
			break;
		}
		break;
	case GBA_UNL_CART_NONE:
		break;
	}

	gba->memory.savedata.data[address & (GBA_SIZE_SRAM - 1)] = value;
}

void GBAUnlCartWriteROM(struct GBA* gba, uint32_t address, uint16_t value) {
	struct GBAUnlCart* unl = &gba->memory.unl;

	switch (unl->type) {
	case GBA_UNL_CART_VFAME:
	case GBA_UNL_CART_NONE:
		break;
	case GBA_UNL_CART_MULTICART:
		mLOG(GBA_MEM, STUB, "Unimplemented writing to ROM %07X:%04X", address, value);
		break;
	}
}

static void _multicartSettle(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GBA* gba = context;
	mLOG(GBA_MEM, INFO, "Switching to bank %i offset %i, size %i",
	     gba->memory.unl.multi.bank, gba->memory.unl.multi.offset & 0x3F, gba->memory.unl.multi.size);
	size_t offset = gba->memory.unl.multi.bank * (MULTI_BANK >> 2) + (gba->memory.unl.multi.offset & 0x3F) * (MULTI_BLOCK >> 2);
	size_t size = gba->memory.unl.multi.size * MULTI_BLOCK;
	if (offset * 4 >= gba->memory.unl.multi.fullSize || offset * 4 + size > gba->memory.unl.multi.fullSize) {
		mLOG(GBA_MEM, GAME_ERROR, "Bank switch was out of bounds, %07" PRIz "X + %" PRIz "X > %07" PRIz "X",
		     offset * 4, size, gba->memory.unl.multi.fullSize);
		return;
	}
	gba->memory.rom = gba->memory.unl.multi.rom + offset;
	gba->memory.romSize = size;
}

void GBAUnlCartSerialize(const struct GBA* gba, struct GBASerializedState* state) {
	GBASerializedUnlCartFlags flags = 0;
	GBASerializedMulticartFlags multiFlags = 0;
	const struct GBAUnlCart* unl = &gba->memory.unl;
	switch (unl->type) {
	case GBA_UNL_CART_NONE:
		return;
	case GBA_UNL_CART_VFAME:
		flags = GBASerializedUnlCartFlagsSetType(flags, GBA_UNL_CART_VFAME);
		flags = GBASerializedUnlCartFlagsSetSubtype(flags, unl->vfame.cartType);
		STORE_16(unl->vfame.sramMode, 0, &state->vfame.sramMode);
		STORE_16(unl->vfame.romMode, 0, &state->vfame.romMode);
		memcpy(state->vfame.writeSequence, unl->vfame.writeSequence, sizeof(state->vfame.writeSequence));
		state->vfame.acceptingModeChange = unl->vfame.acceptingModeChange;
		break;
	case GBA_UNL_CART_MULTICART:
		flags = GBASerializedUnlCartFlagsSetType(0, GBA_UNL_CART_MULTICART);
		state->multicart.bank = unl->multi.bank;
		state->multicart.offset = unl->multi.offset;
		state->multicart.size = unl->multi.size;
		state->multicart.sramActive = unl->multi.sramActive;
		state->multicart.unk = unl->multi.unk;
		state->multicart.currentSize = gba->memory.romSize / MULTI_BLOCK;
		multiFlags = GBASerializedMulticartFlagsSetLocked(flags, unl->multi.locked);
		STORE_16((gba->memory.rom - unl->multi.rom) / 0x20000, 0, &state->multicart.currentOffset);
		STORE_32(unl->multi.settle.when, 0, &state->multicart.settleNextEvent);
		if (mTimingIsScheduled(&gba->timing, &unl->multi.settle)) {
			multiFlags = GBASerializedMulticartFlagsFillDustSettling(multiFlags);
		}
		STORE_32(multiFlags, 0, &state->multicart.flags);
		break;
	}
	STORE_32(flags, 0, &state->hw.unlCartFlags);
}

void GBAUnlCartDeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	GBASerializedUnlCartFlags flags;
	struct GBAUnlCart* unl = &gba->memory.unl;

	LOAD_32(flags, 0, &state->hw.unlCartFlags);
	enum GBAUnlCartType type = GBASerializedUnlCartFlagsGetType(flags);
	if (type != unl->type) {
		mLOG(GBA_STATE, WARN, "Save state expects different bootleg type; not restoring bootleg state");
		return;
	}

	uint32_t when;
	uint32_t offset;
	size_t size;
	GBASerializedMulticartFlags multiFlags;

	switch (type) {
	case GBA_UNL_CART_NONE:
		return;
	case GBA_UNL_CART_VFAME:
		LOAD_16(unl->vfame.sramMode, 0, &state->vfame.sramMode);
		LOAD_16(unl->vfame.romMode, 0, &state->vfame.romMode);
		memcpy(unl->vfame.writeSequence, state->vfame.writeSequence, sizeof(state->vfame.writeSequence));
		unl->vfame.acceptingModeChange = state->vfame.acceptingModeChange;
		return;
	case GBA_UNL_CART_MULTICART:
		unl->multi.bank = state->multicart.bank;
		unl->multi.offset = state->multicart.offset;
		unl->multi.size = state->multicart.size;
		unl->multi.sramActive = state->multicart.sramActive;
		unl->multi.unk = state->multicart.unk;
		size = state->multicart.currentSize * MULTI_BLOCK;
		LOAD_16(offset, 0, &state->multicart.currentOffset);
		offset *= 0x20000;
		if (offset * 4 >= gba->memory.unl.multi.fullSize || offset * 4 + size > gba->memory.unl.multi.fullSize) {
			mLOG(GBA_STATE, WARN, "Multicart save state has corrupted ROM offset");
		} else {
			gba->memory.romSize = size;
			gba->memory.rom = unl->multi.rom + offset;
		}
		LOAD_32(multiFlags, 0, &state->multicart.flags);
		unl->multi.locked = GBASerializedMulticartFlagsGetLocked(multiFlags);
		if (GBASerializedMulticartFlagsIsDustSettling(multiFlags)) {
			LOAD_32(when, 0, &state->multicart.settleNextEvent);
			mTimingSchedule(&gba->timing, &unl->multi.settle, when);
		}
		break;
	}
}
