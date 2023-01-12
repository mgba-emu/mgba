/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba-util/crc32.h>
#include <mgba-util/vfs.h>

const uint32_t GB_LOGO_HASH = 0x46195417;

mLOG_DEFINE_CATEGORY(GB_MBC, "GB MBC", "gb.mbc");

static void _GBMBCNone(struct GB* gb, uint16_t address, uint8_t value) {
	UNUSED(address);
	UNUSED(value);

	if (!gb->yankedRomSize) {
		mLOG(GB_MBC, GAME_ERROR, "Wrote to invalid MBC");
	}
}

void GBMBCSwitchBank(struct GB* gb, int bank) {
	size_t bankStart = bank * GB_SIZE_CART_BANK0;
	if (bankStart + GB_SIZE_CART_BANK0 > gb->memory.romSize) {
		mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid ROM bank: %0X", bank);
		bankStart &= (gb->memory.romSize - 1);
		bank = bankStart / GB_SIZE_CART_BANK0;
	}
	gb->memory.romBank = &gb->memory.rom[bankStart];
	gb->memory.currentBank = bank;
	if (gb->cpu->pc < GB_BASE_VRAM) {
		gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);
	}
}

void GBMBCSwitchBank0(struct GB* gb, int bank) {
	size_t bankStart = bank * GB_SIZE_CART_BANK0;
	if (bankStart + GB_SIZE_CART_BANK0 > gb->memory.romSize) {
		mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid ROM bank: %0X", bank);
		bankStart &= (gb->memory.romSize - 1);
	}
	gb->memory.romBase = &gb->memory.rom[bankStart];
	gb->memory.currentBank0 = bank;
	if (gb->cpu->pc < GB_SIZE_CART_BANK0) {
		gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);
	}
}

void GBMBCSwitchHalfBank(struct GB* gb, int half, int bank) {
	size_t bankStart = bank * GB_SIZE_CART_HALFBANK;
	bool isFlash = false;
	if (gb->memory.mbcType == GB_MBC6) {
		isFlash = half ? gb->memory.mbcState.mbc6.flashBank1 : gb->memory.mbcState.mbc6.flashBank0;
	}
	if (isFlash) {
		if (bankStart + GB_SIZE_CART_HALFBANK > GB_SIZE_MBC6_FLASH) {
			mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid Flash bank: %0X", bank);
			bankStart &= GB_SIZE_MBC6_FLASH - 1;
			bank = bankStart / GB_SIZE_CART_HALFBANK;
		}
		bankStart += gb->sramSize - GB_SIZE_MBC6_FLASH;
	} else {
		if (bankStart + GB_SIZE_CART_HALFBANK > gb->memory.romSize) {
			mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid ROM bank: %0X", bank);
			bankStart &= gb->memory.romSize - 1;
			bank = bankStart / GB_SIZE_CART_HALFBANK;
			if (!bank) {
				++bank;
			}
		}
	}
	if (!half) {
		if (isFlash) {
			gb->memory.romBank = &gb->memory.sram[bankStart];
		} else {
			gb->memory.romBank = &gb->memory.rom[bankStart];
		}
		gb->memory.currentBank = bank;
	} else {
		if (isFlash) {
			gb->memory.romBank1 = &gb->memory.sram[bankStart];
		} else {
			gb->memory.romBank1 = &gb->memory.rom[bankStart];
		}
		gb->memory.currentBank1 = bank;
	}
	if (gb->cpu->pc < GB_BASE_VRAM) {
		gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);
	}
}

static struct {
	const char* fourcc;
	enum GBMemoryBankControllerType mbc;
} _gbxToMbc[] = {
	{"ROM", GB_MBC_NONE},
	{"MBC1", GB_MBC1},
	{"MBC2", GB_MBC2},
	{"MBC3", GB_MBC3},
	{"MBC5", GB_MBC5},
	{"MBC6", GB_MBC6},
	{"MBC7", GB_MBC7},
	{"MB1M", GB_MBC1},
	{"MMM1", GB_MMM01},
	{"CAMR", GB_POCKETCAM},
	{"HUC1", GB_HuC1},
	{"HUC3", GB_HuC3},
	{"TAM5", GB_TAMA5},
	{"M161", GB_MBC_AUTODETECT}, // TODO
	{"BBD", GB_UNL_BBD},
	{"HITK", GB_UNL_HITEK},
	{"SNTX", GB_MBC_AUTODETECT}, // TODO
	{"NTO1", GB_UNL_NT_OLD_1},
	{"NTO2", GB_UNL_NT_OLD_2},
	{"NTN", GB_UNL_NT_NEW},
	{"LICH", GB_UNL_LI_CHENG},
	{"LBMC", GB_MBC_AUTODETECT}, // TODO
	{"LIBA", GB_MBC_AUTODETECT}, // TODO
	{"PKJD", GB_UNL_PKJD},
	{"WISD", GB_UNL_WISDOM_TREE},
	{"SAM1", GB_UNL_SACHEN_MMC1},
	{"SAM2", GB_UNL_SACHEN_MMC2},
	{"ROCK", GB_MBC_AUTODETECT}, // TODO
	{"NGHK", GB_MBC_AUTODETECT}, // TODO
	{"GB81", GB_UNL_GGB81},
	{"TPP1", GB_MBC_AUTODETECT}, // TODO

	{NULL, GB_MBC_AUTODETECT},
};

enum GBMemoryBankControllerType GBMBCFromGBX(const void* fourcc) {
	size_t i;
	for (i = 0; _gbxToMbc[i].fourcc; ++i) {
		if (memcmp(fourcc, _gbxToMbc[i].fourcc, 4) == 0) {
			break;
		}
	}
	return _gbxToMbc[i].mbc;
}

static bool _isMulticart(const uint8_t* mem) {
	bool success;
	struct VFile* vf;

	vf = VFileFromConstMemory(&mem[GB_SIZE_CART_BANK0 * 0x10], 1024);
	success = GBIsROM(vf);
	vf->close(vf);

	if (!success) {
		return false;
	}

	vf = VFileFromConstMemory(&mem[GB_SIZE_CART_BANK0 * 0x20], 1024);
	success = GBIsROM(vf);
	vf->close(vf);

	if (!success) {
		vf = VFileFromConstMemory(&mem[GB_SIZE_CART_BANK0 * 0x30], 1024);
		success = GBIsROM(vf);
		vf->close(vf);
	}
	
	return success;
}

static bool _isWisdomTree(const uint8_t* mem, size_t size) {
	size_t i;
	for (i = 0x134; i < 0x14C; i += 4) {
		if (*(uint32_t*) &mem[i] != 0) {
			return false;
		}
	}
	for (i = 0xF0; i < 0x100; i += 4) {
		if (*(uint32_t*) &mem[i] != 0) {
			return false;
		}
	}
	if (mem[0x14D] != 0xE7) {
		return false;
	}
	for (i = 0x300; i < size - 11; ++i) {
		if (memcmp(&mem[i], "WISDOM", 6) == 0 && memcmp(&mem[i + 7], "TREE", 4) == 0) {
			return true;
		}
	}
	return false;
}

static enum GBMemoryBankControllerType _detectUnlMBC(const uint8_t* mem, size_t size) {
	const struct GBCartridge* cart = (const struct GBCartridge*) &mem[0x100];

	switch (cart->type) {
		case 0:
		if (_isWisdomTree(mem, size)) {
			return GB_UNL_WISDOM_TREE;
		}
		break;
	}

	uint32_t secondaryLogo = doCrc32(&mem[0x184], 0x30);
	switch (secondaryLogo) {
	case 0x4fdab691:
		return GB_UNL_HITEK;
	case 0xc7d8c1df:
	case 0x6d1ea662: // Garou
		if (mem[0x7FFF] != 0x01) { // Make sure we're not using a "fixed" version
			return GB_UNL_BBD;
		}
		break;
	case 0x79f34594: // DATA.
	case 0x7e8c539b: // TD-SOFT
		return GB_UNL_GGB81;
	case 0x20d092e2:
	case 0xd2b57657:
		if (cart->type == 0x01) { // Make sure we're not using a "fixed" version
			return GB_UNL_LI_CHENG;
		}
		if ((0x8000 << cart->romSize) != size) {
			return GB_UNL_LI_CHENG;
		}
		break;
	}

	if (mem[0x104] == 0xCE && mem[0x144] == 0xED && mem[0x114] == 0x66) {
		return GB_UNL_SACHEN_MMC1;
	}

	if (mem[0x184] == 0xCE && mem[0x1C4] == 0xED && mem[0x194] == 0x66) {
		return GB_UNL_SACHEN_MMC2;
	}

	return GB_MBC_AUTODETECT;
}

void GBMBCSwitchSramBank(struct GB* gb, int bank) {
	size_t bankStart = bank * GB_SIZE_EXTERNAL_RAM;
	if (bankStart + GB_SIZE_EXTERNAL_RAM > gb->sramSize) {
		mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid RAM bank: %0X", bank);
		bankStart &= (gb->sramSize - 1);
		bank = bankStart / GB_SIZE_EXTERNAL_RAM;
	}
	gb->memory.sramBank = &gb->memory.sram[bankStart];
	gb->memory.sramCurrentBank = bank;
}

void GBMBCSwitchSramHalfBank(struct GB* gb, int half, int bank) {
	size_t bankStart = bank * GB_SIZE_EXTERNAL_RAM_HALFBANK;
	size_t sramSize = gb->sramSize - GB_SIZE_MBC6_FLASH;
	if (bankStart + GB_SIZE_EXTERNAL_RAM_HALFBANK > sramSize) {
		mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid RAM bank: %0X", bank);
		bankStart &= (sramSize - 1);
		bank = bankStart / GB_SIZE_EXTERNAL_RAM_HALFBANK;
	}
	if (!half) {
		gb->memory.sramBank = &gb->memory.sram[bankStart];
		gb->memory.sramCurrentBank = bank;
	} else {
		gb->memory.sramBank1 = &gb->memory.sram[bankStart];
		gb->memory.currentSramBank1 = bank;
	}
}

void GBMBCInit(struct GB* gb) {
	const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	if (gb->memory.rom && gb->memory.romSize) {
		if (gb->memory.romSize >= 0x8000) {
			const struct GBCartridge* cartFooter = (const struct GBCartridge*) &gb->memory.rom[gb->memory.romSize - 0x7F00];
			if (doCrc32(cartFooter->logo, sizeof(cartFooter->logo)) == GB_LOGO_HASH && cartFooter->type >= 0x0B && cartFooter->type <= 0x0D) {
				cart = cartFooter;
			}
		}
		if (gb->gbx.romSize) {
			gb->sramSize = gb->gbx.ramSize;
			gb->memory.mbcType = gb->gbx.mbc;
		} else {
			switch (cart->ramSize) {
			case 0:
				gb->sramSize = 0;
				break;
			default:
			case 2:
				gb->sramSize = 0x2000;
				break;
			case 3:
				gb->sramSize = 0x8000;
				break;
			case 4:
				gb->sramSize = 0x20000;
				break;
			case 5:
				gb->sramSize = 0x10000;
				break;
			}
		}
		if (gb->memory.mbcType == GB_MBC_AUTODETECT) {
			gb->memory.mbcType = _detectUnlMBC(gb->memory.rom, gb->memory.romSize);
		}

		if (gb->memory.mbcType == GB_MBC_AUTODETECT) {
			switch (cart->type) {
			case 0:
			case 8:
			case 9:
				gb->memory.mbcType = GB_MBC_NONE;
				break;
			case 1:
			case 2:
			case 3:
				gb->memory.mbcType = GB_MBC1;
				break;
			case 5:
			case 6:
				gb->memory.mbcType = GB_MBC2;
				break;
			case 0x0B:
			case 0x0C:
			case 0x0D:
				gb->memory.mbcType = GB_MMM01;
				break;
			case 0x0F:
			case 0x10:
				gb->memory.mbcType = GB_MBC3_RTC;
				break;
			case 0x11:
			case 0x12:
			case 0x13:
				gb->memory.mbcType = GB_MBC3;
				break;
			default:
				mLOG(GB_MBC, WARN, "Unknown MBC type: %02X", cart->type);
				// Fall through
			case 0x19:
			case 0x1A:
			case 0x1B:
				gb->memory.mbcType = GB_MBC5;
				break;
			case 0x1C:
			case 0x1D:
			case 0x1E:
				gb->memory.mbcType = GB_MBC5_RUMBLE;
				break;
			case 0x20:
				gb->memory.mbcType = GB_MBC6;
				break;
			case 0x22:
				gb->memory.mbcType = GB_MBC7;
				break;
			case 0xFC:
				gb->memory.mbcType = GB_POCKETCAM;
				break;
			case 0xFD:
				gb->memory.mbcType = GB_TAMA5;
				break;
			case 0xFE:
				gb->memory.mbcType = GB_HuC3;
				break;
			case 0xFF:
				gb->memory.mbcType = GB_HuC1;
				break;
			}
		}
	} else {
		gb->memory.mbcType = GB_MBC_NONE;
	}
	gb->memory.mbcRead = NULL;
	gb->memory.directSramAccess = true;
	gb->memory.mbcReadBank0 = false;
	gb->memory.mbcReadBank1 = false;
	gb->memory.mbcReadHigh = false;
	gb->memory.mbcWriteHigh = false;
	gb->memory.cartBusDecay = 4;
	switch (gb->memory.mbcType) {
	case GB_MBC_NONE:
		gb->memory.mbcWrite = _GBMBCNone;
		break;
	case GB_MBC1:
		gb->memory.mbcWrite = _GBMBC1;
		if (gb->gbx.mapperVars.u8[0]) {
			gb->memory.mbcState.mbc1.multicartStride = gb->gbx.mapperVars.u8[0];
		} else if (gb->memory.romSize >= GB_SIZE_CART_BANK0 * 0x31 && _isMulticart(gb->memory.rom)) {
			gb->memory.mbcState.mbc1.multicartStride = 4;
		} else {
			gb->memory.mbcState.mbc1.multicartStride = 5;
		}
		break;
	case GB_MBC2:
		gb->memory.mbcWrite = _GBMBC2;
		gb->memory.mbcRead = _GBMBC2Read;
		gb->memory.directSramAccess = false;
		gb->sramSize = 0x100;
		break;
	case GB_MBC3:
		gb->memory.mbcWrite = _GBMBC3;
		break;
	default:
		mLOG(GB_MBC, WARN, "Unknown MBC type: %02X", cart->type);
		// Fall through
	case GB_MBC5:
		gb->memory.mbcWrite = _GBMBC5;
		break;
	case GB_MBC6:
		gb->memory.mbcWrite = _GBMBC6;
		gb->memory.mbcRead = _GBMBC6Read;
		gb->memory.directSramAccess = false;
		if (!gb->sramSize) {
			gb->sramSize = GB_SIZE_EXTERNAL_RAM; // Force minimum size for convenience
		}
		gb->sramSize += GB_SIZE_MBC6_FLASH; // Flash is concatenated at the end
		break;
	case GB_MBC7:
		gb->memory.mbcWrite = _GBMBC7;
		gb->memory.mbcRead = _GBMBC7Read;
		gb->sramSize = 0x100;
		break;
	case GB_MMM01:
		gb->memory.mbcWrite = _GBMMM01;
		break;
	case GB_HuC1:
		gb->memory.mbcWrite = _GBHuC1;
		break;
	case GB_HuC3:
		gb->memory.mbcWrite = _GBHuC3;
		gb->memory.mbcRead = _GBHuC3Read;
		break;
	case GB_TAMA5:
		gb->memory.mbcWrite = _GBTAMA5;
		gb->memory.mbcRead = _GBTAMA5Read;
		gb->memory.mbcState.tama5.rtcAlarmPage[GBTAMA6_RTC_PAGE] = 1;
		gb->memory.mbcState.tama5.rtcFreePage0[GBTAMA6_RTC_PAGE] = 2;
		gb->memory.mbcState.tama5.rtcFreePage1[GBTAMA6_RTC_PAGE] = 3;
		gb->sramSize = 0x20;
		break;
	case GB_MBC3_RTC:
		memset(gb->memory.rtcRegs, 0, sizeof(gb->memory.rtcRegs));
		gb->memory.mbcWrite = _GBMBC3;
		break;
	case GB_MBC5_RUMBLE:
		gb->memory.mbcWrite = _GBMBC5;
		break;
	case GB_POCKETCAM:
		gb->memory.mbcWrite = _GBPocketCam;
		gb->memory.mbcRead = _GBPocketCamRead;
		if (!gb->sramSize) {
			gb->sramSize = GB_SIZE_EXTERNAL_RAM; // Force minimum size for convenience
		}
		if (gb->memory.cam && gb->memory.cam->startRequestImage) {
			gb->memory.cam->startRequestImage(gb->memory.cam, GBCAM_WIDTH, GBCAM_HEIGHT, mCOLOR_ANY);
		}
		break;
	case GB_UNL_WISDOM_TREE:
		gb->memory.mbcWrite = _GBWisdomTree;
		break;
	case GB_UNL_NT_OLD_1:
		gb->memory.mbcWrite = _GBNTOld1;
		break;
	case GB_UNL_NT_OLD_2:
		gb->memory.mbcWrite = _GBNTOld2;
		break;
	case GB_UNL_NT_NEW:
		gb->memory.mbcWrite = _GBNTNew;
		break;
	case GB_UNL_PKJD:
		gb->memory.mbcWrite = _GBPKJD;
		gb->memory.mbcRead = _GBPKJDRead;
		break;
	case GB_UNL_BBD:
		gb->memory.mbcWrite = _GBBBD;
		gb->memory.mbcRead = _GBBBDRead;
		gb->memory.mbcReadBank1 = true;
		break;
	case GB_UNL_HITEK:
		gb->memory.mbcWrite = _GBHitek;
		gb->memory.mbcRead = _GBHitekRead;
		gb->memory.mbcState.bbd.dataSwapMode = 7;
		gb->memory.mbcState.bbd.bankSwapMode = 7;
		gb->memory.mbcReadBank1 = true;
		break;
	case GB_UNL_LI_CHENG:
		gb->memory.mbcWrite = _GBLiCheng;
		break;
	case GB_UNL_GGB81:
		gb->memory.mbcWrite = _GBGGB81;
		gb->memory.mbcRead = _GBGGB81Read;
		gb->memory.mbcReadBank1 = true;
		break;
	case GB_UNL_SACHEN_MMC1:
		gb->memory.mbcWrite = _GBSachen;
		gb->memory.mbcRead = _GBSachenMMC1Read;
		gb->memory.mbcReadBank0 = true;
		gb->memory.mbcReadBank1 = true;
		break;
	case GB_UNL_SACHEN_MMC2:
		gb->memory.mbcWrite = _GBSachen;
		gb->memory.mbcRead = _GBSachenMMC2Read;
		gb->memory.mbcReadBank0 = true;
		gb->memory.mbcReadBank1 = true;
		gb->memory.mbcReadHigh = true;
		gb->memory.mbcWriteHigh = true;
		if (gb->sramSize) {
			gb->memory.sramAccess = true;
		}
		break;
	}

	gb->memory.currentBank = 1;
	gb->memory.sramCurrentBank = 0;
	gb->memory.sramAccess = false;
	gb->memory.rtcAccess = false;
	gb->memory.activeRtcReg = 0;
	gb->memory.rtcLatched = false;
	gb->memory.rtcLastLatch = 0;
	if (gb->memory.rtc) {
		if (gb->memory.rtc->sample) {
			gb->memory.rtc->sample(gb->memory.rtc);
		}
		gb->memory.rtcLastLatch = gb->memory.rtc->unixTime(gb->memory.rtc);
	} else {
		gb->memory.rtcLastLatch = time(0);
	}
	memset(&gb->memory.rtcRegs, 0, sizeof(gb->memory.rtcRegs));

	GBResizeSram(gb, gb->sramSize);

	if (gb->memory.mbcType == GB_MBC3_RTC) {
		GBMBCRTCRead(gb);
	} else if (gb->memory.mbcType == GB_HuC3) {
		GBMBCHuC3Read(gb);
	} else if (gb->memory.mbcType == GB_TAMA5) {
		GBMBCTAMA5Read(gb);
	}
}

void GBMBCReset(struct GB* gb) {
	gb->memory.currentBank0 = 0;
	gb->memory.romBank = &gb->memory.rom[GB_SIZE_CART_BANK0];
	gb->memory.cartBus = 0xFF;
	gb->memory.cartBusPc = 0;
	gb->memory.cartBusDecay = 1;

	memset(&gb->memory.mbcState, 0, sizeof(gb->memory.mbcState));
	GBMBCInit(gb);
	switch (gb->memory.mbcType) {
	case GB_MBC1:
		gb->memory.mbcState.mbc1.mode = 0;
		gb->memory.mbcState.mbc1.bankLo = 1;
		break;
	case GB_MBC6:
		GBMBCSwitchHalfBank(gb, 0, 2);
		GBMBCSwitchHalfBank(gb, 1, 3);
		GBMBCSwitchSramHalfBank(gb, 0, 0);
		GBMBCSwitchSramHalfBank(gb, 0, 1);
		break;
	case GB_MMM01:
		GBMBCSwitchBank0(gb, gb->memory.romSize / GB_SIZE_CART_BANK0 - 2);
		GBMBCSwitchBank(gb, gb->memory.romSize / GB_SIZE_CART_BANK0 - 1);
		break;
	default:
		break;
	}
	gb->memory.sramBank = gb->memory.sram;
}

void _GBMBCAppendSaveSuffix(struct GB* gb, const void* buffer, size_t size) {
	struct VFile* vf = gb->sramVf;
	if ((size_t) vf->size(vf) < gb->sramSize + size) {
		// Writing past the end of the file can invalidate the file mapping
		vf->unmap(vf, gb->memory.sram, gb->sramSize);
		gb->memory.sram = NULL;
	}
	vf->seek(vf, gb->sramSize, SEEK_SET);
	vf->write(vf, buffer, size);
	if (!gb->memory.sram) {
		gb->memory.sram = vf->map(vf, gb->sramSize, MAP_WRITE);
		GBMBCSwitchSramBank(gb, gb->memory.sramCurrentBank);
	}
}

void GBMBCRTCRead(struct GB* gb) {
	struct GBMBCRTCSaveBuffer rtcBuffer;
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}
	vf->seek(vf, gb->sramSize, SEEK_SET);
	if (vf->read(vf, &rtcBuffer, sizeof(rtcBuffer)) < (ssize_t) sizeof(rtcBuffer) - 4) {
		return;
	}

	LOAD_32LE(gb->memory.rtcRegs[0], 0, &rtcBuffer.latchedSec);
	LOAD_32LE(gb->memory.rtcRegs[1], 0, &rtcBuffer.latchedMin);
	LOAD_32LE(gb->memory.rtcRegs[2], 0, &rtcBuffer.latchedHour);
	LOAD_32LE(gb->memory.rtcRegs[3], 0, &rtcBuffer.latchedDays);
	LOAD_32LE(gb->memory.rtcRegs[4], 0, &rtcBuffer.latchedDaysHi);
	LOAD_64LE(gb->memory.rtcLastLatch, 0, &rtcBuffer.unixTime);
}

void GBMBCRTCWrite(struct GB* gb) {
	struct VFile* vf = gb->sramVf;
	if (!vf) {
		return;
	}

	uint8_t rtcRegs[5];
	memcpy(rtcRegs, gb->memory.rtcRegs, sizeof(rtcRegs));
	time_t rtcLastLatch = gb->memory.rtcLastLatch;
	_GBMBCLatchRTC(gb->memory.rtc, rtcRegs, &rtcLastLatch);

	struct GBMBCRTCSaveBuffer rtcBuffer;
	STORE_32LE(rtcRegs[0], 0, &rtcBuffer.sec);
	STORE_32LE(rtcRegs[1], 0, &rtcBuffer.min);
	STORE_32LE(rtcRegs[2], 0, &rtcBuffer.hour);
	STORE_32LE(rtcRegs[3], 0, &rtcBuffer.days);
	STORE_32LE(rtcRegs[4], 0, &rtcBuffer.daysHi);
	STORE_32LE(gb->memory.rtcRegs[0], 0, &rtcBuffer.latchedSec);
	STORE_32LE(gb->memory.rtcRegs[1], 0, &rtcBuffer.latchedMin);
	STORE_32LE(gb->memory.rtcRegs[2], 0, &rtcBuffer.latchedHour);
	STORE_32LE(gb->memory.rtcRegs[3], 0, &rtcBuffer.latchedDays);
	STORE_32LE(gb->memory.rtcRegs[4], 0, &rtcBuffer.latchedDaysHi);
	STORE_64LE(gb->memory.rtcLastLatch, 0, &rtcBuffer.unixTime);

	_GBMBCAppendSaveSuffix(gb, &rtcBuffer, sizeof(rtcBuffer));
}
