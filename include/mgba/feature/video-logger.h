/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_LOGGER_H
#define VIDEO_LOGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/core.h>

#include <mgba-util/circle-buffer.h>

#define mVL_MAX_CHANNELS 32

enum mVideoLoggerDirtyType {
	DIRTY_DUMMY = 0,
	DIRTY_FLUSH,
	DIRTY_SCANLINE,
	DIRTY_REGISTER,
	DIRTY_OAM,
	DIRTY_PALETTE,
	DIRTY_VRAM,
	DIRTY_FRAME,
	DIRTY_RANGE,
	DIRTY_BUFFER,
};

enum mVideoLoggerEvent {
	LOGGER_EVENT_NONE = 0,
	LOGGER_EVENT_INIT,
	LOGGER_EVENT_DEINIT,
	LOGGER_EVENT_RESET,
	LOGGER_EVENT_GET_PIXELS,
};

enum mVideoLoggerInjectionPoint {
	LOGGER_INJECTION_IMMEDIATE = 0,
	LOGGER_INJECTION_FIRST_SCANLINE,
};

struct mVideoLoggerDirtyInfo {
	enum mVideoLoggerDirtyType type;
	uint32_t address;
	uint32_t value;
	uint32_t value2;
};

struct VFile;
struct mVideoLogger {
	bool (*writeData)(struct mVideoLogger* logger, const void* data, size_t length);
	bool (*readData)(struct mVideoLogger* logger, void* data, size_t length, bool block);
	void (*postEvent)(struct mVideoLogger* logger, enum mVideoLoggerEvent event);
	void* dataContext;

	bool block;
	bool waitOnFlush;
	void (*init)(struct mVideoLogger*);
	void (*deinit)(struct mVideoLogger*);
	void (*reset)(struct mVideoLogger*);

	void (*lock)(struct mVideoLogger*);
	void (*unlock)(struct mVideoLogger*);
	void (*wait)(struct mVideoLogger*);
	void (*wake)(struct mVideoLogger*, int y);
	void* context;

	bool (*parsePacket)(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* packet);
	void (*handleEvent)(struct mVideoLogger* logger, enum mVideoLoggerEvent event);
	uint16_t* (*vramBlock)(struct mVideoLogger* logger, uint32_t address);

	size_t vramSize;
	size_t oamSize;
	size_t paletteSize;

	uint32_t* vramDirtyBitmap;
	uint32_t* oamDirtyBitmap;

	uint16_t* vram;
	uint16_t* oam;
	uint16_t* palette;

	const void* pixelBuffer;
	size_t pixelStride;
};

void mVideoLoggerRendererCreate(struct mVideoLogger* logger, bool readonly);
void mVideoLoggerRendererInit(struct mVideoLogger* logger);
void mVideoLoggerRendererDeinit(struct mVideoLogger* logger);
void mVideoLoggerRendererReset(struct mVideoLogger* logger);

void mVideoLoggerRendererWriteVideoRegister(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerRendererWriteVRAM(struct mVideoLogger* logger, uint32_t address);
void mVideoLoggerRendererWritePalette(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerRendererWriteOAM(struct mVideoLogger* logger, uint32_t address, uint16_t value);

void mVideoLoggerWriteBuffer(struct mVideoLogger* logger, uint32_t bufferId, uint32_t offset, uint32_t length, const void* data);

void mVideoLoggerRendererDrawScanline(struct mVideoLogger* logger, int y);
void mVideoLoggerRendererDrawRange(struct mVideoLogger* logger, int startX, int endX, int y);
void mVideoLoggerRendererFlush(struct mVideoLogger* logger);
void mVideoLoggerRendererFinishFrame(struct mVideoLogger* logger);

bool mVideoLoggerRendererRun(struct mVideoLogger* logger, bool block);
bool mVideoLoggerRendererRunInjected(struct mVideoLogger* logger);

struct mVideoLogContext;
void mVideoLoggerAttachChannel(struct mVideoLogger* logger, struct mVideoLogContext* context, size_t channelId);

struct mCore;
struct mVideoLogContext* mVideoLogContextCreate(struct mCore* core);

void mVideoLogContextSetCompression(struct mVideoLogContext*, bool enable);
void mVideoLogContextSetOutput(struct mVideoLogContext*, struct VFile*);
void mVideoLogContextWriteHeader(struct mVideoLogContext*, struct mCore* core);

bool mVideoLogContextLoad(struct mVideoLogContext*, struct VFile*);
void mVideoLogContextDestroy(struct mCore* core, struct mVideoLogContext*, bool closeVF);

void mVideoLogContextRewind(struct mVideoLogContext*, struct mCore*);
void* mVideoLogContextInitialState(struct mVideoLogContext*, size_t* size);

int mVideoLoggerAddChannel(struct mVideoLogContext*);

void mVideoLoggerInjectionPoint(struct mVideoLogger* logger, enum mVideoLoggerInjectionPoint);
void mVideoLoggerIgnoreAfterInjection(struct mVideoLogger* logger, uint32_t mask);
void mVideoLoggerInjectVideoRegister(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerInjectPalette(struct mVideoLogger* logger, uint32_t address, uint16_t value);
void mVideoLoggerInjectOAM(struct mVideoLogger* logger, uint32_t address, uint16_t value);

enum mPlatform mVideoLogIsCompatible(struct VFile*);
struct mCore* mVideoLogCoreFind(struct VFile*);

CXX_GUARD_END

#endif
