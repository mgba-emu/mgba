/* Copyright (c) 2013-2026 Jeffrey Pfau
 * Copyright (c) 2026 Felix Jones
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/everdrive-sd.h>

#include <mgba-util/vfs.h>

#include <string.h>

enum {
	REG_CFG = 0x00,
	REG_STATUS = 0x01,
	REG_SD_CMD = 0x08,
	REG_SD_DAT = 0x09,
	REG_SD_CFG = 0x0A,
	REG_SD_RAM = 0x0B,
	REG_KEY = 0x5A,
};

enum {
	STAT_SDC_TOUT = 2,
};

enum {
	CMD0_GO_IDLE_STATE = 0x40,
	CMD2_ALL_SEND_CID = 0x42,
	CMD3_SEND_RELATIVE_ADDR = 0x43,
	CMD6_SWITCH_FUNC = 0x46,
	CMD7_SELECT_DESELECT_CARD = 0x47,
	CMD8_SEND_IF_COND = 0x48,
	CMD9_SEND_CSD = 0x49,
	CMD12_STOP_TRANSMISSION = 0x4C,
	CMD17_READ_SINGLE_BLOCK = 0x51,
	CMD18_READ_MULTIPLE_BLOCK = 0x52,
	CMD24_WRITE_SINGLE_BLOCK = 0x58,
	CMD25_WRITE_MULTIPLE_BLOCK = 0x59,
	CMD55_APP_CMD = 0x77,
	CMD58_READ_OCR = 0x7A,
	ACMD41_SEND_OP_COND = 0x69,
};

enum {
	SD_CFG_WAIT_F0 = 0x08,
	SD_CFG_STRT_F0 = 0x10,
	SD_CFG_MODE_MASK = 0x1E,
	SD_CFG_MODE1 = 0x00,
};

enum {
	SD_SECTOR_SIZE = 512,
	SD_RESPONSE_R1_LEN = 6,
	SD_RESPONSE_R2_LEN = 17,
	SD_TOKEN_IDLE = 0xFF,
	SD_TOKEN_DATA_RESPONSE = 0xFE,
	SD_TOKEN_START_BLOCK = 0xF0,
};

static void _closeImage(struct GBAEverdriveSD* sd) {
	if (sd->image) {
		sd->image->close(sd->image);
		sd->image = NULL;
	}
}

static uint8_t _crc7(const uint8_t* data, size_t len) {
	unsigned crc = 0;
	while (len--) {
		crc ^= *data++;
		for (int i = 0; i < 8; ++i) {
			crc <<= 1;
			if (crc & (1 << 8)) {
				crc ^= 0x12;
			}
		}
	}
	return (uint8_t) ((crc & 0xFE) | 1);
}

static void _finalizeResponseCrc(uint8_t* response, const int len) {
	if (len < 2) {
		return;
	}
	if (len == SD_RESPONSE_R2_LEN) {
		response[len - 1] = _crc7(&response[1], len - 2);
	} else {
		response[len - 1] = _crc7(response, len - 1);
	}
}

static void _queueCmdResponse(struct GBAEverdriveSD* sd, const uint8_t* response, int len) {
	const int idleLeadBytes = len > 0 ? 1 : 0;
	if (len + idleLeadBytes > (int) sizeof(sd->cmdResponse)) {
		len = sizeof(sd->cmdResponse) - idleLeadBytes;
	}
	if (len > 0) {
		memcpy(sd->cmdResponse, response, len);
		_finalizeResponseCrc(sd->cmdResponse, len);
		memmove(&sd->cmdResponse[1], sd->cmdResponse, len);
		sd->cmdResponse[0] = SD_TOKEN_IDLE;
	}
	sd->cmdResponseLen = len + idleLeadBytes;
	sd->cmdResponsePos = 0;
}

static uint8_t _cmdRead(struct GBAEverdriveSD* sd) {
	if (sd->cmdResponsePos >= sd->cmdResponseLen) {
		return SD_TOKEN_IDLE;
	}
	return sd->cmdResponse[sd->cmdResponsePos++];
}

static uint8_t _cmdPeek(const struct GBAEverdriveSD* sd) {
	if (sd->cmdResponsePos >= sd->cmdResponseLen) {
		return SD_TOKEN_IDLE;
	}
	return sd->cmdResponse[sd->cmdResponsePos];
}

static bool _readSector(struct GBAEverdriveSD* sd, const uint32_t sector) {
	const off_t base = (off_t) sector * SD_SECTOR_SIZE;
	if (!sd->image || sd->image->seek(sd->image, base, SEEK_SET) < 0) {
		return false;
	}
	const ssize_t read = sd->image->read(sd->image, sd->dataBlock, sizeof(sd->dataBlock));
	if (read < 0) {
		return false;
	}
	if (read < (ssize_t) sizeof(sd->dataBlock)) {
		memset(&sd->dataBlock[read], 0, sizeof(sd->dataBlock) - read);
	}
	sd->dataBlockPos = 0;
	return true;
}

static bool _writeSector(const struct GBAEverdriveSD* sd, const uint32_t sector) {
	if (!sd->image || sd->readOnly) {
		return false;
	}
	const off_t base = (off_t) sector * SD_SECTOR_SIZE;
	if (sd->image->seek(sd->image, base, SEEK_SET) < 0) {
		return false;
	}
	const ssize_t written = sd->image->write(sd->image, sd->dataBlock, sizeof(sd->dataBlock));
	if (written != (ssize_t) sizeof(sd->dataBlock)) {
		return false;
	}
	if (sd->image->sync) {
		sd->image->sync(sd->image, NULL, 0);
	}
	return true;
}

static void _queueWriteDataResponse(struct GBAEverdriveSD* sd, const uint8_t responseCode) {
	sd->dataScriptLen = 0;
	sd->dataScript[sd->dataScriptLen++] = SD_TOKEN_IDLE;
	sd->dataScript[sd->dataScriptLen++] = SD_TOKEN_DATA_RESPONSE;
	sd->dataScript[sd->dataScriptLen++] = (responseCode >> 2) & 1;
	sd->dataScript[sd->dataScriptLen++] = (responseCode >> 1) & 1;
	sd->dataScript[sd->dataScriptLen++] = responseCode & 1;
	sd->dataScript[sd->dataScriptLen++] = SD_TOKEN_IDLE;
	sd->dataScriptPos = 0;
}

static uint8_t _dataReadByte(struct GBAEverdriveSD* sd) {
	if (sd->dataScriptPos < sd->dataScriptLen) {
		return sd->dataScript[sd->dataScriptPos++];
	}
	if (sd->readMulti && sd->dataBlockPos < (int) sizeof(sd->dataBlock)) {
		return sd->dataBlock[sd->dataBlockPos++];
	}
	return SD_TOKEN_IDLE;
}

static void _handleReadStart(struct GBAEverdriveSD* sd) {
	if (!sd->readMulti) {
		return;
	}
	if (!_readSector(sd, sd->currentReadSector)) {
		sd->status |= STAT_SDC_TOUT;
		return;
	}
	sd->status &= (uint8_t) ~STAT_SDC_TOUT;
	++sd->currentReadSector;
	sd->dataScriptLen = 2;
	sd->dataScript[0] = SD_TOKEN_START_BLOCK;
	sd->dataScript[1] = SD_TOKEN_IDLE;
	sd->dataScriptPos = 0;
}

static void _appendWriteByte(struct GBAEverdriveSD* sd, const uint8_t byte) {
	if (!sd->writeMulti) {
		return;
	}
	if (!sd->writeCapturing) {
		if (byte == SD_TOKEN_START_BLOCK) {
			sd->writeCapturing = true;
			sd->dataBlockPos = 0;
		}
		return;
	}
	if (sd->dataBlockPos < (int) sizeof(sd->dataBlock)) {
		sd->dataBlock[sd->dataBlockPos++] = byte;
	}
	if (sd->dataBlockPos == (int) sizeof(sd->dataBlock)) {
		const bool ok = _writeSector(sd, sd->currentWriteSector);
		if (ok) {
			++sd->currentWriteSector;
			_queueWriteDataResponse(sd, 0x2);
		} else {
			_queueWriteDataResponse(sd, 0x5);
		}
		sd->writeCapturing = false;
		sd->dataBlockPos = 0;
	}
}

static void _execCmd(struct GBAEverdriveSD* sd, const uint8_t cmd, const uint32_t arg) {
	uint8_t resp[18] = { 0 };

	switch (cmd) {
	case CMD0_GO_IDLE_STATE:
		sd->cardIdle = true;
		sd->appCmd = false;
		sd->readMulti = false;
		sd->writeMulti = false;
		resp[0] = 0x01;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD8_SEND_IF_COND:
		resp[0] = sd->cardIdle ? 0x01 : 0x00;
		resp[3] = 0x01;
		resp[4] = 0xAA;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD55_APP_CMD:
		sd->appCmd = true;
		resp[0] = sd->cardIdle ? 0x01 : 0x00;
		resp[1] = 0xFF;
		resp[2] = 0xFF;
		resp[3] = 0xFF;
		resp[4] = 0xFF;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case ACMD41_SEND_OP_COND:
		if (sd->appCmd) {
			sd->cardIdle = false;
			resp[0] = 0x00;
			resp[1] = 0xC0;
			_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		} else {
			resp[0] = 0x04;
			_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		}
		sd->appCmd = false;
		break;
	case CMD58_READ_OCR:
		resp[0] = sd->cardIdle ? 0x01 : 0x00;
		resp[1] = 0xC0;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD2_ALL_SEND_CID:
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R2_LEN);
		break;
	case CMD3_SEND_RELATIVE_ADDR:
		resp[0] = 0x00;
		resp[1] = 0x00;
		resp[2] = 0x01;
		resp[3] = 0x00;
		resp[4] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD7_SELECT_DESELECT_CARD:
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD9_SEND_CSD:
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R2_LEN);
		break;
	case CMD6_SWITCH_FUNC:
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD17_READ_SINGLE_BLOCK:
		sd->currentReadSector = arg;
		sd->readMulti = true;
		_handleReadStart(sd);
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD18_READ_MULTIPLE_BLOCK:
		sd->currentReadSector = arg;
		sd->readMulti = true;
		_queueCmdResponse(sd, NULL, 0);
		break;
	case CMD12_STOP_TRANSMISSION:
		sd->readMulti = false;
		sd->writeMulti = false;
		sd->writeCapturing = false;
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	case CMD24_WRITE_SINGLE_BLOCK:
	case CMD25_WRITE_MULTIPLE_BLOCK:
		sd->currentWriteSector = arg;
		sd->writeMulti = true;
		sd->writeCapturing = false;
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	default:
		resp[0] = 0x00;
		_queueCmdResponse(sd, resp, SD_RESPONSE_R1_LEN);
		break;
	}
}

static void _cmdWrite(struct GBAEverdriveSD* sd, const uint8_t value) {
	if (!sd->cmdPacketLen && (value & 0xC0) == 0x40) {
		sd->cmdPacketLen = 0;
		sd->cmdPacket[sd->cmdPacketLen++] = value;
		return;
	}
	if (!sd->cmdPacketLen) {
		return;
	}
	if (sd->cmdPacketLen < (int) sizeof(sd->cmdPacket)) {
		sd->cmdPacket[sd->cmdPacketLen++] = value;
	}
	if (sd->cmdPacketLen == (int) sizeof(sd->cmdPacket)) {
		const uint32_t arg = ((uint32_t) sd->cmdPacket[1] << 24)
			| ((uint32_t) sd->cmdPacket[2] << 16)
			| ((uint32_t) sd->cmdPacket[3] << 8)
			| sd->cmdPacket[4];
		_execCmd(sd, sd->cmdPacket[0], arg);
		sd->cmdPacketLen = 0;
	}
}

void GBAEverdriveSDInit(struct GBAEverdriveSD* sd) {
	memset(sd, 0, sizeof(*sd));
	GBAEverdriveSDReset(sd);
}

void GBAEverdriveSDDeinit(struct GBAEverdriveSD* sd) {
	_closeImage(sd);
}

void GBAEverdriveSDReset(struct GBAEverdriveSD* sd) {
	sd->status = 0;
	sd->cardIdle = true;
	sd->appCmd = false;
	sd->readMulti = false;
	sd->writeMulti = false;
	sd->writeCapturing = false;
	sd->cmdPacketLen = 0;
	sd->cmdResponseLen = 0;
	sd->cmdResponsePos = 0;
	sd->dataScriptLen = 0;
	sd->dataScriptPos = 0;
	sd->dataBlockPos = 0;
	memset(sd->regs, 0, sizeof(sd->regs));
}

void GBAEverdriveSDConfigure(struct GBAEverdriveSD* sd, bool enabled, const char* path) {
	sd->readOnly = false;
	_closeImage(sd);

	if (!enabled || !path || !path[0]) {
		return;
	}

	sd->image = VFileOpen(path, O_RDWR);
	if (!sd->image) {
		sd->image = VFileOpen(path, O_RDONLY);
		if (!sd->image) {
			return;
		}
		sd->readOnly = true;
	}

	GBAEverdriveSDReset(sd);
}

bool GBAEverdriveSDIsActive(const struct GBAEverdriveSD* sd) {
	return sd->image != NULL;
}

bool GBAEverdriveSDHandlesAddress(const uint32_t address) {
	if (address >= GBA_EVERDRIVE_SD_BASE && address < GBA_EVERDRIVE_SD_BASE + GBA_EVERDRIVE_SD_REG_SIZE) {
		return true;
	}
	if (address >= GBA_EVERDRIVE_SD_EEP_BASE && address < GBA_EVERDRIVE_SD_EEP_BASE + 0x20000) {
		return true;
	}
	return false;
}

uint16_t GBAEverdriveSDRead16(struct GBAEverdriveSD* sd, const uint32_t address) {
	if (address >= GBA_EVERDRIVE_SD_EEP_BASE) {
		return 0xFFFF;
	}

	const uint32_t reg = (address - GBA_EVERDRIVE_SD_BASE) >> 1;
	if (sd->readMulti && reg != REG_STATUS) {
		const uint8_t lo = _dataReadByte(sd);
		const uint8_t hi = _dataReadByte(sd);
		return ((uint16_t) hi << 8) | lo;
	}

	switch (reg) {
	case REG_STATUS:
		return sd->status;
	case REG_SD_CMD:
		return _cmdRead(sd);
	case REG_SD_CFG:
		return _cmdPeek(sd);
	case REG_SD_DAT: {
		if ((sd->regs[REG_SD_CFG] & SD_CFG_MODE_MASK) == SD_CFG_MODE1) {
			const uint8_t value = _dataReadByte(sd);
			return ((uint16_t) value << 8) | value;
		}
		const uint8_t lo = _dataReadByte(sd);
		const uint8_t hi = _dataReadByte(sd);
		return ((uint16_t) hi << 8) | lo;
	}
	default:
		if (reg < sizeof(sd->regs)) {
			return sd->regs[reg];
		}
		return 0xFFFF;
	}
}

void GBAEverdriveSDWrite16(struct GBAEverdriveSD* sd, const uint32_t address, const uint16_t value) {
	if (address >= GBA_EVERDRIVE_SD_EEP_BASE) {
		return;
	}

	const uint32_t reg = (address - GBA_EVERDRIVE_SD_BASE) >> 1;
	if (reg < sizeof(sd->regs)) {
		sd->regs[reg] = (uint8_t) value;
	}

	switch (reg) {
	case REG_SD_CMD:
		_cmdWrite(sd, (uint8_t) value);
		break;
	case REG_SD_CFG:
		if ((value & (SD_CFG_WAIT_F0 | SD_CFG_STRT_F0)) == (SD_CFG_WAIT_F0 | SD_CFG_STRT_F0)) {
			_handleReadStart(sd);
		}
		break;
	case REG_SD_DAT:
		_appendWriteByte(sd, (uint8_t) value);
		_appendWriteByte(sd, (uint8_t) (value >> 8));
		break;
	case REG_STATUS:
		sd->status = (uint8_t) value;
		break;
	case REG_KEY:
	case REG_CFG:
	case REG_SD_RAM:
	default:
		break;
	}
}
