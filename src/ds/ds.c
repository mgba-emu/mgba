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
static void DS7TestIRQ(struct ARMCore* cpu);
static void DS7InterruptHandlerInit(struct ARMInterruptHandler* irqh);

static void DS9Reset(struct ARMCore* cpu);
static void DS9TestIRQ(struct ARMCore* cpu);
static void DS9WriteCP15(struct ARMCore* cpu, int crn, int crm, int opcode1, int opcode2, uint32_t value);
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
	ds->timersEnabled7 = 0;
	ds->timersEnabled9 = 0;
	memset(ds->timers7, 0, sizeof(ds->timers7));
	memset(ds->timers9, 0, sizeof(ds->timers9));
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
	irqh->swi16 = DS7Swi16;
	irqh->swi32 = DS7Swi32;
	irqh->hitIllegal = DSIllegal;
	irqh->readCPSR = DS7TestIRQ;
	irqh->writeCP15 = NULL;
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
	irqh->readCPSR = DS9TestIRQ;
	irqh->writeCP15 = DS9WriteCP15;
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

	struct DS* ds = (struct DS*) cpu->master;
	DSMemoryReset(ds);
	DS7IOInit(ds);

	struct DSCartridge* header = ds->romVf->map(ds->romVf, sizeof(*header), MAP_READ);
	if (header) {
		// TODO: Error check
		ds->romVf->seek(ds->romVf, header->arm7Offset, SEEK_SET);
		uint32_t base = header->arm7Base - DS_BASE_RAM;
		uint32_t* basePointer = &ds->memory.ram[base >> 2];
		if (base < DS_SIZE_RAM && base + header->arm7Size <= DS_SIZE_RAM) {
			ds->romVf->read(ds->romVf, basePointer, header->arm7Size);
		}
		cpu->gprs[12] = header->arm7Entry;
		cpu->gprs[ARM_LR] = header->arm7Entry;
		cpu->gprs[ARM_PC] = header->arm7Entry;
		int currentCycles = 0;
		ARM_WRITE_PC;

		ds->romVf->unmap(ds->romVf, header, sizeof(*header));
	}
}

void DS9Reset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = DS9_SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = DS9_SP_BASE_SVC;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = DS9_SP_BASE;

	struct DS* ds = (struct DS*) cpu->master;
	DS9IOInit(ds);

	struct DSCartridge* header = ds->romVf->map(ds->romVf, sizeof(*header), MAP_READ);
	if (header) {
		// TODO: Error check
		ds->romVf->seek(ds->romVf, header->arm9Offset, SEEK_SET);
		uint32_t base = header->arm9Base - DS_BASE_RAM;
		uint32_t* basePointer = &ds->memory.ram[base >> 2];
		if (base < DS_SIZE_RAM && base + header->arm9Size <= DS_SIZE_RAM) {
			ds->romVf->read(ds->romVf, basePointer, header->arm9Size);
		}
		cpu->gprs[12] = header->arm9Entry;
		cpu->gprs[ARM_LR] = header->arm9Entry;
		cpu->gprs[ARM_PC] = header->arm9Entry;
		int currentCycles = 0;
		ARM_WRITE_PC;

		ds->romVf->unmap(ds->romVf, header, sizeof(*header));
	}
}

static void DSProcessEvents(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;

	if (ds->springIRQ7) {
		ARMRaiseIRQ(cpu);
		ds->springIRQ7 = 0;
	}

	int32_t cycles = cpu->nextEvent;
	int32_t nextEvent = INT_MAX;
	int32_t testEvent;
#ifndef NDEBUG
	if (cycles < 0) {
		mLOG(DS, FATAL, "Negative cycles passed: %i", cycles);
	}
#endif

	testEvent = DSTimersProcessEvents(ds, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	cpu->cycles -= cycles;
	cpu->nextEvent = nextEvent;

	if (cpu->halted) {
		cpu->cycles = cpu->nextEvent;
	}
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

void DS7TestIRQ(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;
	if (0) {
		ds->springIRQ7 = 1;
		cpu->nextEvent = cpu->cycles;
	}
}

void DS9TestIRQ(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;
	if (0) {
		ds->springIRQ9 = 1;
		cpu->nextEvent = cpu->cycles;
	}
}

static void _writeSysControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	mLOG(DS, STUB, "CP15 system control write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
}

static void _writeCacheControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	mLOG(DS, STUB, "CP15 cache control control write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
	switch (opcode2) {
	case 0:
		cpu->cp15.r2.d = value;
		break;
	case 1:
		cpu->cp15.r2.i = value;
		break;
	default:
		mLOG(DS, GAME_ERROR, "CP15 cache control control bad op2: %i", opcode2);
		break;
	}
}

static void _writeWriteBufferControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	mLOG(DS, STUB, "CP15 write buffer control write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
	switch (opcode2) {
	case 0:
		cpu->cp15.r3.d = value;
		break;
	default:
		mLOG(DS, GAME_ERROR, "CP15 cache control control bad op2: %i", opcode2);
		break;
	}
}

static void _writeAccessControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	mLOG(DS, STUB, "CP15 access control write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
}

static void _writeRegionConfiguration(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	cpu->cp15.r6.region[crm] = value;
	uint32_t base = ARMProtectionGetBase(value) << 12;
	uint32_t size = 2 << ARMProtectionGetSize(value);
	mLOG(DS, STUB, "CP15 region configuration write: Region: %i, Insn: %i, Base: %08X, Size: %08X", crm, opcode2, base, size);
}

static void _writeCache(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	mLOG(DS, STUB, "CP15 cache write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
}

static void _writeTCMControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	uint32_t base = ARMTCMControlGetBase(value) << 12;
	uint32_t size = 512 << ARMTCMControlGetVirtualSize(value);
	mLOG(DS, STUB, "CP15 TCM control write: CRm: %i, Op2: %i, Base: %08X, Size: %08X", crm, opcode2, base, size);
	switch (opcode2) {
	case 0:
		cpu->cp15.r9.d = value;
		break;
	case 1:
		cpu->cp15.r9.i = value;
		break;
	default:
		mLOG(DS, GAME_ERROR, "CP15 TCM control bad op2: %i", opcode2);
		break;
	}
}

void DS9WriteCP15(struct ARMCore* cpu, int crn, int crm, int opcode1, int opcode2, uint32_t value) {
	switch (crn) {
	default:
		mLOG(DS, STUB, "CP15 unknown write: CRn: %i, CRm: %i, Op1: %i, Op2: %i, Value: 0x%08X", crn, crm, opcode1, opcode2, value);
		break;
	case 0:
		mLOG(DS, GAME_ERROR, "Attempted to write to read-only cp15 register");
		ARMRaiseUndefined(cpu);
		break;
	case 1:
		_writeSysControl(cpu, crm, opcode2, value);
		break;
	case 2:
		_writeCacheControl(cpu, crm, opcode2, value);
		break;
	case 3:
		_writeWriteBufferControl(cpu, crm, opcode2, value);
		break;
	case 5:
		_writeAccessControl(cpu, crm, opcode2, value);
		break;
	case 6:
		_writeRegionConfiguration(cpu, crm, opcode2, value);
		break;
	case 7:
		_writeCache(cpu, crm, opcode2, value);
		break;
	case 9:
		_writeTCMControl(cpu, crm, opcode2, value);
		break;
	}
}

void DSWriteIE(struct ARMCore* cpu, uint16_t* io, uint32_t value) {
	if (io[DS7_REG_IME >> 1] && (value & io[DS7_REG_IF_LO >> 1] || (value >> 16) & io[DS7_REG_IF_HI >> 1])) {
		ARMRaiseIRQ(cpu);
	}
}
void DSWriteIME(struct ARMCore* cpu, uint16_t* io, uint16_t value) {
	if (value && (io[DS7_REG_IE_LO >> 1] & io[DS7_REG_IF_LO >> 1] || io[DS7_REG_IE_HI >> 1] & io[DS7_REG_IF_HI >> 1])) {
		ARMRaiseIRQ(cpu);
	}
}

void DSRaiseIRQ(struct ARMCore* cpu, uint16_t* io, enum DSIRQ irq) {
	if (irq < 16) {
		io[DS7_REG_IF_LO >> 1] |= 1 << irq;
	} else {
		io[DS7_REG_IF_HI >> 1] |= 1 << (irq - 16);
	}
	cpu->halted = 0;

	if (!io[DS7_REG_IME >> 1]) {
		return;
	}
	if (irq < 16 && (io[DS7_REG_IE_LO >> 1] & 1 << irq)) {
		ARMRaiseIRQ(cpu);
	} else if (io[DS7_REG_IE_HI >> 1] & 1 << (irq - 16)) {
		ARMRaiseIRQ(cpu);
	}
}
