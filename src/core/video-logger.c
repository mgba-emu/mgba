/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/video-logger.h>

#include <mgba-util/memory.h>

static inline size_t _roundUp(size_t value, int shift) {
	value += (1 << shift) - 1;
	return value >> shift;
}

void mVideoLoggerRendererInit(struct mVideoLogger* logger) {
	logger->palette = anonymousMemoryMap(logger->paletteSize);
	logger->vram = anonymousMemoryMap(logger->vramSize);
	logger->oam = anonymousMemoryMap(logger->oamSize);

	logger->vramDirtyBitmap = calloc(_roundUp(logger->vramSize, 17), sizeof(uint32_t));
	logger->oamDirtyBitmap = calloc(_roundUp(logger->oamSize, 6), sizeof(uint32_t));
}

void mVideoLoggerRendererDeinit(struct mVideoLogger* logger) {
	mappedMemoryFree(logger->palette, logger->paletteSize);
	mappedMemoryFree(logger->vram, logger->vramSize);
	mappedMemoryFree(logger->oam, logger->oamSize);

	free(logger->vramDirtyBitmap);
	free(logger->oamDirtyBitmap);
}

void mVideoLoggerRendererReset(struct mVideoLogger* logger) {
	memset(logger->vramDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(logger->vramSize, 17));
	memset(logger->oamDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(logger->oamSize, 6));
}

void mVideoLoggerRendererWriteVideoRegister(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_REGISTER,
		address,
		value,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerRendererWriteVRAM(struct mVideoLogger* logger, uint32_t address) {
	int bit = 1 << (address >> 12);
	if (logger->vramDirtyBitmap[address >> 17] & bit) {
		return;
	}
	logger->vramDirtyBitmap[address >> 17] |= bit;
}

void mVideoLoggerRendererWritePalette(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_PALETTE,
		address,
		value,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerRendererWriteOAM(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_OAM,
		address,
		value,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerRendererDrawScanline(struct mVideoLogger* logger, int y) {
	size_t i;
	for (i = 0; i < _roundUp(logger->vramSize, 17); ++i) {
		if (logger->vramDirtyBitmap[i]) {
			uint32_t bitmap = logger->vramDirtyBitmap[i];
			logger->vramDirtyBitmap[i] = 0;
			int j;
			for (j = 0; j < 32; ++j) {
				if (!(bitmap & (1 << j))) {
					continue;
				}
				struct mVideoLoggerDirtyInfo dirty = {
					DIRTY_VRAM,
					j * 0x1000,
					0xABCD,
					0xDEADBEEF,
				};
				logger->writeData(logger, &dirty, sizeof(dirty));
				logger->writeData(logger, logger->vramBlock(logger, j * 0x1000), 0x1000);
			}
		}
	}
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_SCANLINE,
		y,
		0,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerRendererFlush(struct mVideoLogger* logger) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

bool mVideoLoggerRendererRun(struct mVideoLogger* logger) {
	struct mVideoLoggerDirtyInfo item = {0};
	while (logger->readData(logger, &item, sizeof(item), false)) {
		switch (item.type) {
		case DIRTY_REGISTER:
		case DIRTY_PALETTE:
		case DIRTY_OAM:
		case DIRTY_VRAM:
		case DIRTY_SCANLINE:
		case DIRTY_FLUSH:
			if (!logger->parsePacket(logger, &item)) {
				return true;
			}
			break;
		default:
			return false;
		}
	}
	return true;
}
