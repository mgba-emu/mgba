/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/ipc.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>

void DSIPCWriteSYNC(struct ARMCore* remoteCpu, uint16_t* remoteIo, int16_t value) {
	remoteIo[DS_REG_IPCSYNC >> 1] &= 0xFFF0;
	remoteIo[DS_REG_IPCSYNC >> 1] |= (value >> 8) & 0x0F;
	if (value & 0x2000 && remoteIo[DS_REG_IPCSYNC >> 1] & 0x4000) {
		mLOG(DS_IO, STUB, "Unimplemented IPC IRQ");
		UNUSED(remoteCpu);
	}
}

int16_t DSIPCWriteFIFOCNT(struct DSCommon* dscore, int16_t value) {
	value &= 0xC40C;
	int16_t oldValue = dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] & 0x4303;
	int16_t newValue = value | oldValue;
	// TODO: Does Enable set enabled on both ends?
	if (DSIPCFIFOCNTIsError(value)) {
		newValue = DSIPCFIFOCNTClearError(newValue);
	}
	if (DSIPCFIFOCNTIsSendClear(newValue)) {
		CircleBufferClear(&dscore->ipc->fifo);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillRecvEmpty(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		newValue = DSIPCFIFOCNTFillSendEmpty(newValue);
		newValue = DSIPCFIFOCNTClearSendClear(newValue);
	}
	return newValue;
}

void DSIPCWriteFIFO(struct DSCommon* dscore, int32_t value) {
	if (!DSIPCFIFOCNTIsEnable(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
		return;
	}
	CircleBufferWrite32(&dscore->ipc->fifo, value);
	size_t fullness = CircleBufferSize(&dscore->ipc->fifo);
	if (fullness == 4) {
		dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearSendEmpty(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearRecvEmpty(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		if (DSIPCFIFOCNTIsRecvIRQ(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
			// TODO: Adaptive time slicing?
			DSRaiseIRQ(dscore->ipc->cpu, dscore->ipc->memory.io, DS_IRQ_IPC_NOT_EMPTY);
		}
	} else if (fullness == 64) {
		dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillSendFull(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillRecvFull(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
	}
}

int32_t DSIPCReadFIFO(struct DSCommon* dscore) {
	if (!DSIPCFIFOCNTIsEnable(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
		return 0;
	}
	int32_t value = ((int32_t*) dscore->ipc->memory.io)[DS_REG_IPCFIFOSEND_LO >> 2]; // TODO: actual last value
	CircleBufferRead32(&dscore->fifo, &value);
	size_t fullness = CircleBufferSize(&dscore->fifo);
	dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearRecvFull(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
	dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearSendFull(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
	if (fullness == 0) {
		dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillRecvEmpty(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillSendEmpty(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		if (DSIPCFIFOCNTIsSendIRQ(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
			// TODO: Adaptive time slicing?
			DSRaiseIRQ(dscore->ipc->cpu, dscore->ipc->memory.io, DS_IRQ_IPC_NOT_EMPTY);
		}
	}
	return value;
}
