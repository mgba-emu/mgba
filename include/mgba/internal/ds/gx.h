/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_GX_H
#define DS_GX_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba-util/circle-buffer.h>

mLOG_DECLARE_CATEGORY(DS_GX);

DECL_BITFIELD(DSRegGXSTAT, uint32_t);
DECL_BIT(DSRegGXSTAT, TestBusy, 0);
DECL_BIT(DSRegGXSTAT, BoxTestResult, 1);
DECL_BITS(DSRegGXSTAT, PVMatrixStackLevel, 8, 5);
DECL_BIT(DSRegGXSTAT, ProjMatrixStackLevel, 13);
DECL_BIT(DSRegGXSTAT, MatrixStackBusy, 14);
DECL_BIT(DSRegGXSTAT, MatrixStackError, 15);
DECL_BITS(DSRegGXSTAT, FIFOEntries, 16, 9);
DECL_BIT(DSRegGXSTAT, FIFOFull, 24);
DECL_BIT(DSRegGXSTAT, FIFOLtHalf, 25);
DECL_BIT(DSRegGXSTAT, FIFOEmpty, 26);
DECL_BIT(DSRegGXSTAT, Busy, 27);
DECL_BITS(DSRegGXSTAT, DoIRQ, 30, 2);

enum DSGXCommand {
	DS_GX_CMD_NOP = 0,
	DS_GX_CMD_MTX_MODE = 0x10,
	DS_GX_CMD_MTX_PUSH = 0x11,
	DS_GX_CMD_MTX_POP = 0x12,
	DS_GX_CMD_MTX_STORE = 0x13,
	DS_GX_CMD_MTX_RESTORE = 0x14,
	DS_GX_CMD_MTX_IDENTITY = 0x15,
	DS_GX_CMD_MTX_LOAD_4x4 = 0x16,
	DS_GX_CMD_MTX_LOAD_4x3 = 0x17,
	DS_GX_CMD_MTX_MULT_4x4 = 0x18,
	DS_GX_CMD_MTX_MULT_4x3 = 0x19,
	DS_GX_CMD_MTX_MULT_3x3 = 0x1A,
	DS_GX_CMD_MTX_SCALE = 0x1B,
	DS_GX_CMD_MTX_TRANS = 0x1C,
	DS_GX_CMD_COLOR = 0x20,
	DS_GX_CMD_NORMAL = 0x21,
	DS_GX_CMD_TEXCOORD = 0x22,
	DS_GX_CMD_VTX_16 = 0x23,
	DS_GX_CMD_VTX_10 = 0x24,
	DS_GX_CMD_VTX_XY = 0x25,
	DS_GX_CMD_VTX_XZ = 0x26,
	DS_GX_CMD_VTX_YZ = 0x27,
	DS_GX_CMD_VTX_DIFF = 0x28,
	DS_GX_CMD_POLYGON_ATTR = 0x29,
	DS_GX_CMD_TEXIMAGE_PARAM = 0x2A,
	DS_GX_CMD_PLTT_BASE = 0x2B,
	DS_GX_CMD_DIF_AMB = 0x30,
	DS_GX_CMD_SPE_EMI = 0x31,
	DS_GX_CMD_LIGHT_VECTOR = 0x32,
	DS_GX_CMD_LIGHT_COLOR = 0x33,
	DS_GX_CMD_SHININESS = 0x34,
	DS_GX_CMD_BEGIN_VTXS = 0x40,
	DS_GX_CMD_END_VTXS = 0x41,
	DS_GX_CMD_SWAP_BUFFERS = 0x50,
	DS_GX_CMD_VIEWPORT = 0x60,
	DS_GX_CMD_BOX_TEST = 0x70,
	DS_GX_CMD_POS_TEST = 0x71,
	DS_GX_CMD_VEC_TEST = 0x72,

	DS_GX_CMD_MAX
};

#pragma pack(push, 1)
struct DSGXEntry {
	uint8_t command;
	uint8_t params[4];
};
#pragma pack(pop)

struct DS;
struct DSGX {
	struct DS* p;
	struct DSGXEntry pipe[4];
	struct CircleBuffer fifo;

	struct mTimingEvent fifoEvent;

	bool swapBuffers;
};

void DSGXInit(struct DSGX*);
void DSGXDeinit(struct DSGX*);
void DSGXReset(struct DSGX*);

uint16_t DSGXWriteRegister(struct DSGX*, uint32_t address, uint16_t value);
uint32_t DSGXWriteRegister32(struct DSGX*, uint32_t address, uint32_t value);

void DSGXSwapBuffers(struct DSGX*);
void DSGXUpdateGXSTAT(struct DSGX*);

CXX_GUARD_END

#endif
