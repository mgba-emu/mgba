/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/sio/printer.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>


static bool GBPrinterInit(struct GBSIODriver* driver);
static void GBPrinterDeinit(struct GBSIODriver* driver);
static void GBPrinterWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBPrinterWriteSC(struct GBSIODriver* driver, uint8_t value);

void GBPrinterCreate(struct GBPrinter* printer) {
	printer->d.init = GBPrinterInit;
	printer->d.deinit = GBPrinterDeinit;
	printer->d.writeSB = GBPrinterWriteSB;
	printer->d.writeSC = GBPrinterWriteSC;
	printer->print = NULL;
}

bool GBPrinterInit(struct GBSIODriver* driver) {
	struct GBPrinter* printer = (struct GBPrinter*) driver;

	printer->checksum = 0;
	printer->command = 0;
	printer->remainingBytes = 0;
	printer->currentIndex = 0;
	printer->compression = false;
	printer->byte = 0;
	printer->next = GB_PRINTER_BYTE_MAGIC_0;
	printer->status = 0;
	printer->printWait = -1;

	printer->buffer = malloc(GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS / 2);

	return true;
}

void GBPrinterDeinit(struct GBSIODriver* driver) {
	struct GBPrinter* printer = (struct GBPrinter*) driver;
	free(printer->buffer);
}

static void GBPrinterWriteSB(struct GBSIODriver* driver, uint8_t value) {
	struct GBPrinter* printer = (struct GBPrinter*) driver;
	printer->byte = value;
}

static void _processByte(struct GBPrinter* printer) {
	switch (printer->command) {
	case GB_PRINTER_COMMAND_DATA:
		if (printer->currentIndex < GB_VIDEO_VERTICAL_PIXELS * GB_VIDEO_HORIZONTAL_PIXELS / 2) {
			printer->buffer[printer->currentIndex] = printer->byte;
			++printer->currentIndex;
		}
		break;
	case GB_PRINTER_COMMAND_PRINT:
		// TODO
		break;
	default:
		break;
	}
}

static uint8_t GBPrinterWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBPrinter* printer = (struct GBPrinter*) driver;
	if ((value & 0x81) == 0x81) {
		driver->p->pendingSB = 0;
		switch (printer->next) {
		case GB_PRINTER_BYTE_MAGIC_0:
			if (printer->byte == 0x88) {
				printer->next = GB_PRINTER_BYTE_MAGIC_1;
			} else {
				printer->next = GB_PRINTER_BYTE_MAGIC_0;
			}
			break;
		case GB_PRINTER_BYTE_MAGIC_1:
			if (printer->byte == 0x33) {
				printer->next = GB_PRINTER_BYTE_COMMAND;
			} else {
				printer->next = GB_PRINTER_BYTE_MAGIC_0;
			}
			break;
		case GB_PRINTER_BYTE_COMMAND:
			printer->checksum = printer->byte;
			printer->command = printer->byte;
			printer->next = GB_PRINTER_BYTE_COMPRESSION;
			break;
		case GB_PRINTER_BYTE_COMPRESSION:
			printer->checksum += printer->byte;
			printer->compression = printer->byte;
			printer->next = GB_PRINTER_BYTE_LENGTH_0;
			break;
		case GB_PRINTER_BYTE_LENGTH_0:
			printer->checksum += printer->byte;
			printer->remainingBytes = printer->byte;
			printer->next = GB_PRINTER_BYTE_LENGTH_1;
			break;
		case GB_PRINTER_BYTE_LENGTH_1:
			printer->checksum += printer->byte;
			printer->remainingBytes |= printer->byte << 8;
			if (printer->remainingBytes) {
				printer->next = GB_PRINTER_BYTE_DATA;
			} else {
				printer->next = GB_PRINTER_BYTE_CHECKSUM_0;
			}
			switch (printer->command) {
			case GB_PRINTER_COMMAND_INIT:
				printer->currentIndex = 0;
				printer->status &= ~(GB_PRINTER_STATUS_PRINT_REQ | GB_PRINTER_STATUS_READY);
				break;
			default:
				break;
			}
			break;
		case GB_PRINTER_BYTE_DATA:
			printer->checksum += printer->byte;
			if (!printer->compression) {
				_processByte(printer);
			} else {
				printer->next = printer->byte & 0x80 ? GB_PRINTER_BYTE_COMPRESSED_DATUM : GB_PRINTER_BYTE_UNCOMPRESSED_DATA;
				printer->remainingCmpBytes = (printer->byte & 0x7F) + 1;
				if (printer->byte & 0x80) {
					++printer->remainingCmpBytes;
				}
			}
			--printer->remainingBytes;
			if (!printer->remainingBytes) {
				printer->next = GB_PRINTER_BYTE_CHECKSUM_0;
			}
			break;
		case GB_PRINTER_BYTE_UNCOMPRESSED_DATA:
			printer->checksum += printer->byte;
			_processByte(printer);
			--printer->remainingCmpBytes;
			if (!printer->remainingCmpBytes) {
				printer->next = GB_PRINTER_BYTE_DATA;
			}
			--printer->remainingBytes;
			if (!printer->remainingBytes) {
				printer->next = GB_PRINTER_BYTE_CHECKSUM_0;
			}
			break;
		case GB_PRINTER_BYTE_COMPRESSED_DATUM:
			printer->checksum += printer->byte;
			while (printer->remainingCmpBytes) {
				_processByte(printer);
				--printer->remainingCmpBytes;
			}
			--printer->remainingBytes;
			if (!printer->remainingBytes) {
				printer->next = GB_PRINTER_BYTE_CHECKSUM_0;
			} else {
				printer->next = GB_PRINTER_BYTE_DATA;
			}
			break;
		case GB_PRINTER_BYTE_CHECKSUM_0:
			printer->checksum ^= printer->byte;
			printer->next = GB_PRINTER_BYTE_CHECKSUM_1;
			break;
		case GB_PRINTER_BYTE_CHECKSUM_1:
			printer->checksum ^= printer->byte << 8;
			printer->next = GB_PRINTER_BYTE_KEEPALIVE;
			break;
		case GB_PRINTER_BYTE_KEEPALIVE:
			driver->p->pendingSB = 0x81;
			printer->next = GB_PRINTER_BYTE_STATUS;
			break;
		case GB_PRINTER_BYTE_STATUS:
			switch (printer->command) {
			case GB_PRINTER_COMMAND_DATA:
				if (printer->currentIndex >= 0x280 && !(printer->status & GB_PRINTER_STATUS_CHECKSUM_ERROR)) {
					printer->status |= GB_PRINTER_STATUS_READY;
				}
				break;
			case GB_PRINTER_COMMAND_PRINT:
				if (printer->currentIndex >= GB_VIDEO_HORIZONTAL_PIXELS * 2) {
					printer->printWait = 0;
				}
				break;
			case GB_PRINTER_COMMAND_STATUS:
			default:
				break;
			}

			driver->p->pendingSB = printer->status;
			printer->next = GB_PRINTER_BYTE_MAGIC_0;
			break;
		}

		if (!printer->printWait) {
			printer->status &= ~GB_PRINTER_STATUS_READY;
			printer->status |= GB_PRINTER_STATUS_PRINTING | GB_PRINTER_STATUS_PRINT_REQ;
			if (printer->print) {
				size_t y;
				for (y = 0; y < printer->currentIndex / (2 * GB_VIDEO_HORIZONTAL_PIXELS); ++y) {
					uint8_t lineBuffer[GB_VIDEO_HORIZONTAL_PIXELS * 2];
					uint8_t* buffer = &printer->buffer[sizeof(lineBuffer) * y];
					size_t i;
					for (i = 0; i < sizeof(lineBuffer); i += 2) {
						uint8_t ilo = buffer[i + 0x0];
						uint8_t ihi = buffer[i + 0x1];
						uint8_t olo = 0;
						uint8_t ohi = 0;
						olo |= ((ihi & 0x80) >> 0) | ((ilo & 0x80) >> 1);
						olo |= ((ihi & 0x40) >> 1) | ((ilo & 0x40) >> 2);
						olo |= ((ihi & 0x20) >> 2) | ((ilo & 0x20) >> 3);
						olo |= ((ihi & 0x10) >> 3) | ((ilo & 0x10) >> 4);
						ohi |= ((ihi & 0x08) << 4) | ((ilo & 0x08) << 3);
						ohi |= ((ihi & 0x04) << 3) | ((ilo & 0x04) << 2);
						ohi |= ((ihi & 0x02) << 2) | ((ilo & 0x02) << 1);
						ohi |= ((ihi & 0x01) << 1) | ((ilo & 0x01) << 0);
						lineBuffer[(((i >> 1) & 0x7) * GB_VIDEO_HORIZONTAL_PIXELS / 4) + ((i >> 3) & ~1)] = olo;
						lineBuffer[(((i >> 1) & 0x7) * GB_VIDEO_HORIZONTAL_PIXELS / 4) + ((i >> 3) |  1)] = ohi;
					}
					memcpy(buffer, lineBuffer, sizeof(lineBuffer));
				}
				printer->print(printer, printer->currentIndex * 4 / GB_VIDEO_HORIZONTAL_PIXELS, printer->buffer);
			}
			printer->printWait = -1;
			printer->currentIndex = 0;
		} else if (printer->printWait > 0) {
			--printer->printWait;
		}

		printer->byte = 0;
	}
	return value;
}

void GBPrinterDonePrinting(struct GBPrinter* printer) {
	printer->status &= ~(GB_PRINTER_STATUS_PRINTING | GB_PRINTER_STATUS_PRINT_REQ);
}
