/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/gb.h>

#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/lr35902/lr35902.h>

#include <mgba/core/core.h>
#include <mgba/core/cheats.h>
#include <mgba-util/crc32.h>
#include <mgba-util/memory.h>
#include <mgba-util/math.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

#define CLEANUP_THRESHOLD 15

const uint32_t CGB_LR35902_FREQUENCY = 0x800000;
const uint32_t SGB_LR35902_FREQUENCY = 0x418B1E;

const uint32_t GB_COMPONENT_MAGIC = 0x400000;

static const uint8_t _knownHeader[4] = { 0xCE, 0xED, 0x66, 0x66};

#define DMG_BIOS_CHECKSUM 0xC2F5CC97
#define DMG_2_BIOS_CHECKSUM 0x59C8598E
#define CGB_BIOS_CHECKSUM 0x41884E46

mLOG_DEFINE_CATEGORY(GB, "GB", "gb");

static void GBInit(void* cpu, struct mCPUComponent* component);
static void GBDeinit(struct mCPUComponent* component);
static void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh);
static void GBProcessEvents(struct LR35902Core* cpu);
static void GBSetInterrupts(struct LR35902Core* cpu, bool enable);
static void GBIllegal(struct LR35902Core* cpu);
static void GBStop(struct LR35902Core* cpu);

static void _enableInterrupts(struct mTiming* timing, void* user, uint32_t cyclesLate);

#ifdef _3DS
extern uint32_t* romBuffer;
extern size_t romBufferSize;
#endif

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
	GBAudioInit(&gb->audio, 2048, &gb->memory.io[REG_NR52], GB_AUDIO_DMG); // TODO: Remove magic constant

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

bool GBLoadROM(struct GB* gb, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	GBUnloadROM(gb);
	gb->romVf = vf;
	gb->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
	gb->isPristine = true;
#ifdef _3DS
	if (gb->pristineRomSize <= romBufferSize) {
		gb->memory.rom = romBuffer;
		vf->read(vf, romBuffer, gb->pristineRomSize);
	}
#else
	gb->memory.rom = vf->map(vf, gb->pristineRomSize, MAP_READ);
#endif
	if (!gb->memory.rom) {
		return false;
	}
	gb->yankedRomSize = 0;
	gb->memory.romBase = gb->memory.rom;
	gb->memory.romSize = gb->pristineRomSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);
	GBMBCInit(gb);

	if (gb->cpu) {
		struct LR35902Core* cpu = gb->cpu;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
	}

	// TODO: error check
	return true;
}

static void GBSramDeinit(struct GB* gb) {
	if (gb->sramVf) {
		gb->sramVf->unmap(gb->sramVf, gb->memory.sram, gb->sramSize);
		if (gb->memory.mbcType == GB_MBC3_RTC && gb->sramVf == gb->sramRealVf) {
			GBMBCRTCWrite(gb);
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
	gb->sramRealVf = vf;
	if (gb->sramSize) {
		GBResizeSram(gb, gb->sramSize);
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
				gb->memory.sram = vf->map(vf, size, MAP_WRITE);
				memset(&gb->memory.sram[gb->sramSize], 0xFF, size - gb->sramSize);
			} else if (size > gb->sramSize || !gb->memory.sram) {
				if (gb->memory.sram) {
					vf->unmap(vf, gb->memory.sram, gb->sramSize);
				}
				gb->memory.sram = vf->map(vf, size, MAP_WRITE);
			}
		} else {
			if (gb->memory.sram) {
				vf->unmap(vf, gb->memory.sram, gb->sramSize);
			}
			gb->memory.sram = vf->map(vf, size, MAP_READ);
		}
		if (gb->memory.sram == (void*) -1) {
			gb->memory.sram = NULL;
		}
	} else {
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
	if (gb->sramDirty & GB_SRAM_DIRT_NEW) {
		gb->sramDirtAge = frameCount;
		gb->sramDirty &= ~GB_SRAM_DIRT_NEW;
		if (!(gb->sramDirty & GB_SRAM_DIRT_SEEN)) {
			gb->sramDirty |= GB_SRAM_DIRT_SEEN;
		}
	} else if ((gb->sramDirty & GB_SRAM_DIRT_SEEN) && frameCount - gb->sramDirtAge > CLEANUP_THRESHOLD) {
		if (gb->sramMaskWriteback) {
			GBSavedataUnmask(gb);
		}
		if (gb->memory.mbcType == GB_MBC3_RTC) {
			GBMBCRTCWrite(gb);
		}
		gb->sramDirty = 0;
		if (gb->memory.sram && gb->sramVf->sync(gb->sramVf, gb->memory.sram, gb->sramSize)) {
			mLOG(GB_MEM, INFO, "Savedata synced");
		} else {
			mLOG(GB_MEM, INFO, "Savedata failed to sync!");
		}
	}
}

void GBSavedataMask(struct GB* gb, struct VFile* vf, bool writeback) {
	GBSramDeinit(gb);
	gb->sramVf = vf;
	gb->sramMaskWriteback = writeback;
	gb->memory.sram = vf->map(vf, gb->sramSize, MAP_READ);
	GBMBCSwitchSramBank(gb, gb->memory.sramCurrentBank);
}

void GBSavedataUnmask(struct GB* gb) {
	if (gb->sramVf == gb->sramRealVf) {
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
	vf->close(vf);
}

void GBUnloadROM(struct GB* gb) {
	// TODO: Share with GBAUnloadROM
	if (gb->memory.rom && gb->memory.romBase != gb->memory.rom && !gb->isPristine) {
		free(gb->memory.romBase);
	}
	if (gb->memory.rom && !gb->isPristine) {
		if (gb->yankedRomSize) {
			gb->yankedRomSize = 0;
		}
		mappedMemoryFree(gb->memory.rom, GB_SIZE_CART_MAX);
	}

	if (gb->romVf) {
#ifndef _3DS
		gb->romVf->unmap(gb->romVf, gb->memory.rom, gb->pristineRomSize);
#endif
		gb->romVf->close(gb->romVf);
		gb->romVf = NULL;
	}
	gb->memory.rom = NULL;
	gb->memory.mbcType = GB_MBC_AUTODETECT;
	gb->isPristine = false;

	gb->sramMaskWriteback = false;
	GBSavedataUnmask(gb);
	GBSramDeinit(gb);
	if (gb->sramRealVf) {
		gb->sramRealVf->close(gb->sramRealVf);
	}
	gb->sramRealVf = NULL;
	gb->sramVf = NULL;
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
#ifndef _3DS
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

void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh) {
	irqh->reset = GBReset;
	irqh->processEvents = GBProcessEvents;
	irqh->setInterrupts = GBSetInterrupts;
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
	case DMG_2_BIOS_CHECKSUM:
	case CGB_BIOS_CHECKSUM:
		return true;
	default:
		return false;
	}
}

void GBReset(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	gb->memory.romBase = gb->memory.rom;
	GBDetectModel(gb);

	if (gb->biosVf) {
		if (!GBIsBIOS(gb->biosVf)) {
			gb->biosVf->close(gb->biosVf);
			gb->biosVf = NULL;
		} else {
			gb->biosVf->seek(gb->biosVf, 0, SEEK_SET);
			gb->memory.romBase = malloc(GB_SIZE_CART_BANK0);
			ssize_t size = gb->biosVf->read(gb->biosVf, gb->memory.romBase, GB_SIZE_CART_BANK0);
			memcpy(&gb->memory.romBase[size], &gb->memory.rom[size], GB_SIZE_CART_BANK0 - size);
			if (size > 0x100) {
				memcpy(&gb->memory.romBase[0x100], &gb->memory.rom[0x100], sizeof(struct GBCartridge));
			}

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

	cpu->b = 0;
	cpu->d = 0;

	if (!gb->biosVf) {
		switch (gb->model) {
		case GB_MODEL_DMG:
			// TODO: SGB
		case GB_MODEL_SGB:
		case GB_MODEL_AUTODETECT: // Silence warnings
			gb->model = GB_MODEL_DMG;
			cpu->a = 1;
			cpu->f.packed = 0xB0;
			cpu->c = 0x13;
			cpu->e = 0xD8;
			cpu->h = 1;
			cpu->l = 0x4D;
			gb->timer.internalDiv = 0x2AF3;
			break;
		case GB_MODEL_AGB:
			cpu->b = 1;
			// Fall through
		case GB_MODEL_CGB:
			cpu->a = 0x11;
			cpu->f.packed = 0x80;
			cpu->c = 0;
			cpu->e = 0x08;
			cpu->h = 0;
			cpu->l = 0x7C;
			gb->timer.internalDiv = 0x7A8;
			break;
		}

		cpu->sp = 0xFFFE;
		cpu->pc = 0x100;
	}

	gb->cpuBlocked = false;
	gb->earlyExit = false;
	gb->doubleSpeed = 0;

	cpu->memory.setActiveRegion(cpu, cpu->pc);

	if (gb->yankedRomSize) {
		gb->memory.romSize = gb->yankedRomSize;
		gb->yankedRomSize = 0;
	}

	mTimingClear(&gb->timing);

	GBMemoryReset(gb);
	GBVideoReset(&gb->video);
	GBTimerReset(&gb->timer);
	mTimingSchedule(&gb->timing, &gb->timer.event, GB_DMG_DIV_PERIOD);

	GBAudioReset(&gb->audio);
	GBIOReset(gb);
	GBSIOReset(&gb->sio);

	GBSavedataUnmask(gb);
}

void GBDetectModel(struct GB* gb) {
	if (gb->model != GB_MODEL_AUTODETECT) {
		return;
	}
	if (gb->biosVf) {
		switch (_GBBiosCRC32(gb->biosVf)) {
		case DMG_BIOS_CHECKSUM:
		case DMG_2_BIOS_CHECKSUM:
			gb->model = GB_MODEL_DMG;
			break;
		case CGB_BIOS_CHECKSUM:
			gb->model = GB_MODEL_CGB;
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
		} else {
			gb->model = GB_MODEL_DMG;
		}
	}

	switch (gb->model) {
	case GB_MODEL_DMG:
	case GB_MODEL_SGB:
	case GB_MODEL_AUTODETECT: //Silence warnings
		gb->audio.style = GB_AUDIO_DMG;
		break;
	case GB_MODEL_AGB:
	case GB_MODEL_CGB:
		gb->audio.style = GB_AUDIO_CGB;
		break;
	}
}

void GBUpdateIRQs(struct GB* gb) {
	int irqs = gb->memory.ie & gb->memory.io[REG_IF];
	if (!irqs) {
		return;
	}
	gb->cpu->halted = false;

	if (!gb->memory.ime || gb->cpu->irqPending) {
		return;
	}

	if (irqs & (1 << GB_IRQ_VBLANK)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_VBLANK);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_VBLANK);
		return;
	}
	if (irqs & (1 << GB_IRQ_LCDSTAT)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_LCDSTAT);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_LCDSTAT);
		return;
	}
	if (irqs & (1 << GB_IRQ_TIMER)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_TIMER);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_TIMER);
		return;
	}
	if (irqs & (1 << GB_IRQ_SIO)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_SIO);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_SIO);
		return;
	}
	if (irqs & (1 << GB_IRQ_KEYPAD)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_KEYPAD);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_KEYPAD);
	}
}

void GBProcessEvents(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	do {
		int32_t cycles = cpu->cycles;
		int32_t nextEvent;

		cpu->cycles = 0;
		cpu->nextEvent = INT_MAX;

		nextEvent = cycles;
		do {
			nextEvent = mTimingTick(&gb->timing, nextEvent);
		} while (gb->cpuBlocked);
		cpu->nextEvent = nextEvent;

		if (gb->earlyExit) {
			gb->earlyExit = false;
			break;
		}
		if (cpu->halted) {
			cpu->cycles = cpu->nextEvent;
			if (!gb->memory.ie || !gb->memory.ime) {
				break;
			}
		}
	} while (cpu->cycles >= cpu->nextEvent);
}

void GBSetInterrupts(struct LR35902Core* cpu, bool enable) {
	struct GB* gb = (struct GB*) cpu->master;
	if (!enable) {
		gb->memory.ime = enable;
		mTimingDeschedule(&gb->timing, &gb->eiPending);
		GBUpdateIRQs(gb);
	} else {
		mTimingDeschedule(&gb->timing, &gb->eiPending);
		mTimingSchedule(&gb->timing, &gb->eiPending, 4);
	}
}

static void _enableInterrupts(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GB* gb = user;
	gb->memory.ime = true;
	GBUpdateIRQs(gb);
}

void GBHalt(struct LR35902Core* cpu) {
	if (!cpu->irqPending) {
		cpu->cycles = cpu->nextEvent;
		cpu->halted = true;
	}
}

void GBStop(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	if (cpu->bus) {
		mLOG(GB, GAME_ERROR, "Hit illegal stop at address %04X:%02X\n", cpu->pc, cpu->bus);
	}
	if (gb->memory.io[REG_KEY1] & 1) {
		gb->doubleSpeed ^= 1;
		gb->audio.timingFactor = gb->doubleSpeed + 1;
		gb->memory.io[REG_KEY1] = 0;
		gb->memory.io[REG_KEY1] |= gb->doubleSpeed << 7;
	} else if (cpu->bus) {
#ifdef USE_DEBUGGERS
		if (cpu->components && cpu->components[CPU_COMPONENT_DEBUGGER]) {
			struct mDebuggerEntryInfo info = {
				.address = cpu->pc - 1,
				.type.bp.opcode = 0x1000 | cpu->bus
			};
			mDebuggerEnter((struct mDebugger*) cpu->components[CPU_COMPONENT_DEBUGGER], DEBUGGER_ENTER_ILLEGAL_OP, &info);
		}
#endif
		// Hang forever
		gb->memory.ime = 0;
		cpu->pc -= 2;
	}
	// TODO: Actually stop
}

void GBIllegal(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	mLOG(GB, GAME_ERROR, "Hit illegal opcode at address %04X:%02X\n", cpu->pc, cpu->bus);
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
	vf->seek(vf, 0x104, SEEK_SET);
	uint8_t header[4];

	if (vf->read(vf, &header, sizeof(header)) < (ssize_t) sizeof(header)) {
		return false;
	}
	if (memcmp(header, _knownHeader, sizeof(header))) {
		return false;
	}
	return true;
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

	GBTestKeypadIRQ(gb);
}
