/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/gb.h>

#include <mgba/internal/defines.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/sm83/sm83.h>

#include <mgba/core/core.h>
#include <mgba/core/cheats.h>
#include <mgba-util/crc32.h>
#include <mgba-util/memory.h>
#include <mgba-util/math.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

const uint32_t CGB_SM83_FREQUENCY = 0x800000;
const uint32_t SGB_SM83_FREQUENCY = 0x418B1E;

const uint32_t GB_COMPONENT_MAGIC = 0x400000;

static const uint8_t _knownHeader[4] = {0xCE, 0xED, 0x66, 0x66};
static const uint8_t _knownHeaderSachen[4] = {0x7C, 0xE7, 0xC0, 0x00};
static const uint8_t _registeredTrademark[] = {0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C};

#define DMG0_BIOS_CHECKSUM 0xC2F5CC97
#define DMG_BIOS_CHECKSUM 0x59C8598E
#define MGB_BIOS_CHECKSUM 0xE6920754
#define SGB_BIOS_CHECKSUM 0xEC8A83B9
#define SGB2_BIOS_CHECKSUM 0X53D0DD63
#define CGB_BIOS_CHECKSUM 0x41884E46
#define CGB0_BIOS_CHECKSUM 0xE8EF5318
#define AGB_BIOS_CHECKSUM 0xFFD6B0F1

mLOG_DEFINE_CATEGORY(GB, "GB", "gb");

static void GBInit(void* cpu, struct mCPUComponent* component);
static void GBDeinit(struct mCPUComponent* component);
static void GBInterruptHandlerInit(struct SM83InterruptHandler* irqh);
static void GBProcessEvents(struct SM83Core* cpu);
static void GBSetInterrupts(struct SM83Core* cpu, bool enable);
static uint16_t GBIRQVector(struct SM83Core* cpu);
static void GBIllegal(struct SM83Core* cpu);
static void GBStop(struct SM83Core* cpu);

static void _enableInterrupts(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBCreate(struct GB* gb) {
	gb->d.id = GB_COMPONENT_MAGIC;
	gb->d.init = GBInit;
	gb->d.deinit = GBDeinit;
}

static void GBInit(void* cpu, struct mCPUComponent* component) {
	struct GB* gb = (struct GB*) component;
	gb->cpu = cpu;
	gb->sync = NULL;

	GBInterruptHandlerInit(&gb->cpu->irqh);
	GBMemoryInit(gb);

	gb->video.p = gb;
	GBVideoInit(&gb->video);

	gb->audio.p = gb;
	GBAudioInit(&gb->audio, 2048, &gb->memory.io[GB_REG_NR52], GB_AUDIO_DMG); // TODO: Remove magic constant

	gb->sio.p = gb;
	GBSIOInit(&gb->sio);

	gb->timer.p = gb;

	gb->model = GB_MODEL_AUTODETECT;

	gb->biosVf = NULL;
	gb->romVf = NULL;
	gb->sramVf = NULL;
	gb->sramRealVf = NULL;

	gb->isPristine = false;
	gb->pristineRomSize = 0;
	gb->yankedRomSize = 0;

	memset(&gb->gbx, 0, sizeof(gb->gbx));

	mCoreCallbacksListInit(&gb->coreCallbacks, 0);
	gb->stream = NULL;

	mTimingInit(&gb->timing, &gb->cpu->cycles, &gb->cpu->nextEvent);
	gb->audio.timing = &gb->timing;

	gb->eiPending.name = "GB EI";
	gb->eiPending.callback = _enableInterrupts;
	gb->eiPending.context = gb;
	gb->eiPending.priority = 0;
}

static void GBDeinit(struct mCPUComponent* component) {
	struct GB* gb = (struct GB*) component;
	mTimingDeinit(&gb->timing);
}

bool GBLoadGBX(struct GBXMetadata* metadata, struct VFile* vf) {
	uint8_t footer[16];
	if (vf->seek(vf, -sizeof(footer), SEEK_END) < 0) {
		return false;
	}
	if (vf->read(vf, footer, sizeof(footer)) < (ssize_t) sizeof(footer)) {
		return false;
	}
	int32_t gbxSize = 0;
	uint32_t vers;
	LOAD_32BE(gbxSize, 0, footer);
	LOAD_32BE(vers, 4, footer);
	if (memcmp(&footer[12], "GBX!", 4) != 0 || gbxSize != 0x40 || vers != 1) {
		return false;
	}
	if (vf->seek(vf, -gbxSize, SEEK_END) < 0) {
		return false;
	}
	if (vf->read(vf, footer, sizeof(footer)) != (ssize_t) sizeof(footer)) {
		return false;
	}
	memset(metadata, 0, sizeof(*metadata));
	metadata->mbc = GBMBCFromGBX(footer);

	if (footer[4] == 1) {
		metadata->battery = true;
	}
	if (footer[5] == 1) {
		metadata->rumble = true;
		if (metadata->mbc == GB_MBC5) {
			metadata->mbc = GB_MBC5_RUMBLE;
		}
	}
	if (footer[6] == 1) {
		metadata->timer = true;
		if (metadata->mbc == GB_MBC3) {
			metadata->mbc = GB_MBC3_RTC;
		}
	}
	LOAD_32BE(metadata->romSize, 8, footer);
	LOAD_32BE(metadata->ramSize, 12, footer);
	vf->read(vf, &metadata->mapperVars, 0x20);

	// There's no dedicated mapper type for MBC1M so let's stash some data here
	if (memcmp(footer, "MBC1", 4) == 0) {
		metadata->mapperVars.u8[0] = 5;
	} else if (memcmp(footer, "MB1M", 4) == 0) {
		metadata->mapperVars.u8[0] = 4;		
	}
	return true;
}

bool GBLoadROM(struct GB* gb, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	GBUnloadROM(gb);

	if (!GBLoadGBX(&gb->gbx, vf)) {
		// GBX handles the pristine size itself, but other formats don't
		gb->pristineRomSize = vf->size(vf);
	} else {
		uint32_t fileSize = vf->size(vf);
		if (gb->gbx.romSize <= fileSize - 0x40) {
			gb->pristineRomSize = gb->gbx.romSize;
		} else {
			// TODO: Should we make a temporary buffer?
			mLOG(GB, WARN, "GBX file size %d is larger than real file size %d", gb->gbx.romSize, fileSize - 0x40);
			gb->pristineRomSize = fileSize - 0x40;
		}
	}

	gb->romVf = vf;
	vf->seek(vf, 0, SEEK_SET);
	gb->isPristine = true;
	gb->memory.rom = vf->map(vf, gb->pristineRomSize, MAP_READ);
	if (!gb->memory.rom) {
		return false;
	}
	gb->yankedRomSize = 0;
	gb->memory.romSize = gb->pristineRomSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);
	GBMBCReset(gb);

	if (gb->cpu) {
		struct SM83Core* cpu = gb->cpu;
		if (!gb->memory.romBase) {
			GBMBCSwitchBank0(gb, 0);
		}
		cpu->memory.setActiveRegion(cpu, cpu->pc);
	}

	// TODO: error check
	return true;
}

void GBYankROM(struct GB* gb) {
	gb->yankedRomSize = gb->memory.romSize;
	gb->yankedMbc = gb->memory.mbcType;
	gb->memory.romSize = 0;
	gb->memory.mbcType = GB_MBC_NONE;
	GBMBCReset(gb);

	if (gb->cpu) {
		struct SM83Core* cpu = gb->cpu;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
	}
}

static void GBSramDeinit(struct GB* gb) {
	if (gb->sramVf) {
		gb->sramVf->unmap(gb->sramVf, gb->memory.sram, gb->sramSize);
		if (gb->sramVf == gb->sramRealVf) {
			if (gb->memory.mbcType == GB_MBC3_RTC) {
				GBMBCRTCWrite(gb);
			} else if (gb->memory.mbcType == GB_HuC3) {
				GBMBCHuC3Write(gb);
			} else if (gb->memory.mbcType == GB_TAMA5) {
				GBMBCTAMA5Write(gb);
			}
		}
		gb->sramVf = NULL;
	} else if (gb->memory.sram) {
		mappedMemoryFree(gb->memory.sram, gb->sramSize);
	}
	gb->memory.sram = 0;
}

bool GBLoadSave(struct GB* gb, struct VFile* vf) {
	GBSramDeinit(gb);
	gb->sramVf = vf;
	if (gb->sramRealVf && gb->sramRealVf != vf) {
		gb->sramRealVf->close(gb->sramRealVf);
	}
	gb->sramRealVf = vf;
	if (gb->sramSize) {
		GBResizeSram(gb, gb->sramSize);
		GBMBCSwitchSramBank(gb, gb->memory.sramCurrentBank);

		if (gb->memory.mbcType == GB_MBC3_RTC) {
			GBMBCRTCRead(gb);
		} else if (gb->memory.mbcType == GB_HuC3) {
			GBMBCHuC3Read(gb);
		} else if (gb->memory.mbcType == GB_TAMA5) {
			GBMBCTAMA5Read(gb);
		}
	}
	return vf;
}

void GBResizeSram(struct GB* gb, size_t size) {
	if (gb->memory.sram && size <= gb->sramSize) {
		return;
	}
	struct VFile* vf = gb->sramVf;
	if (vf) {
		if (vf == gb->sramRealVf) {
			ssize_t vfSize = vf->size(vf);
			if (vfSize >= 0 && (size_t) vfSize < size) {
				uint8_t extdataBuffer[0x100];
				if (vfSize & 0xFF) {
					vf->seek(vf, -(vfSize & 0xFF), SEEK_END);
					vf->read(vf, extdataBuffer, vfSize & 0xFF);
				}
				if (gb->memory.sram) {
					vf->unmap(vf, gb->memory.sram, gb->sramSize);
				}
				vf->truncate(vf, size + (vfSize & 0xFF));
				if (vfSize & 0xFF) {
					vf->seek(vf, size, SEEK_SET);
					vf->write(vf, extdataBuffer, vfSize & 0xFF);
				}
				if (size) {
					gb->memory.sram = vf->map(vf, size, MAP_WRITE);
					memset(&gb->memory.sram[vfSize], 0xFF, size - vfSize);
				}
			} else if (size > gb->sramSize || !gb->memory.sram) {
				if (gb->memory.sram) {
					vf->unmap(vf, gb->memory.sram, gb->sramSize);
				}
				if (size) {
					gb->memory.sram = vf->map(vf, size, MAP_WRITE);
				}
			}
		} else {
			if (gb->memory.sram) {
				vf->unmap(vf, gb->memory.sram, gb->sramSize);
			}
			if (vf->size(vf) < gb->sramSize) {
				void* sram = vf->map(vf, vf->size(vf), MAP_READ);
				struct VFile* newVf = VFileMemChunk(sram, vf->size(vf));
				vf->unmap(vf, sram,vf->size(vf));
				vf = newVf;
				gb->sramVf = newVf;
				vf->truncate(vf, size);
			}
			if (size) {
				gb->memory.sram = vf->map(vf, size, MAP_READ);
			}
		}
		if (!size || gb->memory.sram == (void*) -1) {
			gb->memory.sram = NULL;
		}
	} else if (size) {
		uint8_t* newSram = anonymousMemoryMap(size);
		if (gb->memory.sram) {
			if (size > gb->sramSize) {
				memcpy(newSram, gb->memory.sram, gb->sramSize);
				memset(&newSram[gb->sramSize], 0xFF, size - gb->sramSize);
			} else {
				memcpy(newSram, gb->memory.sram, size);
			}
			mappedMemoryFree(gb->memory.sram, gb->sramSize);
		} else {
			memset(newSram, 0xFF, size);
		}
		gb->memory.sram = newSram;
	}
	if (gb->sramSize < size) {
		gb->sramSize = size;
	}
}

void GBSramClean(struct GB* gb, uint32_t frameCount) {
	// TODO: Share with GBASavedataClean
	if (!gb->sramVf) {
		return;
	}
	if (mSavedataClean(&gb->sramDirty, &gb->sramDirtAge, frameCount)) {
		if (gb->sramMaskWriteback) {
			GBSavedataUnmask(gb);
		}
		if (gb->memory.mbcType == GB_MBC3_RTC) {
			GBMBCRTCWrite(gb);
		} else if (gb->memory.mbcType == GB_HuC3) {
			GBMBCHuC3Write(gb);
		} else if (gb->memory.mbcType == GB_TAMA5) {
			GBMBCTAMA5Write(gb);
		}
		if (gb->sramVf == gb->sramRealVf) {
			if (gb->memory.sram && gb->sramVf->sync(gb->sramVf, gb->memory.sram, gb->sramSize)) {
				mLOG(GB_MEM, INFO, "Savedata synced");
			} else {
				mLOG(GB_MEM, INFO, "Savedata failed to sync!");
			}
		}

		size_t c;
		for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
			struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
			if (callbacks->savedataUpdated) {
				callbacks->savedataUpdated(callbacks->context);
			}
		}
	}
}

void GBSavedataMask(struct GB* gb, struct VFile* vf, bool writeback) {
	struct VFile* oldVf = gb->sramVf;
	GBSramDeinit(gb);
	if (oldVf && oldVf != gb->sramRealVf) {
		oldVf->close(oldVf);
	}
	gb->sramVf = vf;
	gb->sramMaskWriteback = writeback;
	GBResizeSram(gb, gb->sramSize);
	GBMBCSwitchSramBank(gb, gb->memory.sramCurrentBank);
}

void GBSavedataUnmask(struct GB* gb) {
	if (!gb->sramRealVf || gb->sramVf == gb->sramRealVf) {
		return;
	}
	struct VFile* vf = gb->sramVf;
	GBSramDeinit(gb);
	gb->sramVf = gb->sramRealVf;
	gb->memory.sram = gb->sramVf->map(gb->sramVf, gb->sramSize, MAP_WRITE);
	if (gb->sramMaskWriteback) {
		vf->seek(vf, 0, SEEK_SET);
		vf->read(vf, gb->memory.sram, gb->sramSize);
		gb->sramMaskWriteback = false;
	}
	GBMBCSwitchSramBank(gb, gb->memory.sramCurrentBank);
	vf->close(vf);
}

void GBUnloadROM(struct GB* gb) {
	// TODO: Share with GBAUnloadROM
	off_t romBase = gb->memory.romBase - gb->memory.rom;
	if (romBase >= 0 && ((size_t) romBase < gb->memory.romSize || (size_t) romBase < gb->yankedRomSize)) {
		gb->memory.romBase = NULL;
	}
	if (gb->memory.rom && !gb->isPristine) {
		if (gb->yankedRomSize) {
			gb->yankedRomSize = 0;
		}
		mappedMemoryFree(gb->memory.rom, GB_SIZE_CART_MAX);
	}

	if (gb->romVf) {
#ifndef FIXED_ROM_BUFFER
		if (gb->isPristine && gb->memory.rom) {
			gb->romVf->unmap(gb->romVf, gb->memory.rom, gb->pristineRomSize);
		}
#endif
		gb->romVf->close(gb->romVf);
		gb->romVf = NULL;
	}
	gb->memory.rom = NULL;
	gb->memory.mbcType = GB_MBC_AUTODETECT;
	gb->isPristine = false;

	if (!gb->sramDirty) {
		gb->sramMaskWriteback = false;
	}
	GBSavedataUnmask(gb);
	GBSramDeinit(gb);
	if (gb->sramRealVf) {
		gb->sramRealVf->close(gb->sramRealVf);
	}
	gb->sramRealVf = NULL;
	gb->sramVf = NULL;
	if (gb->memory.cam && gb->memory.cam->stopRequestImage) {
		gb->memory.cam->stopRequestImage(gb->memory.cam);
	}
}

void GBSynthesizeROM(struct VFile* vf) {
	if (!vf) {
		return;
	}
	const struct GBCartridge cart = {
		.logo = { _knownHeader[0], _knownHeader[1], _knownHeader[2], _knownHeader[3]}
	};

	vf->seek(vf, 0x100, SEEK_SET);
	vf->write(vf, &cart, sizeof(cart));
}

void GBLoadBIOS(struct GB* gb, struct VFile* vf) {
	gb->biosVf = vf;
}

void GBApplyPatch(struct GB* gb, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gb->memory.romSize);
	if (!patchedSize) {
		return;
	}
	if (patchedSize > GB_SIZE_CART_MAX) {
		patchedSize = GB_SIZE_CART_MAX;
	}
	void* newRom = anonymousMemoryMap(GB_SIZE_CART_MAX);
	if (!patch->applyPatch(patch, gb->memory.rom, gb->pristineRomSize, newRom, patchedSize)) {
		mappedMemoryFree(newRom, GB_SIZE_CART_MAX);
		return;
	}
	if (gb->romVf) {
#ifndef FIXED_ROM_BUFFER
		gb->romVf->unmap(gb->romVf, gb->memory.rom, gb->pristineRomSize);
#endif
		gb->romVf->close(gb->romVf);
		gb->romVf = NULL;
	}
	gb->isPristine = false;
	if (gb->memory.romBase == gb->memory.rom) {
		gb->memory.romBase = newRom;
	}
	gb->memory.rom = newRom;
	gb->memory.romSize = patchedSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);
	gb->cpu->memory.setActiveRegion(gb->cpu, gb->cpu->pc);
}

void GBDestroy(struct GB* gb) {
	GBUnmapBIOS(gb);
	GBUnloadROM(gb);

	if (gb->biosVf) {
		gb->biosVf->close(gb->biosVf);
		gb->biosVf = 0;
	}

	GBMemoryDeinit(gb);
	GBAudioDeinit(&gb->audio);
	GBVideoDeinit(&gb->video);
	GBSIODeinit(&gb->sio);
	mCoreCallbacksListDeinit(&gb->coreCallbacks);
}

void GBInterruptHandlerInit(struct SM83InterruptHandler* irqh) {
	irqh->reset = GBReset;
	irqh->processEvents = GBProcessEvents;
	irqh->setInterrupts = GBSetInterrupts;
	irqh->irqVector = GBIRQVector;
	irqh->hitIllegal = GBIllegal;
	irqh->stop = GBStop;
	irqh->halt = GBHalt;
}

static uint32_t _GBBiosCRC32(struct VFile* vf) {
	ssize_t size = vf->size(vf);
	if (size <= 0 || size > GB_SIZE_CART_BANK0) {
		return 0;
	}
	void* bios = vf->map(vf, size, MAP_READ);
	uint32_t biosCrc = doCrc32(bios, size);
	vf->unmap(vf, bios, size);
	return biosCrc;
}

bool GBIsBIOS(struct VFile* vf) {
	switch (_GBBiosCRC32(vf)) {
	case DMG_BIOS_CHECKSUM:
	case DMG0_BIOS_CHECKSUM:
	case MGB_BIOS_CHECKSUM:
	case SGB_BIOS_CHECKSUM:
	case SGB2_BIOS_CHECKSUM:
	case CGB_BIOS_CHECKSUM:
	case CGB0_BIOS_CHECKSUM:
	case AGB_BIOS_CHECKSUM:
		return true;
	default:
		return false;
	}
}

void GBReset(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	gb->memory.romBase = gb->memory.rom;
	GBDetectModel(gb);

	cpu->b = 0;
	cpu->d = 0;

	gb->timer.internalDiv = 0;

	gb->cpuBlocked = false;
	gb->earlyExit = false;
	gb->doubleSpeed = 0;

	if (gb->yankedRomSize) {
		gb->memory.romSize = gb->yankedRomSize;
		gb->memory.mbcType = gb->yankedMbc;
		gb->yankedRomSize = 0;
	}

	gb->sgbBit = -1;
	gb->sgbControllers = 0;
	gb->sgbCurrentController = 0;
	gb->currentSgbBits = 0;
	gb->sgbIncrement = false;
	memset(gb->sgbPacket, 0, sizeof(gb->sgbPacket));

	mTimingClear(&gb->timing);

	GBMemoryReset(gb);

	if (gb->biosVf) {
		if (!GBIsBIOS(gb->biosVf)) {
			gb->biosVf->close(gb->biosVf);
			gb->biosVf = NULL;
		} else {
			GBMapBIOS(gb);
			cpu->a = 0;
			cpu->f.packed = 0;
			cpu->c = 0;
			cpu->e = 0;
			cpu->h = 0;
			cpu->l = 0;
			cpu->sp = 0;
			cpu->pc = 0;
		}
	}

	switch (gb->model) {
	case GB_MODEL_DMG:
	case GB_MODEL_SGB:
	case GB_MODEL_AUTODETECT: //Silence warnings
		gb->audio.style = GB_AUDIO_DMG;
		break;
	case GB_MODEL_MGB:
	case GB_MODEL_SGB2:
		gb->audio.style = GB_AUDIO_MGB;
		break;
	case GB_MODEL_AGB:
	case GB_MODEL_CGB:
	case GB_MODEL_SCGB:
		gb->audio.style = GB_AUDIO_CGB;
		break;
	}

	GBVideoReset(&gb->video);
	GBTimerReset(&gb->timer);
	GBIOReset(gb);
	GBAudioReset(&gb->audio);
	if (!gb->biosVf && gb->memory.rom) {
		GBSkipBIOS(gb);
	} else {
		mTimingSchedule(&gb->timing, &gb->timer.event, 0);
	}

	GBSIOReset(&gb->sio);

	cpu->memory.setActiveRegion(cpu, cpu->pc);

	gb->sramMaskWriteback = false;
	GBSavedataUnmask(gb);
}

void GBSkipBIOS(struct GB* gb) {
	struct SM83Core* cpu = gb->cpu;
	const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	int nextDiv = 0;

	switch (gb->model) {
	case GB_MODEL_AUTODETECT: // Silence warnings
		gb->model = GB_MODEL_DMG;
		// Fall through
	case GB_MODEL_DMG:
		cpu->a = 1;
		cpu->f.packed = 0xB0;
		cpu->c = 0x13;
		cpu->e = 0xD8;
		cpu->h = 1;
		cpu->l = 0x4D;
		gb->timer.internalDiv = 0xABC;
		nextDiv = 4;
		break;
	case GB_MODEL_SGB:
		cpu->a = 1;
		cpu->f.packed = 0x00;
		cpu->c = 0x14;
		cpu->e = 0x00;
		cpu->h = 0xC0;
		cpu->l = 0x60;
		gb->timer.internalDiv = 0xD85;
		nextDiv = 8;
		break;
	case GB_MODEL_MGB:
		cpu->a = 0xFF;
		cpu->f.packed = 0xB0;
		cpu->c = 0x13;
		cpu->e = 0xD8;
		cpu->h = 1;
		cpu->l = 0x4D;
		gb->timer.internalDiv = 0xABC;
		nextDiv = 4;
		break;
	case GB_MODEL_SGB2:
		cpu->a = 0xFF;
		cpu->f.packed = 0x00;
		cpu->c = 0x14;
		cpu->e = 0x00;
		cpu->h = 0xC0;
		cpu->l = 0x60;
		gb->timer.internalDiv = 0xD84;
		nextDiv = 8;
		break;
	case GB_MODEL_AGB:
		cpu->b = 1;
		// Fall through
	case GB_MODEL_CGB:
	case GB_MODEL_SCGB:
		cpu->a = 0x11;
		if (gb->model == GB_MODEL_AGB) {
			cpu->f.packed = 0x00;
		} else {
			cpu->f.packed = 0x80;
		}
		cpu->c = 0;
		cpu->h = 0;
		if (cart->cgb & 0x80) {
			cpu->d = 0xFF;
			cpu->e = 0x56;
			cpu->l = 0x0D;
			gb->timer.internalDiv = 0x2F0;
		} else {
			cpu->e = 0x08;
			cpu->l = 0x7C;
			gb->timer.internalDiv = 0x260;
			gb->model = GB_MODEL_DMG;
			gb->memory.io[GB_REG_KEY1] = 0xFF;
			gb->memory.io[GB_REG_BCPS] = 0x88; // Faked writing 4 BG palette entries
			gb->memory.io[GB_REG_OCPS] = 0x90; // Faked writing 8 OBJ palette entries
			gb->memory.io[GB_REG_SVBK] = 0xFF;
			GBVideoDisableCGB(&gb->video);
		}
		nextDiv = 0xC;
		break;
	}

	unsigned i;
	for (i = 0; i < sizeof(cart->logo); ++i) {
		uint8_t byte = GBLoad8(cpu, 0x104 + i);

		uint8_t output0 = 0;
		uint8_t output1 = 0;

		output0 |= (byte & 0x80) >> 0;
		output0 |= (byte & 0x40) >> 1;
		output0 |= (byte & 0x20) >> 2;
		output0 |= (byte & 0x10) >> 3;
		output0 |= output0 >> 1;

		output1 |= (byte & 0x08) << 3;
		output1 |= (byte & 0x04) << 2;
		output1 |= (byte & 0x02) << 1;
		output1 |= (byte & 0x01) << 0;
		output1 |= output1 << 1;

		GBPatch8(cpu, 0x8010 + i * 8, output0, NULL, 0);
		GBPatch8(cpu, 0x8012 + i * 8, output0, NULL, 0);
		GBPatch8(cpu, 0x8014 + i * 8, output1, NULL, 0);
		GBPatch8(cpu, 0x8016 + i * 8, output1, NULL, 0);
	}
	for (i = 0; i < sizeof(_registeredTrademark); ++i) {
		GBPatch8(cpu, 0x8190 + i * 2, _registeredTrademark[i], NULL, 0);
	}
	if (gb->model < GB_MODEL_CGB) {
		for (i = 0; i < 12; ++i) {
			GBPatch8(cpu, 0x9904 + i, i + 1, NULL, 0);
			GBPatch8(cpu, 0x9924 + i, i + 13, NULL, 0);
		}
		GBPatch8(cpu, 0x9910, 0x19, NULL, 0);
	}

	if (gb->memory.mbcType == GB_UNL_SACHEN_MMC2) {
		gb->memory.mbcState.sachen.locked = GB_SACHEN_UNLOCKED;
	}

	cpu->sp = 0xFFFE;
	cpu->pc = 0x100;

	gb->timer.nextDiv = GB_DMG_DIV_PERIOD * (16 - nextDiv);

	mTimingDeschedule(&gb->timing, &gb->timer.event);
	mTimingSchedule(&gb->timing, &gb->timer.event, gb->timer.nextDiv);

	if (gb->biosVf) {
		GBUnmapBIOS(gb);
	}

	GBIOWrite(gb, GB_REG_NR52, 0xF1);
	GBIOWrite(gb, GB_REG_NR14, 0x3F);
	GBIOWrite(gb, GB_REG_NR10, 0x80);
	GBIOWrite(gb, GB_REG_NR11, 0xBF);
	GBIOWrite(gb, GB_REG_NR12, 0xF3);
	GBIOWrite(gb, GB_REG_NR13, 0xF3);
	GBIOWrite(gb, GB_REG_NR24, 0x3F);
	GBIOWrite(gb, GB_REG_NR21, 0x3F);
	GBIOWrite(gb, GB_REG_NR22, 0x00);
	GBIOWrite(gb, GB_REG_NR34, 0x3F);
	GBIOWrite(gb, GB_REG_NR30, 0x7F);
	GBIOWrite(gb, GB_REG_NR31, 0xFF);
	GBIOWrite(gb, GB_REG_NR32, 0x9F);
	GBIOWrite(gb, GB_REG_NR44, 0x3F);
	GBIOWrite(gb, GB_REG_NR41, 0xFF);
	GBIOWrite(gb, GB_REG_NR42, 0x00);
	GBIOWrite(gb, GB_REG_NR43, 0x00);
	GBIOWrite(gb, GB_REG_NR50, 0x77);
	GBIOWrite(gb, GB_REG_NR51, 0xF3);
	GBIOWrite(gb, GB_REG_LCDC, 0x91);
	gb->memory.io[GB_REG_BANK] = 0x1;
	GBVideoSkipBIOS(&gb->video);
}

void GBMapBIOS(struct GB* gb) {
	gb->biosVf->seek(gb->biosVf, 0, SEEK_SET);
	gb->memory.romBase = malloc(GB_SIZE_CART_BANK0);
	ssize_t size = gb->biosVf->read(gb->biosVf, gb->memory.romBase, GB_SIZE_CART_BANK0);
	if (gb->memory.rom) {
		memcpy(&gb->memory.romBase[size], &gb->memory.rom[size], GB_SIZE_CART_BANK0 - size);
		if (size > 0x100) {
			memcpy(&gb->memory.romBase[0x100], &gb->memory.rom[0x100], 0x100);
		}
	}
}

void GBUnmapBIOS(struct GB* gb) {
	if (gb->memory.io[GB_REG_BANK] == 0xFF && gb->memory.romBase != gb->memory.rom) {
		free(gb->memory.romBase);
		if (gb->memory.mbcType == GB_MMM01) {
			GBMBCSwitchBank0(gb, gb->memory.romSize / GB_SIZE_CART_BANK0 - 2);
		} else {
			GBMBCSwitchBank0(gb, 0);
		}
	}
	// XXX: Force AGB registers for AGB-mode
	if (gb->model == GB_MODEL_AGB && gb->cpu->pc == 0x100) {
		gb->cpu->b = 1;
	}
}

void GBDetectModel(struct GB* gb) {
	if (gb->model != GB_MODEL_AUTODETECT) {
		return;
	}
	if (gb->biosVf) {
		switch (_GBBiosCRC32(gb->biosVf)) {
		case DMG_BIOS_CHECKSUM:
		case DMG0_BIOS_CHECKSUM:
			gb->model = GB_MODEL_DMG;
			break;
		case MGB_BIOS_CHECKSUM:
			gb->model = GB_MODEL_MGB;
			break;
		case SGB_BIOS_CHECKSUM:
			gb->model = GB_MODEL_SGB;
			break;
		case SGB2_BIOS_CHECKSUM:
			gb->model = GB_MODEL_SGB2;
			break;
		case CGB_BIOS_CHECKSUM:
			gb->model = GB_MODEL_CGB;
			break;
		case AGB_BIOS_CHECKSUM:
			gb->model = GB_MODEL_AGB;
			break;
		default:
			gb->biosVf->close(gb->biosVf);
			gb->biosVf = NULL;
		}
	}
	if (gb->model == GB_MODEL_AUTODETECT && gb->memory.rom) {
		const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
		if (cart->cgb & 0x80) {
			gb->model = GB_MODEL_CGB;
		} else if (cart->sgb == 0x03 && cart->oldLicensee == 0x33) {
			gb->model = GB_MODEL_SGB;
		} else {
			gb->model = GB_MODEL_DMG;
		}
	}
}

int GBValidModels(const uint8_t* bank0) {
	const struct GBCartridge* cart = (const struct GBCartridge*) &bank0[0x100];
	int models;
	if (cart->cgb == 0x80) {
		models = GB_MODEL_CGB | GB_MODEL_MGB;
	} else if (cart->cgb == 0xC0) {
		models = GB_MODEL_CGB;
	} else {
		models = GB_MODEL_MGB;		
	}
	if (cart->sgb == 0x03 && cart->oldLicensee == 0x33) {
		models |= GB_MODEL_SGB;
	}
	return models;
}

void GBUpdateIRQs(struct GB* gb) {
	int irqs = gb->memory.ie & gb->memory.io[GB_REG_IF] & 0x1F;
	if (!irqs) {
		gb->cpu->irqPending = false;
		return;
	}
	gb->cpu->halted = false;

	if (!gb->memory.ime) {
		gb->cpu->irqPending = false;
		return;
	}
	if (gb->cpu->irqPending) {
		return;
	}
	SM83RaiseIRQ(gb->cpu);
}

static void _GBAdvanceCycles(struct GB* gb) {
	struct SM83Core* cpu = gb->cpu;
	int stateMask = (4 * (2 - gb->doubleSpeed)) - 1;
	int stateOffset = ((cpu->nextEvent - cpu->cycles) & stateMask) >> !gb->doubleSpeed;
	cpu->cycles = cpu->nextEvent;
	cpu->executionState = (cpu->executionState + stateOffset) & 3;
}

void GBProcessEvents(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	bool wasHalted = cpu->halted;
	while (true) {
		do {
			int32_t cycles = cpu->cycles;
			int32_t nextEvent;

			cpu->cycles = 0;
			cpu->nextEvent = INT_MAX;

			nextEvent = cycles;
			do {
#ifdef USE_DEBUGGERS
				gb->timing.globalCycles += nextEvent;
#endif
				nextEvent = mTimingTick(&gb->timing, nextEvent);
			} while (gb->cpuBlocked);
			// This loop cannot early exit until the SM83 run loop properly handles mid-M-cycle-exits
			cpu->nextEvent = nextEvent;

			if (cpu->halted) {
				_GBAdvanceCycles(gb);
				if (!gb->memory.ie || !gb->memory.ime) {
					break;
				}
			}
			if (gb->earlyExit) {
				break;
			}
		} while (cpu->cycles >= cpu->nextEvent);
		if (gb->cpuBlocked) {
			_GBAdvanceCycles(gb);
		}
		if (!wasHalted || (cpu->executionState & 3) == SM83_CORE_FETCH) {
			break;
		}
		int nextFetch = (SM83_CORE_FETCH - cpu->executionState) * cpu->tMultiplier;
		if (nextFetch < cpu->nextEvent) {
			cpu->cycles += nextFetch;
			cpu->executionState = SM83_CORE_FETCH;
			break;
		}
		_GBAdvanceCycles(gb);
	}
	gb->earlyExit = false;
}

void GBSetInterrupts(struct SM83Core* cpu, bool enable) {
	struct GB* gb = (struct GB*) cpu->master;
	mTimingDeschedule(&gb->timing, &gb->eiPending);
	if (!enable) {
		gb->memory.ime = false;
		GBUpdateIRQs(gb);
	} else {
		mTimingSchedule(&gb->timing, &gb->eiPending, 4 * cpu->tMultiplier);
	}
}

uint16_t GBIRQVector(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	int irqs = gb->memory.ie & gb->memory.io[GB_REG_IF];

	if (irqs & (1 << GB_IRQ_VBLANK)) {
		gb->memory.io[GB_REG_IF] &= ~(1 << GB_IRQ_VBLANK);
		return GB_VECTOR_VBLANK;
	}
	if (irqs & (1 << GB_IRQ_LCDSTAT)) {
		gb->memory.io[GB_REG_IF] &= ~(1 << GB_IRQ_LCDSTAT);
		return GB_VECTOR_LCDSTAT;
	}
	if (irqs & (1 << GB_IRQ_TIMER)) {
		gb->memory.io[GB_REG_IF] &= ~(1 << GB_IRQ_TIMER);
		return GB_VECTOR_TIMER;
	}
	if (irqs & (1 << GB_IRQ_SIO)) {
		gb->memory.io[GB_REG_IF] &= ~(1 << GB_IRQ_SIO);
		return GB_VECTOR_SIO;
	}
	if (irqs & (1 << GB_IRQ_KEYPAD)) {
		gb->memory.io[GB_REG_IF] &= ~(1 << GB_IRQ_KEYPAD);
		return GB_VECTOR_KEYPAD;
	}
	return 0;
}

static void _enableInterrupts(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GB* gb = user;
	gb->memory.ime = true;
	GBUpdateIRQs(gb);
}

void GBHalt(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	if (!(gb->memory.ie & gb->memory.io[GB_REG_IF] & 0x1F)) {
		_GBAdvanceCycles(gb);
		cpu->executionState = (cpu->executionState - 1) & 3;
		cpu->halted = true;
	} else if (!gb->memory.ime) {
		mLOG(GB, GAME_ERROR, "HALT bug");
		cpu->executionState = SM83_CORE_HALT_BUG;
	}
}

void GBStop(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	if (gb->model >= GB_MODEL_CGB && gb->memory.io[GB_REG_KEY1] & 1) {
		gb->doubleSpeed ^= 1;
		gb->cpu->tMultiplier = 2 - gb->doubleSpeed;
		gb->memory.io[GB_REG_KEY1] = 0;
		gb->memory.io[GB_REG_KEY1] |= gb->doubleSpeed << 7;
	} else {
		int sleep = ~(gb->memory.io[GB_REG_JOYP] & 0x30);
		size_t c;
		for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
			struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
			if (sleep && callbacks->sleep) {
				callbacks->sleep(callbacks->context);
			} else if (callbacks->shutdown) {
				callbacks->shutdown(callbacks->context);
			}
		}
	}
}

void GBIllegal(struct SM83Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	mLOG(GB, GAME_ERROR, "Hit illegal opcode at address %04X:%02X", cpu->pc, cpu->bus);
#ifdef USE_DEBUGGERS
	if (cpu->components && cpu->components[CPU_COMPONENT_DEBUGGER]) {
		struct mDebuggerEntryInfo info = {
			.address = cpu->pc,
			.type.bp.opcode = cpu->bus
		};
		mDebuggerEnter((struct mDebugger*) cpu->components[CPU_COMPONENT_DEBUGGER], DEBUGGER_ENTER_ILLEGAL_OP, &info);
	}
#endif
	// Hang forever
	gb->memory.ime = 0;
	--cpu->pc;
}

bool GBIsROM(struct VFile* vf) {
	if (!vf) {
		return false;
	}
	vf->seek(vf, 0x100, SEEK_SET);
	uint8_t header[0x100];

	if (vf->read(vf, &header, sizeof(header)) < (ssize_t) sizeof(header)) {
		return false;
	}
	if (memcmp(&header[4], _knownHeader, sizeof(_knownHeader)) == 0) {
		return true;
	}
	if (memcmp(&header[4], _knownHeaderSachen, sizeof(_knownHeaderSachen)) == 0) {
		// Sachen logo
		return true;
	}
	if (header[0x04] == _knownHeader[0] && header[0x44] == _knownHeader[1] &&
	    header[0x14] == _knownHeader[2] && header[0x54] == _knownHeader[3]) {
		// Sachen MMC1 scrambled header
		return true;
	}
	if (header[0x04] == _knownHeaderSachen[0] && header[0x44] == _knownHeaderSachen[1] &&
	    header[0x14] == _knownHeaderSachen[2] && header[0x54] == _knownHeaderSachen[3]) {
		// Sachen MMC2 scrambled header
		return true;
	}

	uint8_t footer[16];
	vf->seek(vf, -sizeof(footer), SEEK_END);
	if (vf->read(vf, footer, sizeof(footer)) < (ssize_t) sizeof(footer)) {
		return false;
	}
	uint32_t size;
	uint32_t vers;
	LOAD_32BE(size, 0, footer);
	LOAD_32BE(vers, 4, footer);
	if (memcmp(&footer[12], "GBX!", 4) == 0 && size == 0x40 && vers == 1) {
		// GBX file
		return true;
	}

	return false;
}

void GBGetGameTitle(const struct GB* gb, char* out) {
	const struct GBCartridge* cart = NULL;
	if (gb->memory.rom) {
		cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	}
	if (!cart) {
		return;
	}
	if (cart->oldLicensee != 0x33) {
		memcpy(out, cart->titleLong, 16);
	} else {
		memcpy(out, cart->titleShort, 11);
	}
}

void GBGetGameCode(const struct GB* gb, char* out) {
	memset(out, 0, 8);
	const struct GBCartridge* cart = NULL;
	if (gb->memory.rom) {
		cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	}
	if (!cart) {
		return;
	}
	if (cart->cgb == 0xC0) {
		memcpy(out, "CGB-????", 8);
	} else {
		memcpy(out, "DMG-????", 8);
	}
	if (cart->oldLicensee == 0x33) {
		memcpy(&out[4], cart->maker, 4);
	}
}

void GBFrameStarted(struct GB* gb) {
	GBTestKeypadIRQ(gb);

	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
		if (callbacks->videoFrameStarted) {
			callbacks->videoFrameStarted(callbacks->context);
		}
	}
}

void GBFrameEnded(struct GB* gb) {
	GBSramClean(gb, gb->video.frameCounter);

	if (gb->cpu->components && gb->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
		struct mCheatDevice* device = (struct mCheatDevice*) gb->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
		size_t i;
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
			mCheatRefresh(device, cheats);
		}
	}

	// TODO: Move to common code
	if (gb->stream && gb->stream->postVideoFrame) {
		const color_t* pixels;
		size_t stride;
		gb->video.renderer->getPixels(gb->video.renderer, &stride, (const void**) &pixels);
		gb->stream->postVideoFrame(gb->stream, pixels, stride);
	}

	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
		if (callbacks->videoFrameEnded) {
			callbacks->videoFrameEnded(callbacks->context);
		}
	}
}

enum GBModel GBNameToModel(const char* model) {
	if (strcasecmp(model, "DMG") == 0 || strcasecmp(model, "GB") == 0) {
		return GB_MODEL_DMG;
	} else if (strcasecmp(model, "CGB") == 0 || strcasecmp(model, "GBC") == 0) {
		return GB_MODEL_CGB;
	} else if (strcasecmp(model, "AGB") == 0 || strcasecmp(model, "GBA") == 0) {
		return GB_MODEL_AGB;
	} else if (strcasecmp(model, "SGB") == 0) {
		return GB_MODEL_SGB;
	} else if (strcasecmp(model, "MGB") == 0) {
		return GB_MODEL_MGB;
	} else if (strcasecmp(model, "SGB2") == 0) {
		return GB_MODEL_SGB2;
	} else if (strcasecmp(model, "SCGB") == 0 || strcasecmp(model, "SGBC") == 0) {
		return GB_MODEL_SCGB;
	}
	return GB_MODEL_AUTODETECT;
}

const char* GBModelToName(enum GBModel model) {
	switch (model) {
	case GB_MODEL_DMG:
		return "DMG";
	case GB_MODEL_SGB:
		return "SGB";
	case GB_MODEL_MGB:
		return "MGB";
	case GB_MODEL_SGB2:
		return "SGB2";
	case GB_MODEL_CGB:
		return "CGB";
	case GB_MODEL_AGB:
		return "AGB";
	case GB_MODEL_SCGB:
		return "SCGB";
	default:
	case GB_MODEL_AUTODETECT:
		return NULL;
	}
}
