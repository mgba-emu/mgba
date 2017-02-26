/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/gx.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>

mLOG_DEFINE_CATEGORY(DS_GX, "DS GX");

#define DS_GX_FIFO_SIZE 256

static const int32_t _gxCommandCycleBase[DS_GX_CMD_MAX] = {
	[DS_GX_CMD_NOP] = 0,
	[DS_GX_CMD_MTX_MODE] = 2,
	[DS_GX_CMD_MTX_PUSH] = 34,
	[DS_GX_CMD_MTX_POP] = 72,
	[DS_GX_CMD_MTX_STORE] = 34,
	[DS_GX_CMD_MTX_RESTORE] = 72,
	[DS_GX_CMD_MTX_IDENTITY] = 38,
	[DS_GX_CMD_MTX_LOAD_4x4] = 68,
	[DS_GX_CMD_MTX_LOAD_4x3] = 60,
	[DS_GX_CMD_MTX_MULT_4x4] = 70,
	[DS_GX_CMD_MTX_MULT_4x3] = 62,
	[DS_GX_CMD_MTX_MULT_3x3] = 56,
	[DS_GX_CMD_MTX_SCALE] = 44,
	[DS_GX_CMD_MTX_TRANS] = 44,
	[DS_GX_CMD_COLOR] = 2,
	[DS_GX_CMD_NORMAL] = 18,
	[DS_GX_CMD_TEXCOORD] = 2,
	[DS_GX_CMD_VTX_16] = 18,
	[DS_GX_CMD_VTX_10] = 16,
	[DS_GX_CMD_VTX_XY] = 16,
	[DS_GX_CMD_VTX_XZ] = 16,
	[DS_GX_CMD_VTX_YZ] = 16,
	[DS_GX_CMD_VTX_DIFF] = 16,
	[DS_GX_CMD_POLYGON_ATTR] = 2,
	[DS_GX_CMD_TEXIMAGE_PARAM] = 2,
	[DS_GX_CMD_PLTT_BASE] = 2,
	[DS_GX_CMD_DIF_AMB] = 8,
	[DS_GX_CMD_SPE_EMI] = 8,
	[DS_GX_CMD_LIGHT_VECTOR] = 12,
	[DS_GX_CMD_LIGHT_COLOR] = 2,
	[DS_GX_CMD_SHININESS] = 64,
	[DS_GX_CMD_BEGIN_VTXS] = 2,
	[DS_GX_CMD_END_VTXS] = 2,
	[DS_GX_CMD_SWAP_BUFFERS] = 784,
	[DS_GX_CMD_VIEWPORT] = 2,
	[DS_GX_CMD_BOX_TEST] = 206,
	[DS_GX_CMD_POS_TEST] = 18,
	[DS_GX_CMD_VEC_TEST] = 10,
};

static void _fifoRun(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSGX* gx = context;
	uint32_t cycles;
	while (true) {
		struct DSGXEntry entry = { 0 };
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.command);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[0]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[1]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[2]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[3]);
		cycles = _gxCommandCycleBase[entry.command];

		switch (entry.command) {
		case DS_GX_CMD_SWAP_BUFFERS:
			gx->swapBuffers = true;
			break;
		default:
			mLOG(DS_GX, STUB, "Unimplemented GX command %02X:%02X %02X %02X %02X", entry.command, entry.params[0], entry.params[1], entry.params[2], entry.params[3]);
			break;
		}
		if (CircleBufferSize(&gx->fifo)) {
			if (cycles <= cyclesLate) {
				cyclesLate -= cycles;
			} else {
				break;
			}
		} else {
			cycles = 0;
			break;
		}
	}
	DSGXUpdateGXSTAT(gx);
	if (cycles) {
		mTimingSchedule(&gx->p->ds9.timing, &gx->fifoEvent, cycles);
	}
}

void DSGXInit(struct DSGX* gx) {
	CircleBufferInit(&gx->fifo, sizeof(struct DSGXEntry) * DS_GX_FIFO_SIZE);
	gx->fifoEvent.name = "DS GX FIFO";
	gx->fifoEvent.priority = 0xC;
	gx->fifoEvent.context = gx;
	gx->fifoEvent.callback = _fifoRun;
}

void DSGXDeinit(struct DSGX* gx) {
	CircleBufferDeinit(&gx->fifo);
}

void DSGXReset(struct DSGX* gx) {
	CircleBufferClear(&gx->fifo);
	gx->swapBuffers = false;
}

void DSGXUpdateGXSTAT(struct DSGX* gx) {
	uint32_t value = gx->p->memory.io9[DS9_REG_GXSTAT_HI >> 1] << 16;
	value = DSRegGXSTATIsDoIRQ(value);

	size_t entries = CircleBufferSize(&gx->fifo) / sizeof(struct DSGXEntry);
	// XXX
	if (gx->swapBuffers) {
		entries++;
	}
	value = DSRegGXSTATSetFIFOEntries(value, entries);
	value = DSRegGXSTATSetFIFOLtHalf(value, entries < (DS_GX_FIFO_SIZE / 2));
	value = DSRegGXSTATSetFIFOEmpty(value, entries == 0);

	if ((DSRegGXSTATGetDoIRQ(value) == 1 && entries < (DS_GX_FIFO_SIZE / 2)) ||
		(DSRegGXSTATGetDoIRQ(value) == 2 && entries == 0)) {
		DSRaiseIRQ(gx->p->ds9.cpu, gx->p->ds9.memory.io, DS_IRQ_GEOM_FIFO);
	}

	value = DSRegGXSTATSetBusy(value, mTimingIsScheduled(&gx->p->ds9.timing, &gx->fifoEvent) || gx->swapBuffers);

	gx->p->memory.io9[DS9_REG_GXSTAT_HI >> 1] = value >> 16;
}

static void DSGXWriteFIFO(struct DSGX* gx, struct DSGXEntry entry) {
	uint32_t cycles = _gxCommandCycleBase[entry.command];
	if (!cycles) {
		return;
	}
	// TODO: Outstanding parameters
	if (CircleBufferSize(&gx->fifo) < (DS_GX_FIFO_SIZE * sizeof(entry))) {
		CircleBufferWrite8(&gx->fifo, entry.command);
		CircleBufferWrite8(&gx->fifo, entry.params[0]);
		CircleBufferWrite8(&gx->fifo, entry.params[1]);
		CircleBufferWrite8(&gx->fifo, entry.params[2]);
		CircleBufferWrite8(&gx->fifo, entry.params[3]);
		if (!mTimingIsScheduled(&gx->p->ds9.timing, &gx->fifoEvent)) {
			mTimingSchedule(&gx->p->ds9.timing, &gx->fifoEvent, 0);
		}
	} else {
		mLOG(DS_GX, STUB, "Unimplemented GX full");
	}
}

uint16_t DSGXWriteRegister(struct DSGX* gx, uint32_t address, uint16_t value) {
	uint16_t oldValue = gx->p->memory.io9[address >> 1];
	switch (address) {
	case DS9_REG_DISP3DCNT:
		mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		break;
	case DS9_REG_GXSTAT_LO:
		value = DSRegGXSTATIsMatrixStackError(value);
		if (value) {
			oldValue = DSRegGXSTATClearMatrixStackError(oldValue);
			oldValue = DSRegGXSTATClearProjMatrixStackLevel(oldValue);
		}
		value = oldValue;
		break;
	case DS9_REG_GXSTAT_HI:
		value = DSRegGXSTATIsDoIRQ(value << 16) >> 16;
		gx->p->memory.io9[address >> 1] = value;
		DSGXUpdateGXSTAT(gx);
		value = gx->p->memory.io9[address >> 1];
		break;
	default:
		if (address < DS9_REG_GXFIFO_00) {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		} else if (address < DS9_REG_GXSTAT_LO) {
			struct DSGXEntry entry = {
				.command = (address & 0x1FC) >> 2,
				.params = {
					value,
					value >> 8,
				}
			};
			if (entry.command < 0x80) {
				DSGXWriteFIFO(gx, entry);
			}
		} else {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		}
		break;
	}
	return value;
}

uint32_t DSGXWriteRegister32(struct DSGX* gx, uint32_t address, uint32_t value) {
	switch (address) {
	case DS9_REG_DISP3DCNT:
		mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		break;
	case DS9_REG_GXSTAT_LO:
		value = (value & 0xFFFF0000) | DSGXWriteRegister(gx, DS9_REG_GXSTAT_LO, value);
		value = (value & 0x0000FFFF) | (DSGXWriteRegister(gx, DS9_REG_GXSTAT_HI, value >> 16) << 16);
		break;
	default:
		if (address < DS9_REG_GXFIFO_00) {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		} else if (address < DS9_REG_GXSTAT_LO) {
			struct DSGXEntry entry = {
				.command = (address & 0x1FC) >> 2l,
				.params = {
					value,
					value >> 8,
					value >> 16,
					value >> 24
				}
			};
			DSGXWriteFIFO(gx, entry);
		} else {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		}
		break;
	}
	return value;
}

void DSGXSwapBuffers(struct DSGX* gx) {
	mLOG(DS_GX, STUB, "Unimplemented GX swap buffers");
	gx->swapBuffers = false;

	// TODO
	DSGXUpdateGXSTAT(gx);
	if (CircleBufferSize(&gx->fifo)) {
		mTimingSchedule(&gx->p->ds9.timing, &gx->fifoEvent, 0);
	}
}
