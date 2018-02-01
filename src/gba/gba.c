/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/gba.h>

#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/arm/decoder.h>

#include <mgba/internal/gba/bios.h>
#include <mgba/internal/gba/cheats.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/overrides.h>
#include <mgba/internal/gba/rr/rr.h>

#include <mgba-util/patch.h>
#include <mgba-util/crc32.h>
#include <mgba-util/math.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(GBA, "GBA", "gba");
mLOG_DEFINE_CATEGORY(GBA_DEBUG, "GBA Debug", "gba.debug");

const uint32_t GBA_COMPONENT_MAGIC = 0x1000000;

static const size_t GBA_ROM_MAGIC_OFFSET = 3;
static const uint8_t GBA_ROM_MAGIC[] = { 0xEA };

static const size_t GBA_ROM_MAGIC_OFFSET2 = 0xB2;
static const uint8_t GBA_ROM_MAGIC2[] = { 0x96 };

static const size_t GBA_MB_MAGIC_OFFSET = 0xC0;

static void GBAInit(void* cpu, struct mCPUComponent* component);
static void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh);
static void GBAProcessEvents(struct ARMCore* cpu);
static void GBAHitStub(struct ARMCore* cpu, uint32_t opcode);
static void GBAIllegal(struct ARMCore* cpu, uint32_t opcode);
static void GBABreakpoint(struct ARMCore* cpu, int immediate);

static bool _setSoftwareBreakpoint(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
static bool _clearSoftwareBreakpoint(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t opcode);


#ifdef _3DS
extern uint32_t* romBuffer;
extern size_t romBufferSize;
#endif

void GBACreate(struct GBA* gba) {
	gba->d.id = GBA_COMPONENT_MAGIC;
	gba->d.init = GBAInit;
	gba->d.deinit = 0;
}

static void GBAInit(void* cpu, struct mCPUComponent* component) {
	struct GBA* gba = (struct GBA*) component;
	gba->cpu = cpu;
	gba->debugger = 0;
	gba->sync = 0;

	GBAInterruptHandlerInit(&gba->cpu->irqh);
	GBAMemoryInit(gba);

	gba->memory.savedata.timing = &gba->timing;
	GBASavedataInit(&gba->memory.savedata, NULL);

	gba->video.p = gba;
	GBAVideoInit(&gba->video);

	gba->audio.p = gba;
	GBAAudioInit(&gba->audio, GBA_AUDIO_SAMPLES);

	GBAIOInit(gba);

	gba->sio.p = gba;
	GBASIOInit(&gba->sio);

	GBAHardwareInit(&gba->memory.hw, NULL);

	gba->springIRQ = 0;
	gba->keySource = 0;
	gba->rotationSource = 0;
	gba->luminanceSource = 0;
	gba->rtcSource = 0;
	gba->rumble = 0;
	gba->rr = 0;

	gba->romVf = 0;
	gba->biosVf = 0;

	gba->stream = NULL;
	gba->keyCallback = NULL;
	mCoreCallbacksListInit(&gba->coreCallbacks, 0);

	gba->biosChecksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);

	gba->idleOptimization = IDLE_LOOP_REMOVE;
	gba->idleLoop = IDLE_LOOP_NONE;

	gba->realisticTiming = true;
	gba->hardCrash = true;
	gba->allowOpposingDirections = true;

	gba->performingDMA = false;

	gba->isPristine = false;
	gba->pristineRomSize = 0;
	gba->yankedRomSize = 0;

	mTimingInit(&gba->timing, &gba->cpu->cycles, &gba->cpu->nextEvent);
}

void GBAUnloadROM(struct GBA* gba) {
	if (gba->memory.rom && !gba->isPristine) {
		if (gba->yankedRomSize) {
			gba->yankedRomSize = 0;
		}
		mappedMemoryFree(gba->memory.rom, SIZE_CART0);
	}

	if (gba->romVf) {
#ifndef _3DS
		gba->romVf->unmap(gba->romVf, gba->memory.rom, gba->pristineRomSize);
#endif
		gba->romVf->close(gba->romVf);
		gba->romVf = NULL;
	}
	gba->memory.rom = NULL;
	gba->isPristine = false;

	GBASavedataDeinit(&gba->memory.savedata);
	if (gba->memory.savedata.realVf) {
		gba->memory.savedata.realVf->close(gba->memory.savedata.realVf);
		gba->memory.savedata.realVf = 0;
	}
	gba->idleLoop = IDLE_LOOP_NONE;
}

void GBADestroy(struct GBA* gba) {
	GBAUnloadROM(gba);

	if (gba->biosVf) {
		gba->biosVf->unmap(gba->biosVf, gba->memory.bios, SIZE_BIOS);
		gba->biosVf->close(gba->biosVf);
		gba->biosVf = 0;
	}

	GBAMemoryDeinit(gba);
	GBAVideoDeinit(&gba->video);
	GBAAudioDeinit(&gba->audio);
	GBASIODeinit(&gba->sio);
	gba->rr = 0;
	mTimingDeinit(&gba->timing);
	mCoreCallbacksListDeinit(&gba->coreCallbacks);
}

void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = GBAReset;
	irqh->processEvents = GBAProcessEvents;
	irqh->swi16 = GBASwi16;
	irqh->swi32 = GBASwi32;
	irqh->hitIllegal = GBAIllegal;
	irqh->readCPSR = GBATestIRQ;
	irqh->hitStub = GBAHitStub;
	irqh->bkpt16 = GBABreakpoint;
	irqh->bkpt32 = GBABreakpoint;
}

void GBAReset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;

	struct GBA* gba = (struct GBA*) cpu->master;
	if (!gba->rr || (!gba->rr->isPlaying(gba->rr) && !gba->rr->isRecording(gba->rr))) {
		GBASavedataUnmask(&gba->memory.savedata);
	}

	gba->cpuBlocked = false;
	gba->earlyExit = false;
	if (gba->yankedRomSize) {
		gba->memory.romSize = gba->yankedRomSize;
		gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
		gba->yankedRomSize = 0;
	}
	mTimingClear(&gba->timing);
	GBAMemoryReset(gba);
	GBAVideoReset(&gba->video);
	GBAAudioReset(&gba->audio);
	GBAIOInit(gba);
	GBATimerInit(gba);

	GBASIOReset(&gba->sio);

	gba->lastJump = 0;
	gba->haltPending = false;
	gba->idleDetectionStep = 0;
	gba->idleDetectionFailures = 0;

	gba->debug = false;
	memset(gba->debugString, 0, sizeof(gba->debugString));
}

void GBASkipBIOS(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	if (cpu->gprs[ARM_PC] == BASE_RESET + WORD_SIZE_ARM) {
		if (gba->memory.rom) {
			cpu->gprs[ARM_PC] = BASE_CART0;
		} else {
			cpu->gprs[ARM_PC] = BASE_WORKING_RAM;
		}
		gba->memory.io[REG_VCOUNT >> 1] = 0x7E;
		gba->memory.io[REG_POSTFLG >> 1] = 1;
		int currentCycles = 0;
		ARM_WRITE_PC;
	}
}

static void GBAProcessEvents(struct ARMCore* cpu) {
	struct GBA* gba = (struct GBA*) cpu->master;

	gba->bus = cpu->prefetch[1];
	if (cpu->executionMode == MODE_THUMB) {
		gba->bus |= cpu->prefetch[1] << 16;
	}

	if (gba->springIRQ && !cpu->cpsr.i) {
		ARMRaiseIRQ(cpu);
		gba->springIRQ = 0;
	}

	int32_t nextEvent = cpu->nextEvent;
	while (cpu->cycles >= nextEvent) {
		int32_t cycles = cpu->cycles;

		cpu->cycles = 0;
		cpu->nextEvent = INT_MAX;

#ifndef NDEBUG
		if (cycles < 0) {
			mLOG(GBA, FATAL, "Negative cycles passed: %i", cycles);
		}
#endif
		nextEvent = cycles;
		do {
			nextEvent = mTimingTick(&gba->timing, nextEvent);
		} while (gba->cpuBlocked);

		cpu->nextEvent = nextEvent;

		if (gba->earlyExit) {
			gba->earlyExit = false;
			break;
		}
		if (cpu->halted) {
			cpu->cycles = nextEvent;
			if (!gba->memory.io[REG_IME >> 1] || !gba->memory.io[REG_IE >> 1]) {
				break;
			}
		}
#ifndef NDEBUG
		else if (nextEvent < 0) {
			mLOG(GBA, FATAL, "Negative cycles will pass: %i", nextEvent);
		}
#endif
	}
}

#ifdef USE_DEBUGGERS
void GBAAttachDebugger(struct GBA* gba, struct mDebugger* debugger) {
	gba->debugger = (struct ARMDebugger*) debugger->platform;
	gba->debugger->setSoftwareBreakpoint = _setSoftwareBreakpoint;
	gba->debugger->clearSoftwareBreakpoint = _clearSoftwareBreakpoint;
	gba->cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ARMHotplugAttach(gba->cpu, CPU_COMPONENT_DEBUGGER);
}

void GBADetachDebugger(struct GBA* gba) {
	if (gba->debugger) {
		ARMHotplugDetach(gba->cpu, CPU_COMPONENT_DEBUGGER);
	}
	gba->cpu->components[CPU_COMPONENT_DEBUGGER] = NULL;
	gba->debugger = NULL;
}
#endif

bool GBALoadMB(struct GBA* gba, struct VFile* vf) {
	GBAUnloadROM(gba);
	gba->romVf = vf;
	gba->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
	if (gba->pristineRomSize > SIZE_WORKING_RAM) {
		gba->pristineRomSize = SIZE_WORKING_RAM;
	}
	gba->isPristine = true;
	gba->memory.wram = anonymousMemoryMap(SIZE_WORKING_RAM);
	memset(gba->memory.wram, 0, SIZE_WORKING_RAM);
	vf->read(vf, gba->memory.wram, gba->pristineRomSize);
	if (!gba->memory.wram) {
		mLOG(GBA, WARN, "Couldn't map ROM");
		return false;
	}
	gba->yankedRomSize = 0;
	gba->memory.romSize = 0;
	gba->memory.romMask = 0;
	gba->romCrc32 = doCrc32(gba->memory.wram, gba->pristineRomSize);
	if (gba->cpu && gba->memory.activeRegion == REGION_WORKING_RAM) {
		gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	}
	return true;
}

bool GBALoadROM(struct GBA* gba, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	GBAUnloadROM(gba);
	gba->romVf = vf;
	gba->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
	if (gba->pristineRomSize > SIZE_CART0) {
		gba->pristineRomSize = SIZE_CART0;
	}
	gba->isPristine = true;
#ifdef _3DS
	if (gba->pristineRomSize <= romBufferSize) {
		gba->memory.rom = romBuffer;
		vf->read(vf, romBuffer, gba->pristineRomSize);
	}
#else
	gba->memory.rom = vf->map(vf, gba->pristineRomSize, MAP_READ);
#endif
	if (!gba->memory.rom) {
		mLOG(GBA, WARN, "Couldn't map ROM");
		return false;
	}
	gba->yankedRomSize = 0;
	gba->memory.romSize = gba->pristineRomSize;
	gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
	gba->memory.mirroring = false;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
	GBAHardwareInit(&gba->memory.hw, &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1]);
	GBAVFameDetect(&gba->memory.vfame, gba->memory.rom, gba->memory.romSize);
	if (gba->cpu && gba->memory.activeRegion >= REGION_CART0) {
		gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);
	}
	// TODO: error check
	return true;
}

bool GBALoadSave(struct GBA* gba, struct VFile* sav) {
	GBASavedataInit(&gba->memory.savedata, sav);
	return true;
}

void GBAYankROM(struct GBA* gba) {
	gba->yankedRomSize = gba->memory.romSize;
	gba->memory.romSize = 0;
	gba->memory.romMask = 0;
	GBARaiseIRQ(gba, IRQ_GAMEPAK);
}

void GBALoadBIOS(struct GBA* gba, struct VFile* vf) {
	gba->biosVf = vf;
	uint32_t* bios = vf->map(vf, SIZE_BIOS, MAP_READ);
	if (!bios) {
		mLOG(GBA, WARN, "Couldn't map BIOS");
		return;
	}
	gba->memory.bios = bios;
	gba->memory.fullBios = 1;
	uint32_t checksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);
	mLOG(GBA, DEBUG, "BIOS Checksum: 0x%X", checksum);
	if (checksum == GBA_BIOS_CHECKSUM) {
		mLOG(GBA, INFO, "Official GBA BIOS detected");
	} else if (checksum == GBA_DS_BIOS_CHECKSUM) {
		mLOG(GBA, INFO, "Official GBA (DS) BIOS detected");
	} else {
		mLOG(GBA, WARN, "BIOS checksum incorrect");
	}
	gba->biosChecksum = checksum;
	if (gba->memory.activeRegion == REGION_BIOS) {
		gba->cpu->memory.activeRegion = gba->memory.bios;
	}
	// TODO: error check
}

void GBAApplyPatch(struct GBA* gba, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gba->memory.romSize);
	if (!patchedSize || patchedSize > SIZE_CART0) {
		return;
	}
	void* newRom = anonymousMemoryMap(SIZE_CART0);
	if (!patch->applyPatch(patch, gba->memory.rom, gba->pristineRomSize, newRom, patchedSize)) {
		mappedMemoryFree(newRom, SIZE_CART0);
		return;
	}
	if (gba->romVf) {
#ifndef _3DS
		gba->romVf->unmap(gba->romVf, gba->memory.rom, gba->pristineRomSize);
#endif
		gba->romVf->close(gba->romVf);
		gba->romVf = NULL;
	}
	gba->isPristine = false;
	gba->memory.rom = newRom;
	gba->memory.hw.gpioBase = &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1];
	gba->memory.romSize = patchedSize;
	gba->memory.romMask = SIZE_CART0 - 1;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
}

void GBAWriteIE(struct GBA* gba, uint16_t value) {
	if (gba->memory.io[REG_IME >> 1] && value & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(gba->cpu);
	}
}

void GBAWriteIME(struct GBA* gba, uint16_t value) {
	if (value && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(gba->cpu);
	}
}

void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq) {
	gba->memory.io[REG_IF >> 1] |= 1 << irq;

	if (gba->memory.io[REG_IE >> 1] & 1 << irq) {
		gba->cpu->halted = 0;
		if (gba->memory.io[REG_IME >> 1]) {
			ARMRaiseIRQ(gba->cpu);
		}
	}
}

void GBATestIRQ(struct ARMCore* cpu) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (gba->memory.io[REG_IME >> 1] && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		gba->springIRQ = gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1];
		gba->cpu->nextEvent = gba->cpu->cycles;
	}
}

void GBAHalt(struct GBA* gba) {
	gba->cpu->nextEvent = gba->cpu->cycles;
	gba->cpu->halted = 1;
}

void GBAStop(struct GBA* gba) {
	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&gba->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gba->coreCallbacks, c);
		if (callbacks->sleep) {
			callbacks->sleep(callbacks->context);
		}
	}
	gba->cpu->nextEvent = gba->cpu->cycles;
}

void GBADebug(struct GBA* gba, uint16_t flags) {
	gba->debugFlags = flags;
	if (GBADebugFlagsIsSend(gba->debugFlags)) {
		int level = 1 << GBADebugFlagsGetLevel(gba->debugFlags);
		level &= 0x1F;
		char oolBuf[0x101];
		strncpy(oolBuf, gba->debugString, sizeof(gba->debugString));
		oolBuf[0x100] = '\0';
		mLog(_mLOG_CAT_GBA_DEBUG(), level, "%s", oolBuf);
	}
	gba->debugFlags = GBADebugFlagsClearSend(gba->debugFlags);
}

bool GBAIsROM(struct VFile* vf) {
	if (!vf) {
		return false;
	}

	uint8_t signature[sizeof(GBA_ROM_MAGIC) + sizeof(GBA_ROM_MAGIC2)];
	if (vf->seek(vf, GBA_ROM_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	if (vf->read(vf, &signature, sizeof(GBA_ROM_MAGIC)) != sizeof(GBA_ROM_MAGIC)) {
		return false;
	}
	if (memcmp(signature, GBA_ROM_MAGIC, sizeof(GBA_ROM_MAGIC)) != 0) {
		return false;
	}

	if (vf->seek(vf, GBA_ROM_MAGIC_OFFSET2, SEEK_SET) < 0) {
		return false;
	}
	if (vf->read(vf, &signature, sizeof(GBA_ROM_MAGIC2)) != sizeof(GBA_ROM_MAGIC2)) {
		return false;
	}
	if (memcmp(signature, GBA_ROM_MAGIC2, sizeof(GBA_ROM_MAGIC2)) != 0) {
		// If the signature byte is missing then we must be using an unfixed ROM
		uint32_t buffer[0x9C / sizeof(uint32_t)];
		if (vf->seek(vf, 0x4, SEEK_SET) < 0) {
			return false;
		}
		if (vf->read(vf, &buffer, sizeof(buffer)) != sizeof(buffer)) {
			return false;
		}
		uint32_t bits = 0;
		size_t i;
		for (i = 0; i < sizeof(buffer) / sizeof(*buffer); ++i) {
			bits |= buffer[i];
		}
		if (bits) {
			return false;
		}
	}


	if (GBAIsBIOS(vf)) {
		return false;
	}
	return true;
}

bool GBAIsMB(struct VFile* vf) {
	if (!GBAIsROM(vf)) {
		return false;
	}
	if (vf->size(vf) > SIZE_WORKING_RAM) {
		return false;
	}
	if (vf->seek(vf, GBA_MB_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	uint32_t signature;
	if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
		return false;
	}
	uint32_t opcode;
	LOAD_32(opcode, 0, &signature);
	struct ARMInstructionInfo info;
	ARMDecodeARM(opcode, &info);
	if (info.branchType == ARM_BRANCH) {
		if (info.op1.immediate <= 0) {
			return false;
		} else if (info.op1.immediate == 28) {
			// Ancient toolchain that is known to throw MB detection for a loop
			return false;
		} else if (info.op1.immediate != 24) {
			return true;
		}
	}

	uint32_t pc = GBA_MB_MAGIC_OFFSET;
	int i;
	for (i = 0; i < 80; ++i) {
		if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
			break;
		}
		pc += 4;
		LOAD_32(opcode, 0, &signature);
		ARMDecodeARM(opcode, &info);
		if (info.mnemonic != ARM_MN_LDR) {
			continue;
		}
		if ((info.operandFormat & ARM_OPERAND_MEMORY) && info.memory.baseReg == ARM_PC && info.memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
			uint32_t immediate = info.memory.offset.immediate;
			if (info.memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
				immediate = -immediate;
			}
			immediate += pc + 8;
			if (vf->seek(vf, immediate, SEEK_SET) < 0) {
				break;
			}
			if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
				break;
			}
			LOAD_32(immediate, 0, &signature);
			if (vf->seek(vf, pc, SEEK_SET) < 0) {
				break;
			}
			if ((immediate & ~0x7FF) == BASE_WORKING_RAM) {
				return true;
			}
		}
	}
	// Found a libgba-linked cart...these are a bit harder to detect.
	return false;
}

bool GBAIsBIOS(struct VFile* vf) {
	if (vf->seek(vf, 0, SEEK_SET) < 0) {
		return false;
	}
	uint8_t interruptTable[7 * 4];
	if (vf->read(vf, &interruptTable, sizeof(interruptTable)) != sizeof(interruptTable)) {
		return false;
	}
	int i;
	for (i = 0; i < 7; ++i) {
		if (interruptTable[4 * i + 3] != 0xEA || interruptTable[4 * i + 2]) {
			return false;
		}
	}
	return true;
}

void GBAGetGameCode(const struct GBA* gba, char* out) {
	memset(out, 0, 8);
	if (!gba->memory.rom) {
		return;
	}

	memcpy(out, "AGB-", 4);
	memcpy(&out[4], &((struct GBACartridge*) gba->memory.rom)->id, 4);
}

void GBAGetGameTitle(const struct GBA* gba, char* out) {
	if (gba->memory.rom) {
		memcpy(out, &((struct GBACartridge*) gba->memory.rom)->title, 12);
		return;
	}
	if (gba->isPristine && gba->memory.wram) {
		memcpy(out, &((struct GBACartridge*) gba->memory.wram)->title, 12);
		return;
	}
	strncpy(out, "(BIOS)", 12);
}

void GBAHitStub(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
#ifdef USE_DEBUGGERS
	if (gba->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.type.bp.opcode = opcode
		};
		mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	}
#endif
	// TODO: More sensible category?
	mLOG(GBA, ERROR, "Stub opcode: %08x", opcode);
}

void GBAIllegal(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (!gba->yankedRomSize) {
		// TODO: More sensible category?
		mLOG(GBA, WARN, "Illegal opcode: %08x", opcode);
	}
	if (cpu->executionMode == MODE_THUMB && (opcode & 0xFFC0) == 0xE800) {
		mLOG(GBA, DEBUG, "Hit Wii U VC opcode: %08x", opcode);
		return;
	}
#ifdef USE_DEBUGGERS
	if (gba->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.type.bp.opcode = opcode
		};
		mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	} else
#endif
	{
		ARMRaiseUndefined(cpu);
	}
}

void GBABreakpoint(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (immediate >= CPU_COMPONENT_MAX) {
		return;
	}
	switch (immediate) {
#ifdef USE_DEBUGGERS
	case CPU_COMPONENT_DEBUGGER:
		if (gba->debugger) {
			struct mDebuggerEntryInfo info = {
				.address = _ARMPCAddress(cpu),
				.type.bp.breakType = BREAKPOINT_SOFTWARE
			};
			mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_BREAKPOINT, &info);
		}
		break;
#endif
	case CPU_COMPONENT_CHEAT_DEVICE:
		if (gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
			struct mCheatDevice* device = (struct mCheatDevice*) gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
			struct GBACheatHook* hook = 0;
			size_t i;
			for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
				struct GBACheatSet* cheats = (struct GBACheatSet*) *mCheatSetsGetPointer(&device->cheats, i);
				if (cheats->hook && cheats->hook->address == _ARMPCAddress(cpu)) {
					mCheatRefresh(device, &cheats->d);
					hook = cheats->hook;
				}
			}
			if (hook) {
				ARMRunFake(cpu, hook->patchedOpcode);
			}
		}
		break;
	default:
		break;
	}
}

void GBAFrameStarted(struct GBA* gba) {
	GBATestKeypadIRQ(gba);

	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&gba->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gba->coreCallbacks, c);
		if (callbacks->videoFrameStarted) {
			callbacks->videoFrameStarted(callbacks->context);
		}
	}
}

void GBAFrameEnded(struct GBA* gba) {
	GBASavedataClean(&gba->memory.savedata, gba->video.frameCounter);

	if (gba->rr) {
		gba->rr->nextFrame(gba->rr);
	}

	if (gba->cpu->components && gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
		struct mCheatDevice* device = (struct mCheatDevice*) gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
		size_t i;
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct GBACheatSet* cheats = (struct GBACheatSet*) *mCheatSetsGetPointer(&device->cheats, i);
			mCheatRefresh(device, &cheats->d);
		}
	}

	if (gba->stream && gba->stream->postVideoFrame) {
		const color_t* pixels;
		size_t stride;
		gba->video.renderer->getPixels(gba->video.renderer, &stride, (const void**) &pixels);
		gba->stream->postVideoFrame(gba->stream, pixels, stride);
	}

	if (gba->memory.hw.devices & (HW_GB_PLAYER | HW_GB_PLAYER_DETECTION)) {
		GBAHardwarePlayerUpdate(gba);
	}

	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&gba->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gba->coreCallbacks, c);
		if (callbacks->videoFrameEnded) {
			callbacks->videoFrameEnded(callbacks->context);
		}
	}
}

void GBATestKeypadIRQ(struct GBA* gba) {
	uint16_t keycnt = gba->memory.io[REG_KEYCNT >> 1];
	if (!(keycnt & 0x4000)) {
		return;
	}
	int isAnd = keycnt & 0x8000;
	if (!gba->keySource) {
		// TODO?
		return;
	}

	keycnt &= 0x3FF;
	uint16_t keyInput = *gba->keySource & keycnt;

	if (isAnd && keycnt == keyInput) {
		GBARaiseIRQ(gba, IRQ_KEYPAD);
	} else if (!isAnd && keyInput) {
		GBARaiseIRQ(gba, IRQ_KEYPAD);
	}
}

void GBASetBreakpoint(struct GBA* gba, struct mCPUComponent* component, uint32_t address, enum ExecutionMode mode, uint32_t* opcode) {
	size_t immediate;
	for (immediate = 0; immediate < gba->cpu->numComponents; ++immediate) {
		if (gba->cpu->components[immediate] == component) {
			break;
		}
	}
	if (immediate == gba->cpu->numComponents) {
		return;
	}
	if (mode == MODE_ARM) {
		int32_t value;
		int32_t old;
		value = 0xE1200070;
		value |= immediate & 0xF;
		value |= (immediate & 0xFFF0) << 4;
		GBAPatch32(gba->cpu, address, value, &old);
		*opcode = old;
	} else {
		int16_t value;
		int16_t old;
		value = 0xBE00;
		value |= immediate & 0xFF;
		GBAPatch16(gba->cpu, address, value, &old);
		*opcode = (uint16_t) old;
	}
}

void GBAClearBreakpoint(struct GBA* gba, uint32_t address, enum ExecutionMode mode, uint32_t opcode) {
	if (mode == MODE_ARM) {
		GBAPatch32(gba->cpu, address, opcode, 0);
	} else {
		GBAPatch16(gba->cpu, address, opcode, 0);
	}
}

static bool _setSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode, uint32_t* opcode) {
	GBASetBreakpoint((struct GBA*) debugger->cpu->master, &debugger->d.p->d, address, mode, opcode);
	return true;
}

static bool _clearSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode, uint32_t opcode) {
	GBAClearBreakpoint((struct GBA*) debugger->cpu->master, address, mode, opcode);
	return true;
}
