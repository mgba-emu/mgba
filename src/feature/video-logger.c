/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "video-logger.h"

#include <mgba/core/core.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#endif

const char mVL_MAGIC[] = "mVL\0";

const static struct mVLDescriptor {
	enum mPlatform platform;
	struct mCore* (*open)(void);
} _descriptors[] = {
#ifdef M_CORE_GBA
	{ PLATFORM_GBA, GBAVideoLogPlayerCreate },
#endif
	{ PLATFORM_NONE, 0 }
};

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length);
static bool _writeNull(struct mVideoLogger* logger, const void* data, size_t length);
static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block);

static inline size_t _roundUp(size_t value, int shift) {
	value += (1 << shift) - 1;
	return value >> shift;
}

void mVideoLoggerRendererCreate(struct mVideoLogger* logger, bool readonly) {
	if (readonly) {
		logger->writeData = _writeNull;
		logger->block = true;
	} else {
		logger->writeData = _writeData;
	}
	logger->readData = _readData;
	logger->vf = NULL;

	logger->init = NULL;
	logger->deinit = NULL;
	logger->reset = NULL;

	logger->lock = NULL;
	logger->unlock = NULL;
	logger->wait = NULL;
	logger->wake = NULL;
}

void mVideoLoggerRendererInit(struct mVideoLogger* logger) {
	logger->palette = anonymousMemoryMap(logger->paletteSize);
	logger->vram = anonymousMemoryMap(logger->vramSize);
	logger->oam = anonymousMemoryMap(logger->oamSize);

	logger->vramDirtyBitmap = calloc(_roundUp(logger->vramSize, 17), sizeof(uint32_t));
	logger->oamDirtyBitmap = calloc(_roundUp(logger->oamSize, 6), sizeof(uint32_t));

	if (logger->init) {
		logger->init(logger);
	}
}

void mVideoLoggerRendererDeinit(struct mVideoLogger* logger) {
	if (logger->deinit) {
		logger->deinit(logger);
	}

	mappedMemoryFree(logger->palette, logger->paletteSize);
	mappedMemoryFree(logger->vram, logger->vramSize);
	mappedMemoryFree(logger->oam, logger->oamSize);

	free(logger->vramDirtyBitmap);
	free(logger->oamDirtyBitmap);
}

void mVideoLoggerRendererReset(struct mVideoLogger* logger) {
	memset(logger->vramDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(logger->vramSize, 17));
	memset(logger->oamDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(logger->oamSize, 6));

	if (logger->reset) {
		logger->reset(logger);
	}
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
			for (j = 0; j < mVL_MAX_CHANNELS; ++j) {
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
	return false;
}

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length) {
	return logger->vf->write(logger->vf, data, length) == (ssize_t) length;
}

static bool _writeNull(struct mVideoLogger* logger, const void* data, size_t length) {
	UNUSED(logger);
	UNUSED(data);
	UNUSED(length);
	return false;
}

static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block) {
	UNUSED(block);
	return logger->vf->read(logger->vf, data, length) == (ssize_t) length;
}

struct mVideoLogContext* mVideoLoggerCreate(struct mCore* core) {
	struct mVideoLogContext* context = malloc(sizeof(*context));
	if (core) {
		core->startVideoLog(core, context);
	}
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
			STORE_32LE(context->initialStateSize, 0, &header.initialStateSize);
			pointer += written;
		} else {
			header.initialStatePointer = 0;
		}
	} else {
		header.initialStatePointer = 0;
	}

	size_t i;
	for (i = 0; i < context->nChannels && i < mVL_MAX_CHANNELS; ++i) {
		struct VFile* channel = context->channels[i].channelData;
		void* block = channel->map(channel, channel->size(channel), MAP_READ);

		struct mVideoLogChannelHeader chHeader = {0};
		STORE_32LE(context->channels[i].type, 0, &chHeader.type);
		STORE_32LE(channel->size(channel), 0, &chHeader.channelSize);

		if (context->channels[i].initialStateSize) {
			ssize_t written = vf->write(vf, context->channels[i].initialState, context->channels[i].initialStateSize);
			if (written > 0) {
				STORE_32LE(pointer, 0, &chHeader.channelInitialStatePointer);
				STORE_32LE(context->channels[i].initialStateSize, 0, &chHeader.channelInitialStateSize);
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

struct mCore* mVideoLogCoreFind(struct VFile* vf) {
	if (!vf) {
		return NULL;
	}
	struct mVideoLogHeader header = {{0}};
	vf->seek(vf, 0, SEEK_SET);
	ssize_t read = vf->read(vf, &header, sizeof(header));
	if (read != sizeof(header)) {
		return NULL;
	}
	if (memcmp(header.magic, mVL_MAGIC, sizeof(mVL_MAGIC)) != 0) {
		return NULL;
	}
	enum mPlatform platform;
	LOAD_32LE(platform, 0, &header.platform);

	const struct mVLDescriptor* descriptor;
	for (descriptor = &_descriptors[0]; descriptor->platform != PLATFORM_NONE; ++descriptor) {
		if (platform == descriptor->platform) {
			break;
		}
	}
	struct mCore* core = NULL;
	if (descriptor->open) {
		core = descriptor->open();
	}
	return core;
}

bool mVideoLogContextLoad(struct VFile* vf, struct mVideoLogContext* context) {
	if (!vf) {
		return false;
	}
	struct mVideoLogHeader header = {{0}};
	vf->seek(vf, 0, SEEK_SET);
	ssize_t read = vf->read(vf, &header, sizeof(header));
	if (read != sizeof(header)) {
		return false;
	}
	if (memcmp(header.magic, mVL_MAGIC, sizeof(mVL_MAGIC)) != 0) {
		return false;
	}

	// TODO: Error check
	uint32_t initialStatePointer;
	uint32_t initialStateSize;
	LOAD_32LE(initialStatePointer, 0, &header.initialStatePointer);
	LOAD_32LE(initialStateSize, 0, &header.initialStateSize);
	void* initialState = anonymousMemoryMap(initialStateSize);
	vf->read(vf, initialState, initialStateSize);
	context->initialState = initialState;
	context->initialStateSize = initialStateSize;

	uint32_t nChannels;
	LOAD_32LE(nChannels, 0, &header.nChannels);
	context->nChannels = nChannels;

	size_t i;
	for (i = 0; i < nChannels && i < mVL_MAX_CHANNELS; ++i) {
		uint32_t channelPointer;
		LOAD_32LE(channelPointer, 0, &header.channelPointers[i]);
		vf->seek(vf, channelPointer, SEEK_SET);

		struct mVideoLogChannelHeader chHeader;
		vf->read(vf, &chHeader, sizeof(chHeader));

		LOAD_32LE(context->channels[i].type, 0, &chHeader.type);
		LOAD_32LE(context->channels[i].initialStateSize, 0, &chHeader.channelInitialStateSize);

		LOAD_32LE(channelPointer, 0, &chHeader.channelInitialStatePointer);
		if (channelPointer) {
			off_t position = vf->seek(vf, 0, SEEK_CUR);
			vf->seek(vf, channelPointer, SEEK_SET);

			context->channels[i].initialState = anonymousMemoryMap(context->channels[i].initialStateSize);
			vf->read(vf, context->channels[i].initialState, context->channels[i].initialStateSize);
			vf->seek(vf, position, SEEK_SET);
		}

		uint32_t channelSize;
		LOAD_32LE(channelSize, 0, &chHeader.channelSize);
		struct VFile* vfm = VFileMemChunk(0, channelSize);

		while (channelSize) {
			uint8_t buffer[2048];
			ssize_t toRead = channelSize;
			if (toRead > (ssize_t) sizeof(buffer)) {
				toRead = sizeof(buffer);
			}
			toRead = vf->read(vf, buffer, toRead);
			if (toRead > 0) {
				channelSize -= toRead;
			} else {
				break;
			}
			vfm->write(vfm, buffer, toRead);
		}
		context->channels[i].channelData = vfm;
	}

	for (; i < mVL_MAX_CHANNELS; ++i) {
		context->channels[i].channelData = NULL;
	}
	return true;
}
