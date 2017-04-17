/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_LOGGER_H
#define VIDEO_LOGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define mVL_MAX_CHANNELS 32

enum mVideoLoggerDirtyType {
	DIRTY_DUMMY = 0,
	DIRTY_FLUSH,
	DIRTY_SCANLINE,
	DIRTY_REGISTER,
	DIRTY_OAM,
	DIRTY_PALETTE,
	DIRTY_VRAM
};

struct mVideoLoggerDirtyInfo {
	enum mVideoLoggerDirtyType type;
	uint32_t address;
	uint16_t value;
	uint32_t padding;
};

struct VFile;
struct mVideoLogger {
	bool (*writeData)(struct mVideoLogger* logger, const void* data, size_t length);
	bool (*readData)(struct mVideoLogger* logger, void* data, size_t length, bool block);

	bool block;
	void (*init)(struct mVideoLogger*);
	void (*deinit)(struct mVideoLogger*);
	void (*reset)(struct mVideoLogger*);

	void (*lock)(struct mVideoLogger*);
	void (*unlock)(struct mVideoLogger*);
	void (*wait)(struct mVideoLogger*);
	void (*wake)(struct mVideoLogger*, int y);
	void* context;

	bool (*parsePacket)(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* packet);
	uint16_t* (*vramBlock)(struct mVideoLogger* logger, uint32_t address);

	size_t vramSize;
	size_t oamSize;
	size_t paletteSize;

	uint32_t* vramDirtyBitmap;
	uint32_t* oamDirtyBitmap;

	uint16_t* vram;
	uint16_t* oam;
	uint16_t* palette;

	struct VFile* vf;
};

struct mVideoLogChannel {
	uint32_t type;
	void* initialState;
	size_t initialStateSize;
	struct VFile* channelData;
};

struct mVideoLogContext {
	void* initialState;
	size_t initialStateSize;
	uint32_t nChannels;
	struct mVideoLogChannel channels[mVL_MAX_CHANNELS];
};

struct mVideoLogHeader {
	char magic[4];
	uint32_t reserved;
	uint32_t platform;
	uint32_t padding;
	uint32_t initialStatePointer;
	uint32_t initialStateSize;
	uint32_t nChannels;
	uint32_t channelPointers[mVL_MAX_CHANNELS];
};

struct mVideoLogChannelHeader {
	uint32_t type;
	uint32_t channelInitialStatePointer;
	uint32_t channelInitialStateSize;
	uint32_t channelSize;
};

void mVideoLoggerRendererCreate(struct mVideoLogger* logger, bool readonly);
void mVideoLoggerRendererInit(struct mVideoLogger* logger);
void mVideoLoggerRendererDeinit(struct mVideoLogger* logger);
void mVideoLoggerRendererReset(struct mVideoLogger* logger);

void mVideoLoggerRendererWriteVideoRegister(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerRendererWriteVRAM(struct mVideoLogger* logger, uint32_t address);
void mVideoLoggerRendererWritePalette(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerRendererWriteOAM(struct mVideoLogger* logger, uint32_t address, uint16_t value);

void mVideoLoggerRendererDrawScanline(struct mVideoLogger* logger, int y);
void mVideoLoggerRendererFlush(struct mVideoLogger* logger);

bool mVideoLoggerRendererRun(struct mVideoLogger* logger, bool block);

struct mCore;
struct mVideoLogContext* mVideoLoggerCreate(struct mCore* core);
void mVideoLoggerDestroy(struct mCore* core, struct mVideoLogContext*);
void mVideoLoggerWrite(struct mCore* core, struct mVideoLogContext*, struct VFile*);

struct mCore* mVideoLogCoreFind(struct VFile*);
bool mVideoLogContextLoad(struct VFile*, struct mVideoLogContext*);

CXX_GUARD_END

#endif
