/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/spi.h>

#include <mgba/internal/ds/ds.h>

mLOG_DEFINE_CATEGORY(DS_SPI, "DS SPI");

DSSPICNT DSSPIWriteControl(struct DS* ds, uint16_t control) {
	// TODO
	mLOG(DS_SPI, STUB, "Unimplemented control write: %04X", control);
	return control;
}

void DSSPIWrite(struct DS* ds, uint8_t datum) {
	mLOG(DS_SPI, STUB, "Unimplemented data write: %02X", datum);
	// TODO
}
