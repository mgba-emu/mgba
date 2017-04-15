/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/video-logger.h>

#include <mgba/core/core.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

const char mVL_MAGIC[] = "mVL\0";

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length);
static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block);

static inline size_t _roundUp(size_t value, int shift) {
	value += (1 << shift) - 1;
	return value >> shift;
}

void mVideoLoggerRendererCreate(struct mVideoLogger* logger) {
	logger->writeData = _writeData;
	logger->readData = _readData;
	logger->vf = NULL;
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

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length) {
	return logger->vf->write(logger->vf, data, length) == (ssize_t) length;
}

static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block) {
	return logger->vf->read(logger->vf, data, length) == (ssize_t) length || !block;
}

struct mVideoLogContext* mVideoLoggerCreate(struct mCore* core) {
	struct mVideoLogContext* context = malloc(sizeof(*context));
	core->startVideoLog(core, context);
	return context;
}

void mVideoLoggerDestroy(struct mCore* core, struct mVideoLogContext* context) {
	if (core) {
		core->endVideoLog(core);
	}
	free(context);
}

void mVideoLoggerWrite(struct mCore* core, struct mVideoLogContext* context, struct VFile* vf) {
	struct mVideoLogHeader header = {{0}};
	memcpy(header.magic, mVL_MAGIC, sizeof(mVL_MAGIC));

	enum mPlatform platform = core->platform(core);
	STORE_32LE(platform, 0, &header.platform);
	STORE_32LE(context->nChannels, 0, &header.nChannels);

	ssize_t pointer = vf->seek(vf, sizeof(header), SEEK_SET);
	if (context->initialStateSize) {
		ssize_t written = vf->write(vf, context->initialState, context->initialStateSize);
		if (written > 0) {
			STORE_32LE(pointer, 0, &header.initialStatePointer);
			pointer += written;
		} else {
			header.initialStatePointer = 0;
		}
	} else {
		header.initialStatePointer = 0;
	}

	size_t i;
	for (i = 0; i < context->nChannels && i < 32; ++i) {
		struct VFile* channel = context->channels[i].channelData;
		void* block = channel->map(channel, channel->size(channel), MAP_READ);

		struct mVideoLogChannelHeader chHeader = {0};
		STORE_32LE(context->channels[i].type, 0, &chHeader.type);
		STORE_32LE(channel->size(channel), 0, &chHeader.channelSize);

		if (context->channels[i].initialStateSize) {
			ssize_t written = vf->write(vf, context->channels[i].initialState, context->channels[i].initialStateSize);
			if (written > 0) {
				STORE_32LE(pointer, 0, &chHeader.channelInitialStatePointer);
				pointer += written;
			} else {
				chHeader.channelInitialStatePointer = 0;
			}
		}
		STORE_32LE(pointer, 0, &header.channelPointers[i]);
		ssize_t written = vf->write(vf, &chHeader, sizeof(chHeader));
		if (written != sizeof(chHeader)) {
			continue;
		}
		pointer += written;
		written = vf->write(vf, block, channel->size(channel));
		if (written != channel->size(channel)) {
			break;
		}
		pointer += written;
	}
	vf->seek(vf, 0, SEEK_SET);
	vf->write(vf, &header, sizeof(header));
}
