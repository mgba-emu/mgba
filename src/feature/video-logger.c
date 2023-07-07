/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/video-logger.h>

#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>
#include <mgba-util/math.h>

#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#endif
#ifdef M_CORE_GB
#include <mgba/gb/core.h>
#endif

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#define BUFFER_BASE_SIZE 0x20000
#define MAX_BLOCK_SIZE 0x800000

const char mVL_MAGIC[] = "mVL\0";

static const struct mVLDescriptor {
	enum mPlatform platform;
	struct mCore* (*open)(void);
} _descriptors[] = {
#ifdef M_CORE_GBA
	{ mPLATFORM_GBA, GBAVideoLogPlayerCreate },
#endif
#ifdef M_CORE_GB
	{ mPLATFORM_GB, GBVideoLogPlayerCreate },
#endif
	{ mPLATFORM_NONE, 0 }
};

enum mVLBlockType {
	mVL_BLOCK_DUMMY = 0,
	mVL_BLOCK_INITIAL_STATE,
	mVL_BLOCK_CHANNEL_HEADER,
	mVL_BLOCK_DATA,
	mVL_BLOCK_FOOTER = 0x784C566D
};

enum mVLHeaderFlag {
	mVL_FLAG_HAS_INITIAL_STATE = 1
};

struct mVLBlockHeader {
	uint32_t blockType;
	uint32_t length;
	uint32_t channelId;
	uint32_t flags;
};

enum mVLBlockFlag {
	mVL_FLAG_BLOCK_COMPRESSED = 1
};

struct mVideoLogHeader {
	char magic[4];
	uint32_t flags;
	uint32_t platform;
	uint32_t nChannels;
};

struct mVideoLogContext;
struct mVideoLogChannel {
	struct mVideoLogContext* p;

	uint32_t type;
	void* initialState;
	size_t initialStateSize;

	off_t currentPointer;
	size_t bufferRemaining;
#ifdef USE_ZLIB
	bool inflating;
	z_stream inflateStream;
#endif

	bool injecting;
	enum mVideoLoggerInjectionPoint injectionPoint;
	uint32_t ignorePackets;

	struct CircleBuffer injectedBuffer;
	struct CircleBuffer buffer;
};

struct mVideoLogContext {
	void* initialState;
	size_t initialStateSize;
	uint32_t nChannels;
	struct mVideoLogChannel channels[mVL_MAX_CHANNELS];

	bool write;
	bool compression;
	uint32_t activeChannel;
	struct VFile* backing;
};


static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length);
static bool _writeNull(struct mVideoLogger* logger, const void* data, size_t length);
static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block);

static ssize_t mVideoLoggerReadChannel(struct mVideoLogChannel* channel, void* data, size_t length);
static ssize_t mVideoLoggerWriteChannel(struct mVideoLogChannel* channel, const void* data, size_t length);

static inline size_t _roundUp(size_t value, int shift) {
	value += (1 << shift) - 1;
	return value >> shift;
}

void mVideoLoggerRendererCreate(struct mVideoLogger* logger, bool readonly) {
	if (readonly) {
		logger->writeData = _writeNull;
	} else {
		logger->writeData = _writeData;
	}
	logger->readData = _readData;
	logger->dataContext = NULL;

	logger->init = NULL;
	logger->deinit = NULL;
	logger->reset = NULL;

	logger->lock = NULL;
	logger->unlock = NULL;
	logger->wait = NULL;
	logger->wake = NULL;

	logger->block = readonly;
	logger->waitOnFlush = !readonly;
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
	int bit = 1U << (address >> 12);
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

static void _flushVRAM(struct mVideoLogger* logger) {
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
					0x1000,
					0xDEADBEEF,
				};
				logger->writeData(logger, &dirty, sizeof(dirty));
				logger->writeData(logger, logger->vramBlock(logger, j * 0x1000), 0x1000);
			}
		}
	}
}

void mVideoLoggerRendererDrawScanline(struct mVideoLogger* logger, int y) {
	_flushVRAM(logger);
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_SCANLINE,
		y,
		0,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerRendererDrawRange(struct mVideoLogger* logger, int startX, int endX, int y) {
	_flushVRAM(logger);
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_RANGE,
		y,
		startX,
		endX,
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
	if (logger->waitOnFlush && logger->wait) {
		logger->wait(logger);
	}
}

void mVideoLoggerRendererFinishFrame(struct mVideoLogger* logger) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_FRAME,
		0,
		0,
		0xDEADBEEF,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
}

void mVideoLoggerWriteBuffer(struct mVideoLogger* logger, uint32_t bufferId, uint32_t offset, uint32_t length, const void* data) {
	struct mVideoLoggerDirtyInfo dirty = {
		DIRTY_BUFFER,
		bufferId,
		offset,
		length,
	};
	logger->writeData(logger, &dirty, sizeof(dirty));
	logger->writeData(logger, data, length);
}

bool mVideoLoggerRendererRun(struct mVideoLogger* logger, bool block) {
	struct mVideoLogChannel* channel = logger->dataContext;
	uint32_t ignorePackets = 0;
	if (channel && channel->injectionPoint == LOGGER_INJECTION_IMMEDIATE && !channel->injecting) {
		mVideoLoggerRendererRunInjected(logger);
		ignorePackets = channel->ignorePackets;
	}
	struct mVideoLoggerDirtyInfo buffer = {0};
	struct mVideoLoggerDirtyInfo item = {0};
	while (logger->readData(logger, &buffer, sizeof(buffer), block)) {
		LOAD_32LE(item.type, 0, &buffer.type);
		if (ignorePackets & (1 << item.type)) {
			continue;
		}
		LOAD_32LE(item.address, 0, &buffer.address);
		LOAD_32LE(item.value, 0, &buffer.value);
		LOAD_32LE(item.value2, 0, &buffer.value2);
		switch (item.type) {
		case DIRTY_SCANLINE:
			if (channel && channel->injectionPoint == LOGGER_INJECTION_FIRST_SCANLINE && !channel->injecting && item.address == 0) {
				mVideoLoggerRendererRunInjected(logger);
				ignorePackets = channel->ignorePackets;
			}
			// Fall through
		case DIRTY_REGISTER:
		case DIRTY_PALETTE:
		case DIRTY_OAM:
		case DIRTY_VRAM:
		case DIRTY_FLUSH:
		case DIRTY_FRAME:
		case DIRTY_RANGE:
		case DIRTY_BUFFER:
			if (!logger->parsePacket(logger, &item)) {
				return true;
			}
			break;
		default:
			return false;
		}
	}
	return !block;
}

bool mVideoLoggerRendererRunInjected(struct mVideoLogger* logger) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->injecting = true;
	bool res = mVideoLoggerRendererRun(logger, false);
	channel->injecting = false;
	return res;	
}

void mVideoLoggerInjectionPoint(struct mVideoLogger* logger, enum mVideoLoggerInjectionPoint injectionPoint) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->injectionPoint = injectionPoint;	
}

void mVideoLoggerIgnoreAfterInjection(struct mVideoLogger* logger, uint32_t mask) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->ignorePackets = mask;	
}

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length) {
	struct mVideoLogChannel* channel = logger->dataContext;
	return mVideoLoggerWriteChannel(channel, data, length) == (ssize_t) length;
}

static bool _writeNull(struct mVideoLogger* logger, const void* data, size_t length) {
	struct mVideoLogChannel* channel = logger->dataContext;
	if (channel->injecting) {
		return mVideoLoggerWriteChannel(channel, data, length) == (ssize_t) length;
	}
	return false;
}

static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block) {
	UNUSED(block);
	struct mVideoLogChannel* channel = logger->dataContext;
	return mVideoLoggerReadChannel(channel, data, length) == (ssize_t) length;
}

#ifdef USE_ZLIB
static void _copyVf(struct VFile* dest, struct VFile* src) {
	size_t size = src->size(src);
	void* mem = src->map(src, size, MAP_READ);
	dest->write(dest, mem, size);
	src->unmap(src, mem, size);
}

static void _compress(struct VFile* dest, struct VFile* src) {
	uint8_t writeBuffer[0x800];
	uint8_t compressBuffer[0x400];
	z_stream zstr = {0};
	zstr.avail_in = 0;
	zstr.avail_out = sizeof(compressBuffer);
	zstr.next_out = (Bytef*) compressBuffer;
	if (deflateInit(&zstr, 9) != Z_OK) {
		return;
	}

	while (true) {
		size_t read = src->read(src, writeBuffer, sizeof(writeBuffer));
		if (!read) {
			break;
		}
		zstr.avail_in = read;
		zstr.next_in = (Bytef*) writeBuffer;
		while (zstr.avail_in) {
			if (deflate(&zstr, Z_NO_FLUSH) == Z_STREAM_ERROR) {
				break;
			}
			dest->write(dest, compressBuffer, sizeof(compressBuffer) - zstr.avail_out);
			zstr.avail_out = sizeof(compressBuffer);
			zstr.next_out = (Bytef*) compressBuffer;
		}
	}

	do {
		zstr.avail_out = sizeof(compressBuffer);
		zstr.next_out = (Bytef*) compressBuffer;
		zstr.avail_in = 0;
		int ret = deflate(&zstr, Z_FINISH);
		if (ret == Z_STREAM_ERROR) {
			break;
		}
		dest->write(dest, compressBuffer, sizeof(compressBuffer) - zstr.avail_out);
	} while (sizeof(compressBuffer) - zstr.avail_out);
}

static bool _decompress(struct VFile* dest, struct VFile* src, size_t compressedLength) {
	uint8_t fbuffer[0x400];
	uint8_t zbuffer[0x800];
	z_stream zstr = {0};
	zstr.avail_in = 0;
	zstr.avail_out = sizeof(zbuffer);
	zstr.next_out = (Bytef*) zbuffer;
	bool started = false;

	while (true) {
		size_t thisWrite = sizeof(zbuffer);
		size_t thisRead = 0;
		if (zstr.avail_in) {
			zstr.next_out = zbuffer;
			zstr.avail_out = thisWrite;
			thisRead = zstr.avail_in;
		} else if (compressedLength) {
			thisRead = sizeof(fbuffer);
			if (thisRead > compressedLength) {
				thisRead = compressedLength;
			}

			thisRead = src->read(src, fbuffer, thisRead);
			if (thisRead <= 0) {
				break;
			}

			zstr.next_in = fbuffer;
			zstr.avail_in = thisRead;
			zstr.next_out = zbuffer;
			zstr.avail_out = thisWrite;

			if (!started) {
				if (inflateInit(&zstr) != Z_OK) {
					break;
				}
				started = true;
			}
		} else {
			zstr.next_in = Z_NULL;
			zstr.avail_in = 0;
			zstr.next_out = zbuffer;
			zstr.avail_out = thisWrite;
		}

		int ret = inflate(&zstr, Z_NO_FLUSH);

		if (zstr.next_in != Z_NULL) {
			thisRead -= zstr.avail_in;
			compressedLength -= thisRead;
		}

		if (ret != Z_OK) {
			inflateEnd(&zstr);
			started = false;
			if (ret != Z_STREAM_END) {
				break;
			}
		}

		thisWrite = dest->write(dest, zbuffer, thisWrite - zstr.avail_out);

		if (!started) {
			break;
		}
	}
	return !compressedLength;
}
#endif

void mVideoLoggerAttachChannel(struct mVideoLogger* logger, struct mVideoLogContext* context, size_t channelId) {
	if (channelId >= mVL_MAX_CHANNELS) {
		return;
	}
	logger->dataContext = &context->channels[channelId];
}

struct mVideoLogContext* mVideoLogContextCreate(struct mCore* core) {
	struct mVideoLogContext* context = malloc(sizeof(*context));
	memset(context, 0, sizeof(*context));

	context->write = !!core;
	context->initialStateSize = 0;
	context->initialState = NULL;

#ifdef USE_ZLIB
	context->compression = true;
#else
	context->compression = false;
#endif

	if (core) {
		context->initialStateSize = core->stateSize(core);
		context->initialState = anonymousMemoryMap(context->initialStateSize);
		core->saveState(core, context->initialState);
		core->startVideoLog(core, context);
	}

	context->activeChannel = 0;
	return context;
}

void mVideoLogContextSetOutput(struct mVideoLogContext* context, struct VFile* vf) {
	context->backing = vf;
	vf->truncate(vf, 0);
	vf->seek(vf, 0, SEEK_SET);
}

void mVideoLogContextSetCompression(struct mVideoLogContext* context, bool compression) {
	context->compression = compression;
}

void mVideoLogContextWriteHeader(struct mVideoLogContext* context, struct mCore* core) {
	struct mVideoLogHeader header = { { 0 } };
	memcpy(header.magic, mVL_MAGIC, sizeof(header.magic));
	enum mPlatform platform = core->platform(core);
	STORE_32LE(platform, 0, &header.platform);
	STORE_32LE(context->nChannels, 0, &header.nChannels);

	uint32_t flags = 0;
	if (context->initialState) {
		flags |= mVL_FLAG_HAS_INITIAL_STATE;
	}
	STORE_32LE(flags, 0, &header.flags);
	context->backing->write(context->backing, &header, sizeof(header));
	if (context->initialState) {
		struct mVLBlockHeader chheader = { 0 };
		STORE_32LE(mVL_BLOCK_INITIAL_STATE, 0, &chheader.blockType);
#ifdef USE_ZLIB
		if (context->compression) {
			STORE_32LE(mVL_FLAG_BLOCK_COMPRESSED, 0, &chheader.flags);

			struct VFile* vfm = VFileMemChunk(NULL, 0);
			struct VFile* src = VFileFromConstMemory(context->initialState, context->initialStateSize);
			_compress(vfm, src);
			src->close(src);
			STORE_32LE(vfm->size(vfm), 0, &chheader.length);
			context->backing->write(context->backing, &chheader, sizeof(chheader));
			_copyVf(context->backing, vfm);
			vfm->close(vfm);
		} else
#endif
		{
			STORE_32LE(context->initialStateSize, 0, &chheader.length);
			context->backing->write(context->backing, &chheader, sizeof(chheader));
			context->backing->write(context->backing, context->initialState, context->initialStateSize);
		}
	}

 	size_t i;
	for (i = 0; i < context->nChannels; ++i) {
		struct mVLBlockHeader chheader = { 0 };
		STORE_32LE(mVL_BLOCK_CHANNEL_HEADER, 0, &chheader.blockType);
		STORE_32LE(i, 0, &chheader.channelId);
		context->backing->write(context->backing, &chheader, sizeof(chheader));
	}
}

bool _readBlockHeader(struct mVideoLogContext* context, struct mVLBlockHeader* header) {
	struct mVLBlockHeader buffer;
	if (context->backing->read(context->backing, &buffer, sizeof(buffer)) != sizeof(buffer)) {
		return false;
	}
	LOAD_32LE(header->blockType, 0, &buffer.blockType);
	LOAD_32LE(header->length, 0, &buffer.length);
	LOAD_32LE(header->channelId, 0, &buffer.channelId);
	LOAD_32LE(header->flags, 0, &buffer.flags);

	if (header->length > MAX_BLOCK_SIZE) {
		// Pre-emptively reject blocks that are too big.
		// If we encounter one, the file is probably corrupted.
		return false;
	}
	return true;
}

bool _readHeader(struct mVideoLogContext* context) {
	struct mVideoLogHeader header;
	context->backing->seek(context->backing, 0, SEEK_SET);
	if (context->backing->read(context->backing, &header, sizeof(header)) != sizeof(header)) {
		return false;
	}
	if (memcmp(header.magic, mVL_MAGIC, sizeof(header.magic)) != 0) {
		return false;
	}

	LOAD_32LE(context->nChannels, 0, &header.nChannels);
	if (context->nChannels > mVL_MAX_CHANNELS) {
		context->nChannels = 0;
		return false;
	}

	uint32_t flags;
	LOAD_32LE(flags, 0, &header.flags);
	if (flags & mVL_FLAG_HAS_INITIAL_STATE) {
		struct mVLBlockHeader header;
		if (!_readBlockHeader(context, &header)) {
			return false;
		}
		if (header.blockType != mVL_BLOCK_INITIAL_STATE || !header.length) {
			return false;
		}
		if (context->initialState) {
			mappedMemoryFree(context->initialState, context->initialStateSize);
			context->initialState = NULL;
			context->initialStateSize = 0;
		}
		if (header.flags & mVL_FLAG_BLOCK_COMPRESSED) {
#ifdef USE_ZLIB
			struct VFile* vfm = VFileMemChunk(NULL, 0);
			if (!_decompress(vfm, context->backing, header.length)) {
				vfm->close(vfm);
				return false;
			}
			context->initialStateSize = vfm->size(vfm);
			context->initialState = anonymousMemoryMap(context->initialStateSize);
			void* mem = vfm->map(vfm, context->initialStateSize, MAP_READ);
			memcpy(context->initialState, mem, context->initialStateSize);
			vfm->unmap(vfm, mem, context->initialStateSize);
			vfm->close(vfm);
#else
			return false;
#endif
		} else {
			context->initialStateSize = header.length;
			context->initialState = anonymousMemoryMap(header.length);
			context->backing->read(context->backing, context->initialState, context->initialStateSize);
		}
	}
	return true;
}

bool mVideoLogContextLoad(struct mVideoLogContext* context, struct VFile* vf) {
	context->backing = vf;

	if (!_readHeader(context)) {
		return false;
	}

	off_t pointer = context->backing->seek(context->backing, 0, SEEK_CUR);

	size_t i;
	for (i = 0; i < context->nChannels; ++i) {
		CircleBufferInit(&context->channels[i].injectedBuffer, BUFFER_BASE_SIZE);
		CircleBufferInit(&context->channels[i].buffer, BUFFER_BASE_SIZE);
		context->channels[i].bufferRemaining = 0;
		context->channels[i].currentPointer = pointer;
		context->channels[i].p = context;
#ifdef USE_ZLIB
		context->channels[i].inflating = false;
#endif
	}
	return true;
}

#ifdef USE_ZLIB
static void _flushBufferCompressed(struct mVideoLogContext* context) {
	struct CircleBuffer* buffer = &context->channels[context->activeChannel].buffer;
	if (!CircleBufferSize(buffer)) {
		return;
	}
	struct VFile* vfm = VFileMemChunk(NULL, 0);
	struct VFile* src = VFileFIFO(buffer);
	_compress(vfm, src);
	src->close(src);

	size_t size = vfm->size(vfm);

	struct mVLBlockHeader header = { 0 };
	STORE_32LE(mVL_BLOCK_DATA, 0, &header.blockType);
	STORE_32LE(context->activeChannel, 0, &header.channelId);
	STORE_32LE(mVL_FLAG_BLOCK_COMPRESSED, 0, &header.flags);
	STORE_32LE(size, 0, &header.length);

	context->backing->write(context->backing, &header, sizeof(header));
	_copyVf(context->backing, vfm);
	vfm->close(vfm);
}
#endif

static void _flushBuffer(struct mVideoLogContext* context) {
#ifdef USE_ZLIB
	if (context->compression) {
		_flushBufferCompressed(context);
		return;
	}
#endif

	struct CircleBuffer* buffer = &context->channels[context->activeChannel].buffer;
	if (!CircleBufferSize(buffer)) {
		return;
	}
	struct mVLBlockHeader header = { 0 };
	STORE_32LE(mVL_BLOCK_DATA, 0, &header.blockType);
	STORE_32LE(CircleBufferSize(buffer), 0, &header.length);
	STORE_32LE(context->activeChannel, 0, &header.channelId);

	context->backing->write(context->backing, &header, sizeof(header));

	uint8_t writeBuffer[0x800];
	while (CircleBufferSize(buffer)) {
		size_t read = CircleBufferRead(buffer, writeBuffer, sizeof(writeBuffer));
		context->backing->write(context->backing, writeBuffer, read);
	}
}

void mVideoLogContextDestroy(struct mCore* core, struct mVideoLogContext* context, bool closeVF) {
	if (context->write) {
		_flushBuffer(context);

		struct mVLBlockHeader header = { 0 };
		STORE_32LE(mVL_BLOCK_FOOTER, 0, &header.blockType);
		context->backing->write(context->backing, &header, sizeof(header));
	}

	if (core) {
		core->endVideoLog(core);
	}
	if (context->initialState) {
		mappedMemoryFree(context->initialState, context->initialStateSize);
	}

	size_t i;
	for (i = 0; i < context->nChannels; ++i) {
		CircleBufferDeinit(&context->channels[i].injectedBuffer);
		CircleBufferDeinit(&context->channels[i].buffer);
#ifdef USE_ZLIB
		if (context->channels[i].inflating) {
			inflateEnd(&context->channels[i].inflateStream);
			context->channels[i].inflating = false;
		}
#endif
	}

	if (closeVF && context->backing) {
		context->backing->close(context->backing);
	}

	free(context);
}

void mVideoLogContextRewind(struct mVideoLogContext* context, struct mCore* core) {
	_readHeader(context);
	if (core) {
		size_t size = core->stateSize(core);
		if (size <= context->initialStateSize) {
			core->loadState(core, context->initialState);
		} else {
			void* extendedState = anonymousMemoryMap(size);
			memcpy(extendedState, context->initialState, context->initialStateSize);
			core->loadState(core, extendedState);
			mappedMemoryFree(extendedState, size);
		}
	}

	off_t pointer = context->backing->seek(context->backing, 0, SEEK_CUR);

	size_t i;
	for (i = 0; i < context->nChannels; ++i) {
		CircleBufferClear(&context->channels[i].injectedBuffer);
		CircleBufferClear(&context->channels[i].buffer);
		context->channels[i].bufferRemaining = 0;
		context->channels[i].currentPointer = pointer;
#ifdef USE_ZLIB
		if (context->channels[i].inflating) {
			inflateEnd(&context->channels[i].inflateStream);
			context->channels[i].inflating = false;
		}
#endif
	}
}

void* mVideoLogContextInitialState(struct mVideoLogContext* context, size_t* size) {
	if (size) {
		*size = context->initialStateSize;
	}
	return context->initialState;
}

int mVideoLoggerAddChannel(struct mVideoLogContext* context) {
	if (context->nChannels >= mVL_MAX_CHANNELS) {
		return -1;
	}
	int chid = context->nChannels;
	++context->nChannels;
	context->channels[chid].p = context;
	CircleBufferInit(&context->channels[chid].injectedBuffer, BUFFER_BASE_SIZE);
	CircleBufferInit(&context->channels[chid].buffer, BUFFER_BASE_SIZE);
	context->channels[chid].injecting = false;
	context->channels[chid].injectionPoint = LOGGER_INJECTION_IMMEDIATE;
	context->channels[chid].ignorePackets = 0;
	return chid;
}

void mVideoLoggerInjectVideoRegister(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->injecting = true;
	mVideoLoggerRendererWriteVideoRegister(logger, address, value);
	channel->injecting = false;
}

void mVideoLoggerInjectPalette(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->injecting = true;
	mVideoLoggerRendererWritePalette(logger, address, value);
	channel->injecting = false;
}

void mVideoLoggerInjectOAM(struct mVideoLogger* logger, uint32_t address, uint16_t value) {
	struct mVideoLogChannel* channel = logger->dataContext;
	channel->injecting = true;
	mVideoLoggerRendererWriteOAM(logger, address, value);
	channel->injecting = false;
}

#ifdef USE_ZLIB
static size_t _readBufferCompressed(struct VFile* vf, struct mVideoLogChannel* channel, size_t length) {
	uint8_t fbuffer[0x400];
	uint8_t zbuffer[0x800];
	size_t read = 0;

	// TODO: Share with _decompress
	channel->inflateStream.avail_in = 0;
	while (length) {
		size_t thisWrite = sizeof(zbuffer);
		if (thisWrite > length) {
			thisWrite = length;
		}

		size_t thisRead = 0;
		if (channel->inflating && channel->inflateStream.avail_in) {
			channel->inflateStream.next_out = zbuffer;
			channel->inflateStream.avail_out = thisWrite;
			thisRead = channel->inflateStream.avail_in;
		} else if (channel->bufferRemaining) {
			thisRead = sizeof(fbuffer);
			if (thisRead > channel->bufferRemaining) {
				thisRead = channel->bufferRemaining;
			}

			thisRead = vf->read(vf, fbuffer, thisRead);
			if (thisRead <= 0) {
				break;
			}

			channel->inflateStream.next_in = fbuffer;
			channel->inflateStream.avail_in = thisRead;
			channel->inflateStream.next_out = zbuffer;
			channel->inflateStream.avail_out = thisWrite;

			if (!channel->inflating) {
				if (inflateInit(&channel->inflateStream) != Z_OK) {
					break;
				}
				channel->inflating = true;
			}
		} else {
			channel->inflateStream.next_in = Z_NULL;
			channel->inflateStream.avail_in = 0;
			channel->inflateStream.next_out = zbuffer;
			channel->inflateStream.avail_out = thisWrite;
		}

		int ret = inflate(&channel->inflateStream, Z_NO_FLUSH);

		if (channel->inflateStream.next_in != Z_NULL) {
			thisRead -= channel->inflateStream.avail_in;
			channel->currentPointer += thisRead;
			channel->bufferRemaining -= thisRead;
		}

		if (ret != Z_OK) {
			inflateEnd(&channel->inflateStream);
			channel->inflating = false;
			if (ret != Z_STREAM_END) {
				break;
			}
		}

		thisWrite = CircleBufferWrite(&channel->buffer, zbuffer, thisWrite - channel->inflateStream.avail_out);
		length -= thisWrite;
		read += thisWrite;

		if (!channel->inflating) {
			break;
		}
	}
	return read;
}
#endif

static void _readBuffer(struct VFile* vf, struct mVideoLogChannel* channel, size_t length) {
	uint8_t buffer[0x800];
	while (length) {
		size_t thisRead = sizeof(buffer);
		if (thisRead > length) {
			thisRead = length;
		}
		thisRead = vf->read(vf, buffer, thisRead);
		if (thisRead <= 0) {
			return;
		}
		size_t thisWrite = CircleBufferWrite(&channel->buffer, buffer, thisRead);
		length -= thisWrite;
		channel->bufferRemaining -= thisWrite;
		channel->currentPointer += thisWrite;
		if (thisWrite < thisRead) {
			break;
		}
	}
}

static bool _fillBuffer(struct mVideoLogContext* context, size_t channelId, size_t length) {
	struct mVideoLogChannel* channel = &context->channels[channelId];
	context->backing->seek(context->backing, channel->currentPointer, SEEK_SET);
	struct mVLBlockHeader header;
	while (length) {
		size_t bufferRemaining = channel->bufferRemaining;
		if (bufferRemaining) {
#ifdef USE_ZLIB
			if (channel->inflating) {
				length -= _readBufferCompressed(context->backing, channel, length);
				continue;
			}
#endif
			if (bufferRemaining > length) {
				bufferRemaining = length;
			}

			_readBuffer(context->backing, channel, bufferRemaining);
			length -= bufferRemaining;
			continue;
		}

		if (!_readBlockHeader(context, &header)) {
			return false;
		}
		if (header.blockType == mVL_BLOCK_FOOTER) {
			return true;
		}
		if (header.channelId != channelId || header.blockType != mVL_BLOCK_DATA) {
			context->backing->seek(context->backing, header.length, SEEK_CUR);
			continue;
		}
		channel->currentPointer = context->backing->seek(context->backing, 0, SEEK_CUR);
		if (!header.length) {
			continue;
		}
		channel->bufferRemaining = header.length;

		if (header.flags & mVL_FLAG_BLOCK_COMPRESSED) {
#ifdef USE_ZLIB
			length -= _readBufferCompressed(context->backing, channel, length);
#else
			return false;
#endif
		}
	}
	return true;
}

static ssize_t mVideoLoggerReadChannel(struct mVideoLogChannel* channel, void* data, size_t length) {
	struct mVideoLogContext* context = channel->p;
	unsigned channelId = channel - context->channels;
	if (channelId >= mVL_MAX_CHANNELS) {
		return 0;
	}
	struct CircleBuffer* buffer = &channel->buffer;
	if (channel->injecting) {
		buffer = &channel->injectedBuffer;
	}
	if (CircleBufferSize(buffer) >= length) {
		return CircleBufferRead(buffer, data, length);
	}
	ssize_t size = 0;
	if (CircleBufferSize(buffer)) {
		size = CircleBufferRead(buffer, data, CircleBufferSize(buffer));
		if (size <= 0) {
			return size;
		}
		data = (uint8_t*) data + size;
		length -= size;
	}
	if (channel->injecting || !_fillBuffer(context, channelId, BUFFER_BASE_SIZE)) {
		return size;
	}
	size += CircleBufferRead(buffer, data, length);
	return size;
}

static ssize_t mVideoLoggerWriteChannel(struct mVideoLogChannel* channel, const void* data, size_t length) {
	struct mVideoLogContext* context = channel->p;
	unsigned channelId = channel - context->channels;
	if (channelId >= mVL_MAX_CHANNELS) {
		return 0;
	}
	if (channelId != context->activeChannel) {
		_flushBuffer(context);
		context->activeChannel = channelId;
	}
	struct CircleBuffer* buffer = &channel->buffer;
	if (channel->injecting) {
		buffer = &channel->injectedBuffer;
	}
	if (CircleBufferCapacity(buffer) - CircleBufferSize(buffer) < length) {
		_flushBuffer(context);
		if (CircleBufferCapacity(buffer) < length) {
			CircleBufferDeinit(buffer);
			CircleBufferInit(buffer, toPow2(length << 1));
		}
	}

	ssize_t read = CircleBufferWrite(buffer, data, length);
	if (CircleBufferCapacity(buffer) == CircleBufferSize(buffer)) {
		_flushBuffer(context);
	}
	return read;
}

static const struct mVLDescriptor* _mVideoLogDescriptor(struct VFile* vf) {
	if (!vf) {
		return NULL;
	}
	struct mVideoLogHeader header = { { 0 } };
	vf->seek(vf, 0, SEEK_SET);
	ssize_t read = vf->read(vf, &header, sizeof(header));
	if (read != sizeof(header)) {
		return NULL;
	}
	if (memcmp(header.magic, mVL_MAGIC, sizeof(header.magic)) != 0) {
		return NULL;
	}
	enum mPlatform platform;
	LOAD_32LE(platform, 0, &header.platform);

	const struct mVLDescriptor* descriptor;
	for (descriptor = &_descriptors[0]; descriptor->platform != mPLATFORM_NONE; ++descriptor) {
		if (platform == descriptor->platform) {
			break;
		}
	}
	if (descriptor->platform == mPLATFORM_NONE) {
		return NULL;
	}
	return descriptor;
}

enum mPlatform mVideoLogIsCompatible(struct VFile* vf) {
	const struct mVLDescriptor* descriptor = _mVideoLogDescriptor(vf);
	if (descriptor) {
		return descriptor->platform;
	}
	return mPLATFORM_NONE;
}

struct mCore* mVideoLogCoreFind(struct VFile* vf) {
	const struct mVLDescriptor* descriptor = _mVideoLogDescriptor(vf);
	if (!descriptor) {
		return NULL;
	}
	struct mCore* core = NULL;
	if (descriptor->open) {
		core = descriptor->open();
	}
	return core;
}
