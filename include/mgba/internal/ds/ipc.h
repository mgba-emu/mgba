/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_IPC_H
#define DS_IPC_H

#include <mgba-util/common.h>

CXX_GUARD_START

DECL_BITFIELD(DSIPCFIFOCNT, int16_t);
DECL_BIT(DSIPCFIFOCNT, SendEmpty, 0);
DECL_BIT(DSIPCFIFOCNT, SendFull, 1);
DECL_BIT(DSIPCFIFOCNT, SendIRQ, 2);
DECL_BIT(DSIPCFIFOCNT, SendClear, 3);
DECL_BIT(DSIPCFIFOCNT, RecvEmpty, 8);
DECL_BIT(DSIPCFIFOCNT, RecvFull, 9);
DECL_BIT(DSIPCFIFOCNT, RecvIRQ, 10);
DECL_BIT(DSIPCFIFOCNT, Error, 14);
DECL_BIT(DSIPCFIFOCNT, Enable, 15);

struct ARMCore;
struct DSCommon;
void DSIPCWriteSYNC(struct ARMCore* remoteCpu, uint16_t* remoteIo, int16_t value);
int16_t DSIPCWriteFIFOCNT(struct DSCommon* dscore, int16_t value);
void DSIPCWriteFIFO(struct DSCommon* dscore, int32_t value);
int32_t DSIPCReadFIFO(struct DSCommon* dscore);

CXX_GUARD_END

#endif
