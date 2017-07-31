/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_PRINTER_H
#define GB_PRINTER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gb/interface.h>

enum GBPrinterPacketByte {
	GB_PRINTER_BYTE_MAGIC_0,
	GB_PRINTER_BYTE_MAGIC_1,
	GB_PRINTER_BYTE_COMMAND,
	GB_PRINTER_BYTE_COMPRESSION,
	GB_PRINTER_BYTE_LENGTH_0,
	GB_PRINTER_BYTE_LENGTH_1,
	GB_PRINTER_BYTE_DATA,
	GB_PRINTER_BYTE_CHECKSUM_0,
	GB_PRINTER_BYTE_CHECKSUM_1,
	GB_PRINTER_BYTE_KEEPALIVE,
	GB_PRINTER_BYTE_STATUS,

	GB_PRINTER_BYTE_COMPRESSED_DATUM,
	GB_PRINTER_BYTE_UNCOMPRESSED_DATA
};

enum GBPrinterStatus {
	GB_PRINTER_STATUS_CHECKSUM_ERROR = 0x01,
	GB_PRINTER_STATUS_PRINTING = 0x02,
	GB_PRINTER_STATUS_PRINT_REQ = 0x04,
	GB_PRINTER_STATUS_READY = 0x08,
	GB_PRINTER_STATUS_LOW_BATTERY = 0x10,
	GB_PRINTER_STATUS_TIMEOUT = 0x20,
	GB_PRINTER_STATUS_PAPER_JAM = 0x40,
	GB_PRINTER_STATUS_TEMPERATURE_ISSUE = 0x80
};

enum GBPrinterCommand {
	GB_PRINTER_COMMAND_INIT = 0x1,
	GB_PRINTER_COMMAND_PRINT = 0x2,
	GB_PRINTER_COMMAND_DATA = 0x4,
	GB_PRINTER_COMMAND_STATUS = 0xF,
};

struct GBPrinter {
	struct GBSIODriver d;

	void (*print)(struct GBPrinter*, int height, const uint8_t* data);

	uint8_t* buffer;
	uint16_t checksum;
	enum GBPrinterCommand command;
	uint16_t remainingBytes;
	uint8_t remainingCmpBytes;
	unsigned currentIndex;
	bool compression;

	uint8_t byte;
	enum GBPrinterPacketByte next;
	uint8_t status;
	int printWait;
};

void GBPrinterCreate(struct GBPrinter* printer);
void GBPrinterDonePrinting(struct GBPrinter* printer);

CXX_GUARD_END

#endif
