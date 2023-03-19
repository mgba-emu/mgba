/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_H
#define M_CORE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/config.h>
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
#include <mgba/core/directories.h>
#endif
#ifndef MINIMAL_CORE
#include <mgba/core/input.h>
#endif
#include <mgba/core/interface.h>
#ifdef USE_DEBUGGERS
#include <mgba/debugger/debugger.h>
#endif

enum mPlatform {
	mPLATFORM_NONE = -1,
	mPLATFORM_GBA = 0,
	mPLATFORM_GB = 1,
};

enum mCoreChecksumType {
	mCHECKSUM_CRC32,
};

struct mCoreConfig;
struct mCoreSync;
struct mDebuggerSymbols;
struct mStateExtdata;
struct mVideoLogContext;
struct mCore {
	void* cpu;
	void* board;
	struct mTiming* timing;
	struct mDebugger* debugger;
	struct mDebuggerSymbols* symbolTable;
	struct mVideoLogger* videoLogger;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct mDirectorySet dirs;
#endif
#ifndef MINIMAL_CORE
	struct mInputMap inputMap;
#endif
	struct mCoreConfig config;
	struct mCoreOptions opts;

	struct mRTCGenericSource rtc;

	bool (*init)(struct mCore*);
	void (*deinit)(struct mCore*);

	enum mPlatform (*platform)(const struct mCore*);
	bool (*supportsFeature)(const struct mCore*, enum mCoreFeature);

	void (*setSync)(struct mCore*, struct mCoreSync*);
	void (*loadConfig)(struct mCore*, const struct mCoreConfig*);
	void (*reloadConfigOption)(struct mCore*, const char* option, const struct mCoreConfig*);

	void (*baseVideoSize)(const struct mCore*, unsigned* width, unsigned* height);
	void (*currentVideoSize)(const struct mCore*, unsigned* width, unsigned* height);
	unsigned (*videoScale)(const struct mCore*);
	size_t (*screenRegions)(const struct mCore*, const struct mCoreScreenRegion**);

	void (*setVideoBuffer)(struct mCore*, color_t* buffer, size_t stride);
	void (*setVideoGLTex)(struct mCore*, unsigned texid);

	void (*getPixels)(struct mCore*, const void** buffer, size_t* stride);
	void (*putPixels)(struct mCore*, const void* buffer, size_t stride);

	struct blip_t* (*getAudioChannel)(struct mCore*, int ch);
	void (*setAudioBufferSize)(struct mCore*, size_t samples);
	size_t (*getAudioBufferSize)(struct mCore*);

	void (*addCoreCallbacks)(struct mCore*, struct mCoreCallbacks*);
	void (*clearCoreCallbacks)(struct mCore*);
	void (*setAVStream)(struct mCore*, struct mAVStream*);

	bool (*isROM)(struct VFile* vf);
	bool (*loadROM)(struct mCore*, struct VFile* vf);
	bool (*loadSave)(struct mCore*, struct VFile* vf);
	bool (*loadTemporarySave)(struct mCore*, struct VFile* vf);
	void (*unloadROM)(struct mCore*);
	size_t (*romSize)(const struct mCore*);
	void (*checksum)(const struct mCore*, void* data, enum mCoreChecksumType type);

	bool (*loadBIOS)(struct mCore*, struct VFile* vf, int biosID);
	bool (*selectBIOS)(struct mCore*, int biosID);

	bool (*loadPatch)(struct mCore*, struct VFile* vf);

	void (*reset)(struct mCore*);
	void (*runFrame)(struct mCore*);
	void (*runLoop)(struct mCore*);
	void (*step)(struct mCore*);

	size_t (*stateSize)(struct mCore*);
	bool (*loadState)(struct mCore*, const void* state);
	bool (*saveState)(struct mCore*, void* state);

	void (*setKeys)(struct mCore*, uint32_t keys);
	void (*addKeys)(struct mCore*, uint32_t keys);
	void (*clearKeys)(struct mCore*, uint32_t keys);
	uint32_t (*getKeys)(struct mCore*);

	uint32_t (*frameCounter)(const struct mCore*);
	int32_t (*frameCycles)(const struct mCore*);
	int32_t (*frequency)(const struct mCore*);

	void (*getGameTitle)(const struct mCore*, char* title);
	void (*getGameCode)(const struct mCore*, char* title);

	void (*setPeripheral)(struct mCore*, int type, void*);

	uint32_t (*busRead8)(struct mCore*, uint32_t address);
	uint32_t (*busRead16)(struct mCore*, uint32_t address);
	uint32_t (*busRead32)(struct mCore*, uint32_t address);

	void (*busWrite8)(struct mCore*, uint32_t address, uint8_t);
	void (*busWrite16)(struct mCore*, uint32_t address, uint16_t);
	void (*busWrite32)(struct mCore*, uint32_t address, uint32_t);

	uint32_t (*rawRead8)(struct mCore*, uint32_t address, int segment);
	uint32_t (*rawRead16)(struct mCore*, uint32_t address, int segment);
	uint32_t (*rawRead32)(struct mCore*, uint32_t address, int segment);

	void (*rawWrite8)(struct mCore*, uint32_t address, int segment, uint8_t);
	void (*rawWrite16)(struct mCore*, uint32_t address, int segment, uint16_t);
	void (*rawWrite32)(struct mCore*, uint32_t address, int segment, uint32_t);

	size_t (*listMemoryBlocks)(const struct mCore*, const struct mCoreMemoryBlock**);
	void* (*getMemoryBlock)(struct mCore*, size_t id, size_t* sizeOut);

	size_t (*listRegisters)(const struct mCore*, const struct mCoreRegisterInfo**);
	bool (*readRegister)(const struct mCore*, const char* name, void* out);
	bool (*writeRegister)(struct mCore*, const char* name, const void* in);

#ifdef USE_DEBUGGERS
	bool (*supportsDebuggerType)(struct mCore*, enum mDebuggerType);
	struct mDebuggerPlatform* (*debuggerPlatform)(struct mCore*);
	struct CLIDebuggerSystem* (*cliDebuggerSystem)(struct mCore*);
	void (*attachDebugger)(struct mCore*, struct mDebugger*);
	void (*detachDebugger)(struct mCore*);

	void (*loadSymbols)(struct mCore*, struct VFile*);
	bool (*lookupIdentifier)(struct mCore*, const char* name, int32_t* value, int* segment);
#endif

	struct mCheatDevice* (*cheatDevice)(struct mCore*);

	size_t (*savedataClone)(struct mCore*, void** sram);
	bool (*savedataRestore)(struct mCore*, const void* sram, size_t size, bool writeback);

	size_t (*listVideoLayers)(const struct mCore*, const struct mCoreChannelInfo**);
	size_t (*listAudioChannels)(const struct mCore*, const struct mCoreChannelInfo**);
	void (*enableVideoLayer)(struct mCore*, size_t id, bool enable);
	void (*enableAudioChannel)(struct mCore*, size_t id, bool enable);
	void (*adjustVideoLayer)(struct mCore*, size_t id, int32_t x, int32_t y);

#ifndef MINIMAL_CORE
	void (*startVideoLog)(struct mCore*, struct mVideoLogContext*);
	void (*endVideoLog)(struct mCore*);
#endif
};

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
struct mCore* mCoreFind(const char* path);
bool mCoreLoadFile(struct mCore* core, const char* path);

bool mCorePreloadVF(struct mCore* core, struct VFile* vf);
bool mCorePreloadFile(struct mCore* core, const char* path);

bool mCorePreloadVFCB(struct mCore* core, struct VFile* vf, void (cb)(size_t, size_t, void*), void* context);
bool mCorePreloadFileCB(struct mCore* core, const char* path, void (cb)(size_t, size_t, void*), void* context);

bool mCoreAutoloadSave(struct mCore* core);
bool mCoreAutoloadPatch(struct mCore* core);
bool mCoreAutoloadCheats(struct mCore* core);

bool mCoreLoadSaveFile(struct mCore* core, const char* path, bool temporary);

bool mCoreSaveState(struct mCore* core, int slot, int flags);
bool mCoreLoadState(struct mCore* core, int slot, int flags);
struct VFile* mCoreGetState(struct mCore* core, int slot, bool write);
void mCoreDeleteState(struct mCore* core, int slot);

void mCoreTakeScreenshot(struct mCore* core);
bool mCoreTakeScreenshotVF(struct mCore* core, struct VFile* vf);
#endif

struct mCore* mCoreFindVF(struct VFile* vf);
enum mPlatform mCoreIsCompatible(struct VFile* vf);
struct mCore* mCoreCreate(enum mPlatform);

bool mCoreSaveStateNamed(struct mCore* core, struct VFile* vf, int flags);
bool mCoreLoadStateNamed(struct mCore* core, struct VFile* vf, int flags);

void mCoreInitConfig(struct mCore* core, const char* port);
void mCoreLoadConfig(struct mCore* core);
void mCoreLoadForeignConfig(struct mCore* core, const struct mCoreConfig* config);

void mCoreSetRTC(struct mCore* core, struct mRTCSource* rtc);

void* mCoreGetMemoryBlock(struct mCore* core, uint32_t start, size_t* size);
void* mCoreGetMemoryBlockMasked(struct mCore* core, uint32_t start, size_t* size, uint32_t mask);
const struct mCoreMemoryBlock* mCoreGetMemoryBlockInfo(struct mCore* core, uint32_t address);

#ifdef USE_ELF
struct ELF;
bool mCoreLoadELF(struct mCore* core, struct ELF* elf);
#ifdef USE_DEBUGGERS
void mCoreLoadELFSymbols(struct mDebuggerSymbols* symbols, struct ELF*);
#endif
#endif

CXX_GUARD_END

#endif
