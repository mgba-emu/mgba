/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ds.h"

#include "arm/decoder.h"
#include "arm/debugger/debugger.h"
#include "arm/isa-inlines.h"
#include "ds/bios.h"

#include "util/crc32.h"
#include "util/memory.h"
#include "util/math.h"
#include "util/vfs.h"

mLOG_DEFINE_CATEGORY(DS, "DS");

const uint32_t DS_ARM946ES_FREQUENCY = 0x1FF61FE;
const uint32_t DS_ARM7TDMI_FREQUENCY = 0xFFB0FF;
const uint32_t DS_COMPONENT_MAGIC = 0x1FF61FE;

static const size_t DS_ROM_MAGIC_OFFSET = 0x15C;
static const uint8_t DS_ROM_MAGIC[] = { 0x56, 0xCF };

enum {
	DS7_SP_BASE = 0x380FD80,
	DS7_SP_BASE_IRQ = 0x380FF80,
	DS7_SP_BASE_SVC = 0x380FFC0,

	DS9_SP_BASE = 0x3002F7C,
	DS9_SP_BASE_IRQ = 0x3003F80,
	DS9_SP_BASE_SVC = 0x3003FC0,
};

static void DSInit(void* cpu, struct mCPUComponent* component);

static void DS7Reset(struct ARMCore* cpu);
static void DS7InterruptHandlerInit(struct ARMInterruptHandler* irqh);

static void DS9Reset(struct ARMCore* cpu);
static void DS9InterruptHandlerInit(struct ARMInterruptHandler* irqh);

static void DSProcessEvents(struct ARMCore* cpu);
static void DSHitStub(struct ARMCore* cpu, uint32_t opcode);
static void DSIllegal(struct ARMCore* cpu, uint32_t opcode);
static void DSBreakpoint(struct ARMCore* cpu, int immediate);

void DSCreate(struct DS* ds) {
	ds->d.id = DS_COMPONENT_MAGIC;
	ds->d.init = DSInit;
	ds->d.deinit = NULL;
	ds->arm7 = NULL;
	ds->arm9 = NULL;
}

static void DSInit(void* cpu, struct mCPUComponent* component) {
	struct DS* ds = (struct DS*) component;
	struct ARMCore* core = cpu;
	if (!ds->arm7) {
		// The ARM7 must get initialized first
		ds->arm7 = core;
		ds->debugger = 0;
		ds->sync = 0;
		return;
	}
	ds->arm9 = cpu;

	ds->arm9->cp15.r1.c0 = ARMControlRegFillVE(0);

	DS7InterruptHandlerInit(&ds->arm7->irqh);
	DS9InterruptHandlerInit(&ds->arm9->irqh);
	DSMemoryInit(ds);

	ds->video.p = ds;

	ds->springIRQ7 = 0;
	ds->springIRQ9 = 0;
	ds->keySource = NULL;
	ds->rtcSource = NULL;
	ds->rumble = NULL;

	ds->romVf = NULL;

	ds->keyCallback = NULL;
}

void DSUnloadROM(struct DS* ds) {
	if (ds->romVf) {
		ds->romVf->close(ds->romVf);
		ds->romVf = NULL;
	}
}

void DSDestroy(struct DS* ds) {
	DSUnloadROM(ds);
	DSMemoryDeinit(ds);
}

void DS7InterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = DS7Reset;
	irqh->processEvents = DSProcessEvents;
	irqh->swi16 = NULL;
	irqh->swi32 = NULL;
	irqh->hitIllegal = DSIllegal;
	irqh->readCPSR = NULL;
	irqh->hitStub = DSHitStub;
	irqh->bkpt16 = DSBreakpoint;
	irqh->bkpt32 = DSBreakpoint;
}

void DS9InterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = DS9Reset;
	irqh->processEvents = DSProcessEvents;
	irqh->swi16 = NULL;
	irqh->swi32 = NULL;
	irqh->hitIllegal = DSIllegal;
	irqh->readCPSR = NULL;
	irqh->hitStub = DSHitStub;
	irqh->bkpt16 = DSBreakpoint;
	irqh->bkpt32 = DSBreakpoint;
}

void DS7Reset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = DS7_SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = DS7_SP_BASE_SVC;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = DS7_SP_BASE;
}

void DS9Reset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = DS9_SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = DS9_SP_BASE_SVC;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = DS9_SP_BASE;

	struct DS* ds = (struct DS*) cpu->master;
	DSMemoryReset(ds);
}

static void DSProcessEvents(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;

	if (ds->springIRQ7) {
		ARMRaiseIRQ(cpu);
		ds->springIRQ7 = 0;
	}

	do {
		int32_t cycles = cpu->nextEvent;
		int32_t nextEvent = INT_MAX;
#ifndef NDEBUG
		if (cycles < 0) {
			mLOG(DS, FATAL, "Negative cycles passed: %i", cycles);
		}
#endif

		cpu->cycles -= cycles;
		cpu->nextEvent = nextEvent;

		if (cpu->halted) {
			cpu->cycles = cpu->nextEvent;
		}
	} while (cpu->cycles >= cpu->nextEvent);
}

void DSAttachDebugger(struct DS* ds, struct mDebugger* debugger) {
	ds->debugger = (struct ARMDebugger*) debugger->platform;
	ds->arm7->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ds->arm9->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ARMHotplugAttach(ds->arm7, CPU_COMPONENT_DEBUGGER);
	ARMHotplugAttach(ds->arm9, CPU_COMPONENT_DEBUGGER);
}


void DSDetachDebugger(struct DS* ds) {
	ds->debugger = NULL;
	ARMHotplugDetach(ds->arm7, CPU_COMPONENT_DEBUGGER);
	ARMHotplugDetach(ds->arm9, CPU_COMPONENT_DEBUGGER);
	ds->arm7->components[CPU_COMPONENT_DEBUGGER] = NULL;
	ds->arm9->components[CPU_COMPONENT_DEBUGGER] = NULL;
}

bool DSLoadROM(struct DS* ds, struct VFile* vf) {
	DSUnloadROM(ds);
	ds->romVf = vf;
	// TODO: Checksum?
	// TODO: error check
	return true;
}

bool DSIsROM(struct VFile* vf) {
	if (vf->seek(vf, DS_ROM_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	uint8_t signature[sizeof(DS_ROM_MAGIC)];
	if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
		return false;
	}
	return memcmp(signature, DS_ROM_MAGIC, sizeof(signature)) == 0;
}

bool DSLoadBIOS(struct DS* ds, struct VFile* vf) {
	size_t size = vf->size(vf);
	void* data = NULL;
	uint32_t crc;
	if (size == DS7_SIZE_BIOS) {
		data = vf->map(vf, size, MAP_READ);
	} else if (size == 0x1000) {
		data = vf->map(vf, size, MAP_READ);
	}
	if (!data) {
		return false;
	}
	crc = doCrc32(data, size);
	if (crc == DS7_BIOS_CHECKSUM) {
		ds->bios7Vf = vf;
		ds->memory.bios7 = data;
		mLOG(DS, INFO, "Official DS ARM7 BIOS detected");
	} else if (crc == DS9_BIOS_CHECKSUM) {
		ds->bios9Vf = vf;
		ds->memory.bios9 = data;
		mLOG(DS, INFO, "Official DS ARM9 BIOS detected");
	} else {
		mLOG(DS, WARN, "BIOS checksum incorrect");
		vf->unmap(vf, data, size);
		return false;
	}
	return true;
}

void DSGetGameCode(struct DS* ds, char* out) {
	memset(out, 0, 8);
	if (!ds->romVf) {
		return;
	}

	struct DSCartridge* cart = ds->romVf->map(ds->romVf, sizeof(*cart), MAP_READ);
	memcpy(out, "NTR-", 4);
	memcpy(&out[4], &cart->id, 4);
	ds->romVf->unmap(ds->romVf, cart, sizeof(*cart));
}

void DSGetGameTitle(struct DS* ds, char* out) {
	memset(out, 0, 12);
	if (!ds->romVf) {
		return;
	}

	struct DSCartridge* cart = ds->romVf->map(ds->romVf, sizeof(*cart), MAP_READ);
	memcpy(out, &cart->title, 4);
	ds->romVf->unmap(ds->romVf, cart, sizeof(*cart));
}

void DSHitStub(struct ARMCore* cpu, uint32_t opcode) {
	struct DS* ds = (struct DS*) cpu->master;
	if (ds->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.opcode = opcode
		};
		mDebuggerEnter(ds->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	}
	// TODO: More sensible category?
	mLOG(DS, ERROR, "Stub opcode: %08x", opcode);
}

void DSIllegal(struct ARMCore* cpu, uint32_t opcode) {
	struct DS* ds = (struct DS*) cpu->master;
	if (ds->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.opcode = opcode
		};
		mDebuggerEnter(ds->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	} else {
		ARMRaiseUndefined(cpu);
	}
}

void DSBreakpoint(struct ARMCore* cpu, int immediate) {
	struct DS* ds = (struct DS*) cpu->master;
	if (immediate >= CPU_COMPONENT_MAX) {
		return;
	}
	switch (immediate) {
	case CPU_COMPONENT_DEBUGGER:
		if (ds->debugger) {
			struct mDebuggerEntryInfo info = {
				.address = _ARMPCAddress(cpu)
			};
			mDebuggerEnter(ds->debugger->d.p, DEBUGGER_ENTER_BREAKPOINT, &info);
		}
		break;
	default:
		break;
	}
}
