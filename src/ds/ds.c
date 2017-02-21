/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/ds.h>

#include <mgba/core/interface.h>
#include <mgba/internal/arm/decoder.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/ds/bios.h>

#include <mgba-util/crc32.h>
#include <mgba-util/memory.h>
#include <mgba-util/math.h>
#include <mgba-util/vfs.h>

#define SLICE_CYCLES 2048

mLOG_DEFINE_CATEGORY(DS, "DS");

const uint32_t DS_ARM946ES_FREQUENCY = 0x1FF61FE;
const uint32_t DS_ARM7TDMI_FREQUENCY = 0xFFB0FF;
const uint32_t DS_COMPONENT_MAGIC = 0x1FF61FE;

static const size_t DS_ROM_MAGIC_OFFSET = 0x15C;
static const uint8_t DS_ROM_MAGIC[] = { 0x56, 0xCF };
static const uint8_t DS_ROM_MAGIC_2[] = { 0x1A, 0x9E };

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
static void DS7ProcessEvents(struct ARMCore* cpu);

static void DS9Reset(struct ARMCore* cpu);
static void DS9TestIRQ(struct ARMCore* cpu);
static void DS9WriteCP15(struct ARMCore* cpu, int crn, int crm, int opcode1, int opcode2, uint32_t value);
static uint32_t DS9ReadCP15(struct ARMCore* cpu, int crn, int crm, int opcode1, int opcode2);
static void DS9InterruptHandlerInit(struct ARMInterruptHandler* irqh);
static void DS9ProcessEvents(struct ARMCore* cpu);

static void DSProcessEvents(struct DSCommon* dscore);
static void DSHitStub(struct ARMCore* cpu, uint32_t opcode);
static void DSIllegal(struct ARMCore* cpu, uint32_t opcode);
static void DSBreakpoint(struct ARMCore* cpu, int immediate);

static void _slice(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(cyclesLate);
	struct DS* ds = context;
	uint32_t cycles = mTimingCurrentTime(timing) - ds->sliceStart;
	if (ds->activeCpu == ds->ds9.cpu) {
		ds->activeCpu = ds->ds7.cpu;
		ds->cycleDrift += cycles;
		cycles = ds->cycleDrift >> 1;
		timing = &ds->ds7.timing;
	} else {
		ds->activeCpu = ds->ds9.cpu;
		ds->cycleDrift -= cycles << 1;
		cycles = ds->cycleDrift + SLICE_CYCLES;
		timing = &ds->ds9.timing;
	}
	mTimingSchedule(timing, &ds->slice, cycles);
	ds->sliceStart = mTimingCurrentTime(timing);
	ds->earlyExit = true;
}

static void _divide(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DS* ds = context;
	ds->memory.io9[DS9_REG_DIVCNT >> 1] &= ~0x8000;
	int64_t numerator;
	int64_t denominator;
	LOAD_64LE(numerator, DS9_REG_DIV_NUMER_0, ds->memory.io9);
	LOAD_64LE(denominator, DS9_REG_DIV_DENOM_0, ds->memory.io9);
	bool max = false;
	switch (ds->memory.io9[DS9_REG_DIVCNT >> 1] & 0x3) {
	case 0:
		numerator = (int64_t)(int32_t) numerator;
	case 1:
	case 3:
		denominator = (int64_t)(int32_t) denominator;
		break;
	}
	if (numerator == INT64_MIN) {
		max = true;
	}
	if (!denominator) {
		ds->memory.io9[DS9_REG_DIVCNT >> 1] |= 0x4000;
		STORE_64LE(numerator, DS9_REG_DIVREM_RESULT_0, ds->memory.io9);
		numerator >>= 63LL;
		numerator = -numerator;
		STORE_64LE(numerator, DS9_REG_DIV_RESULT_0, ds->memory.io9);
		return;
	}
	if (denominator == -1LL && max) {
		ds->memory.io9[DS9_REG_DIVCNT >> 1] |= 0x4000;
		STORE_64LE(numerator, DS9_REG_DIV_RESULT_0, ds->memory.io9);
		return;
	}
	ds->memory.io9[DS9_REG_DIVCNT >> 1] &= ~0x4000;
	int64_t result = numerator / denominator;
	int64_t remainder = numerator % denominator; // TODO: defined behavior for negative denominator?
	STORE_64LE(result, DS9_REG_DIV_RESULT_0, ds->memory.io9);
	STORE_64LE(remainder, DS9_REG_DIVREM_RESULT_0, ds->memory.io9);
}

	static void _sqrt(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DS* ds = context;
	ds->memory.io9[DS9_REG_SQRTCNT >> 1] &= ~0x8000;
	uint64_t param;
	LOAD_64LE(param, DS9_REG_SQRT_PARAM_0, ds->memory.io9);
	if (!(ds->memory.io9[DS9_REG_SQRTCNT >> 1] & 1)) {
		param &= 0xFFFFFFFFULL;
	}

	uint64_t result = 0;
	uint64_t bit = 0x4000000000000000ULL; // The second-to-top bit is set: 1 << 30 for 32 bits

	// "bit" starts at the highest power of four <= the argument.
	while (bit > param) {
		bit >>= 2;
	}

	while (bit != 0) {
		if (param >= param + bit) {
			param -= param + bit;
			param = (result >> 1) + bit;
		} else {
			param >>= 1;
		}
		bit >>= 2;
	}
	STORE_32LE(result, DS9_REG_SQRT_RESULT_LO, ds->memory.io9);
}

void DSCreate(struct DS* ds) {
	ds->d.id = DS_COMPONENT_MAGIC;
	ds->d.init = DSInit;
	ds->d.deinit = NULL;
	ds->ds7.p = ds;
	ds->ds9.p = ds;
	ds->ds7.cpu = NULL;
	ds->ds9.cpu = NULL;
	ds->ds7.ipc = &ds->ds9;
	ds->ds9.ipc = &ds->ds7;
}

static void DSInit(void* cpu, struct mCPUComponent* component) {
	struct DS* ds = (struct DS*) component;
	struct ARMCore* core = cpu;
	if (!ds->ds7.cpu) {
		// The ARM7 must get initialized first
		ds->ds7.cpu = core;
		ds->debugger = 0;
		ds->sync = 0;
		return;
	}
	ds->ds9.cpu = cpu;
	ds->activeCpu = NULL;

	ds->ds9.cpu->cp15.r1.c0 = ARMControlRegFillVE(0);

	ds->slice.name = "DS CPU Time Slicing";
	ds->slice.callback = _slice;
	ds->slice.context = ds;
	ds->slice.priority = UINT_MAX;

	CircleBufferInit(&ds->ds7.fifo, 64);
	CircleBufferInit(&ds->ds9.fifo, 64);

	DS7InterruptHandlerInit(&ds->ds7.cpu->irqh);
	DS9InterruptHandlerInit(&ds->ds9.cpu->irqh);
	DSMemoryInit(ds);
	DSDMAInit(ds);

	DSVideoInit(&ds->video);
	ds->video.p = ds;

	ds->ds7.springIRQ = 0;
	ds->ds9.springIRQ = 0;
	DSTimerInit(ds);
	ds->keySource = NULL;
	ds->rtcSource = NULL;
	ds->rumble = NULL;

	ds->romVf = NULL;

	ds->keyCallback = NULL;

	ds->divEvent.name = "DS Hardware Divide";
	ds->divEvent.callback = _divide;
	ds->divEvent.context = ds;
	ds->divEvent.priority = 0x50;

	ds->sqrtEvent.name = "DS Hardware Sqrt";
	ds->sqrtEvent.callback = _sqrt;
	ds->sqrtEvent.context = ds;
	ds->sqrtEvent.priority = 0x51;

	mTimingInit(&ds->ds7.timing, &ds->ds7.cpu->cycles, &ds->ds7.cpu->nextEvent);
	mTimingInit(&ds->ds9.timing, &ds->ds9.cpu->cycles, &ds->ds9.cpu->nextEvent);
}

void DSUnloadROM(struct DS* ds) {
	if (ds->romVf) {
		ds->romVf->close(ds->romVf);
		ds->romVf = NULL;
	}
}

void DSDestroy(struct DS* ds) {
	CircleBufferDeinit(&ds->ds7.fifo);
	CircleBufferDeinit(&ds->ds9.fifo);
	DSUnloadROM(ds);
	DSMemoryDeinit(ds);
	mTimingDeinit(&ds->ds7.timing);
	mTimingDeinit(&ds->ds9.timing);
}

void DS7InterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = DS7Reset;
	irqh->processEvents = DS7ProcessEvents;
	irqh->swi16 = DS7Swi16;
	irqh->swi32 = DS7Swi32;
	irqh->hitIllegal = DSIllegal;
	irqh->readCPSR = DS7TestIRQ;
	irqh->writeCP15 = NULL;
	irqh->readCP15 = NULL;
	irqh->hitStub = DSHitStub;
	irqh->bkpt16 = DSBreakpoint;
	irqh->bkpt32 = DSBreakpoint;
}

void DS9InterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = DS9Reset;
	irqh->processEvents = DS9ProcessEvents;
	irqh->swi16 = DS9Swi16;
	irqh->swi32 = DS9Swi32;
	irqh->hitIllegal = DSIllegal;
	irqh->readCPSR = DS9TestIRQ;
	irqh->writeCP15 = DS9WriteCP15;
	irqh->readCP15 = DS9ReadCP15;
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
	mTimingClear(&ds->ds7.timing);
	CircleBufferInit(&ds->ds7.fifo, 64);
	DSMemoryReset(ds);
	DSDMAReset(&ds->ds7);
	DS7IOInit(ds);

	struct DSCartridge* header = ds->romVf->map(ds->romVf, sizeof(*header), MAP_READ);
	if (header) {
		memcpy(&ds->memory.ram[0x3FFE00 >> 2], header, 0x170);
		DS7IOWrite32(ds, DS_REG_ROMCNT_LO, header->busTiming | 0x2700000);
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
	mTimingClear(&ds->ds9.timing);
	CircleBufferInit(&ds->ds9.fifo, 64);
	DSVideoReset(&ds->video);
	DSDMAReset(&ds->ds9);
	DS9IOInit(ds);

	ds->activeCpu = cpu;
	mTimingSchedule(&ds->ds9.timing, &ds->slice, SLICE_CYCLES);
	ds->cycleDrift = 0;
	ds->sliceStart = mTimingCurrentTime(&ds->ds9.timing);

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

static void DS7ProcessEvents(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;
	DSProcessEvents(&ds->ds7);
}

static void DS9ProcessEvents(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;
	DSProcessEvents(&ds->ds9);
}

static void DSProcessEvents(struct DSCommon* dscore) {
	struct ARMCore* cpu = dscore->cpu;
	struct DS* ds = dscore->p;
	if (dscore->springIRQ && !cpu->cpsr.i) {
		ARMRaiseIRQ(cpu);
		dscore->springIRQ = 0;
	}

	int32_t nextEvent = cpu->nextEvent;
	while (cpu->cycles >= nextEvent) {
		int32_t cycles = cpu->cycles;

		cpu->cycles = 0;
		cpu->nextEvent = INT_MAX;

#ifndef NDEBUG
		if (cycles < 0) {
			mLOG(DS, FATAL, "Negative cycles passed: %i", cycles);
		}
#endif
		nextEvent = cycles;
		do {
			nextEvent = mTimingTick(&dscore->timing, nextEvent);
		} while (ds->cpuBlocked);

		cpu->nextEvent = nextEvent;

		if (ds->earlyExit) {
			ds->earlyExit = false;
			break;
		}
		if (cpu->halted) {
			cpu->cycles = nextEvent;
		}
#ifndef NDEBUG
		else if (nextEvent < 0) {
			mLOG(DS, FATAL, "Negative cycles will pass: %i", nextEvent);
		}
#endif
	}
}

void DSRunLoop(struct DS* ds) {
	if (ds->activeCpu == ds->ds9.cpu) {
		ARMv5RunLoop(ds->ds9.cpu);
	} else {
		ARMv4RunLoop(ds->ds7.cpu);
	}
}

void DS7Step(struct DS* ds) {
	while (ds->activeCpu == ds->ds9.cpu) {
		ARMv5RunLoop(ds->ds9.cpu);
	}
	ARMv4Run(ds->ds7.cpu);
}

void DS9Step(struct DS* ds) {
	while (ds->activeCpu == ds->ds7.cpu) {
		ARMv4RunLoop(ds->ds7.cpu);
	}
	ARMv5Run(ds->ds9.cpu);
}

void DSAttachDebugger(struct DS* ds, struct mDebugger* debugger) {
	ds->debugger = (struct ARMDebugger*) debugger->platform;
	ds->ds7.cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ds->ds9.cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ARMHotplugAttach(ds->ds7.cpu, CPU_COMPONENT_DEBUGGER);
	ARMHotplugAttach(ds->ds9.cpu, CPU_COMPONENT_DEBUGGER);
}

void DSDetachDebugger(struct DS* ds) {
	ds->debugger = NULL;
	ARMHotplugDetach(ds->ds7.cpu, CPU_COMPONENT_DEBUGGER);
	ARMHotplugDetach(ds->ds9.cpu, CPU_COMPONENT_DEBUGGER);
	ds->ds7.cpu->components[CPU_COMPONENT_DEBUGGER] = NULL;
	ds->ds9.cpu->components[CPU_COMPONENT_DEBUGGER] = NULL;
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
	return memcmp(signature, DS_ROM_MAGIC, sizeof(signature)) == 0 || memcmp(signature, DS_ROM_MAGIC_2, sizeof(signature)) == 0;
}

bool DSIsBIOS7(struct VFile* vf) {
	size_t size = vf->size(vf);
	void* data = NULL;
	uint32_t crc;
	if (size == DS7_SIZE_BIOS) {
		data = vf->map(vf, size, MAP_READ);
	}
	if (!data) {
		return false;
	}
	crc = doCrc32(data, size);
	vf->unmap(vf, data, size);
	return crc == DS7_BIOS_CHECKSUM;
}

bool DSIsBIOS9(struct VFile* vf) {
	size_t size = vf->size(vf);
	void* data = NULL;
	uint32_t crc;
	if (size == DS9_SIZE_BIOS) {
		data = vf->map(vf, 0x1000, MAP_READ);
	} else if (size == 0x1000) {
		data = vf->map(vf, 0x1000, MAP_READ);
	}
	if (!data) {
		return false;
	}
	crc = doCrc32(data, 0x1000);
	vf->unmap(vf, data, 0x1000);
	return crc == DS9_BIOS_CHECKSUM;
}

bool DSLoadBIOS(struct DS* ds, struct VFile* vf) {
	size_t size = vf->size(vf);
	void* data = NULL;
	uint32_t crc;
	if (size == DS7_SIZE_BIOS) {
		data = vf->map(vf, size, MAP_READ);
	} else if (size == 0x1000) {
		data = calloc(DS9_SIZE_BIOS, 1);
		vf->read(vf, data, size);
	} else if (size == DS9_SIZE_BIOS) {
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
	// TODO: TWL-?
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
	memcpy(out, &cart->title, 12);
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
	if (!ds->memory.io7[DS_REG_IME >> 1]) {
		return;
	}
	uint32_t test = (ds->memory.io7[DS_REG_IE_LO >> 1] & ds->memory.io7[DS_REG_IF_LO >> 1]);
	test |= (ds->memory.io7[DS_REG_IE_HI >> 1] & ds->memory.io7[DS_REG_IF_HI >> 1]) << 16;
	if (test) {
		ds->ds7.springIRQ = test;
		cpu->nextEvent = cpu->cycles;
	}
}

void DS9TestIRQ(struct ARMCore* cpu) {
	struct DS* ds = (struct DS*) cpu->master;
	if (!ds->memory.io9[DS_REG_IME >> 1]) {
		return;
	}
	uint32_t test = (ds->memory.io9[DS_REG_IE_LO >> 1] & ds->memory.io9[DS_REG_IF_LO >> 1]);
	test |= (ds->memory.io9[DS_REG_IE_HI >> 1] & ds->memory.io9[DS_REG_IF_HI >> 1]) << 16;
	if (test) {
		ds->ds9.springIRQ = test;
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
	switch (crm) {
	case 0:
		if (opcode2 == 4) {
			ARMHalt(cpu);
			return;
		}
		break;
	}
	mLOG(DS, STUB, "CP15 cache write: CRm: %i, Op2: %i, Value: 0x%08X", crm, opcode2, value);
}

static void _writeTCMControl(struct ARMCore* cpu, int crm, int opcode2, uint32_t value) {
	uint32_t base = ARMTCMControlGetBase(value) << 12;
	uint32_t size = 512 << ARMTCMControlGetVirtualSize(value);
	struct DS* ds = (struct DS*) cpu->master;
	mLOG(DS, DEBUG, "CP15 TCM control write: CRm: %i, Op2: %i, Base: %08X, Size: %08X", crm, opcode2, base, size);
	switch (opcode2) {
	case 0:
		cpu->cp15.r9.d = value;
		ds->memory.dtcmBase = base;
		ds->memory.dtcmSize = size;
		break;
	case 1:
		cpu->cp15.r9.i = value;
		ds->memory.itcmSize = size;
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

static uint32_t _readTCMControl(struct ARMCore* cpu, int crm, int opcode2) {
	switch (opcode2) {
	case 0:
		return cpu->cp15.r9.d;
	case 1:
		return cpu->cp15.r9.i;
	default:
		mLOG(DS, GAME_ERROR, "CP15 TCM control bad op2: %i", opcode2);
		return 0;
	}
}

uint32_t DS9ReadCP15(struct ARMCore* cpu, int crn, int crm, int opcode1, int opcode2) {
	switch (crn) {
	default:
		mLOG(DS, STUB, "CP15 unknown read: CRn: %i, CRm: %i, Op1: %i, Op2: %i", crn, crm, opcode1, opcode2);
		return 0;
	case 9:
		return _readTCMControl(cpu, crm, opcode2);
	}
}

void DSWriteIE(struct ARMCore* cpu, uint16_t* io, uint32_t value) {
	if (io[DS_REG_IME >> 1] && (value & io[DS_REG_IF_LO >> 1] || (value >> 16) & io[DS_REG_IF_HI >> 1])) {
		ARMRaiseIRQ(cpu);
	}
}
void DSWriteIME(struct ARMCore* cpu, uint16_t* io, uint16_t value) {
	if (value && (io[DS_REG_IE_LO >> 1] & io[DS_REG_IF_LO >> 1] || io[DS_REG_IE_HI >> 1] & io[DS_REG_IF_HI >> 1])) {
		ARMRaiseIRQ(cpu);
	}
}

void DSRaiseIRQ(struct ARMCore* cpu, uint16_t* io, enum DSIRQ irq) {
	if (irq < 16) {
		io[DS_REG_IF_LO >> 1] |= 1 << irq;
	} else {
		io[DS_REG_IF_HI >> 1] |= 1 << (irq - 16);
	}

	if ((irq < 16 && (io[DS_REG_IE_LO >> 1] & 1 << irq)) || (io[DS_REG_IE_HI >> 1] & 1 << (irq - 16))) {
		cpu->halted = 0;
		if (io[DS_REG_IME >> 1]) {
			ARMRaiseIRQ(cpu);
		}
	}
}

void DSFrameStarted(struct DS* ds) {
	struct mCoreCallbacks* callbacks = ds->coreCallbacks;
	if (callbacks && callbacks->videoFrameStarted) {
		callbacks->videoFrameStarted(callbacks->context);
	}
}

void DSFrameEnded(struct DS* ds) {
	struct mCoreCallbacks* callbacks = ds->coreCallbacks;
	if (callbacks && callbacks->videoFrameEnded) {
		callbacks->videoFrameEnded(callbacks->context);
	}
}
