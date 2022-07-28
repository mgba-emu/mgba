/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/blip_buf.h>
#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/gb/core.h>
#include <mgba/gba/core.h>

#include <mgba/feature/commandline.h>
#include <mgba-util/socket.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#ifdef __3DS__
#include <3ds.h>
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif
#ifdef GEKKO
#define asm __asm__
#include <fat.h>
#include <gccore.h>
#ifdef FIXED_ROM_BUFFER
uint32_t* romBuffer;
size_t romBufferSize;
#endif
#endif
#ifdef PSP2
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>

#define PERF_OPTIONS "DF:L:NPS:T"
#define PERF_USAGE \
	"Benchmark options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -T               Use threaded video rendering\n" \
	"  -P               CSV output, useful for parsing\n" \
	"  -S SEC           Run for SEC in-game seconds before exiting\n" \
	"  -L FILE          Load a savestate when starting the test\n" \
	"  -D               Act as a server"

struct PerfOpts {
	bool noVideo;
	bool threadedVideo;
	bool csv;
	unsigned duration;
	unsigned frames;
	char* savestate;
	bool server;
};

#ifdef __SWITCH__
TimeType __nx_time_type = TimeType_LocalSystemClock;
#endif

static void _mPerfRunloop(struct mCore* context, int* frames, bool quiet);
static void _mPerfShutdown(int signal);
static bool _parsePerfOpts(struct mSubParser* parser, int option, const char* arg);
static void _log(struct mLogger*, int, enum mLogLevel, const char*, va_list);
static bool _mPerfRunCore(const char* fname, const struct mArguments*, const struct PerfOpts*);
static bool _mPerfRunServer(const struct mArguments*, const struct PerfOpts*);

static bool _dispatchExiting = false;
static struct VFile* _savestate = 0;
static void* _outputBuffer = NULL;
static Socket _socket = INVALID_SOCKET;
static Socket _server = INVALID_SOCKET;

int main(int argc, char** argv) {
#ifdef __3DS__
	UNUSED(_mPerfShutdown);
	gfxInitDefault();
	osSetSpeedupEnable(true);
	consoleInit(GFX_BOTTOM, NULL);
#elif defined(__SWITCH__)
	UNUSED(_mPerfShutdown);
	consoleInit(NULL);
#elif defined(GEKKO)
	VIDEO_Init();
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	GXRModeObj* vmode = VIDEO_GetPreferredMode(0);
	void* xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	console_init(xfb, 20, 20, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
	fatInitDefault();

#ifdef FIXED_ROM_BUFFER
	romBufferSize = 0x02000000;
	romBuffer = SYS_GetArena2Lo();
	SYS_SetArena2Lo((void*)((intptr_t) romBuffer + romBufferSize));
#endif
#elif defined(PSP2)
	scePowerSetArmClockFrequency(444);
#else
	signal(SIGINT, _mPerfShutdown);
#endif
	int didFail = 0;

	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

	struct PerfOpts perfOpts = { false, false, false, 0, 0, 0, false };
	struct mSubParser subparser = {
		.usage = PERF_USAGE,
		.parse = _parsePerfOpts,
		.extraOptions = PERF_OPTIONS,
		.opts = &perfOpts
	};

	struct mArguments args = {};
	bool parsed = mArgumentsParse(&args, argc, argv, &subparser, 1);
	if (!args.fname && !perfOpts.server) {
		parsed = false;
	}
	if (!parsed || args.showHelp) {
		usage(argv[0], NULL, NULL, &subparser, 1);
		didFail = !parsed;
		goto cleanup;
	}

	if (args.showVersion) {
		version(argv[0]);
		goto cleanup;
	}

	if (perfOpts.savestate) {
		_savestate = VFileOpen(perfOpts.savestate, O_RDONLY);
		free(perfOpts.savestate);
	}

	_outputBuffer = malloc(256 * 256 * 4);
	if (perfOpts.csv) {
		puts("game_code,frames,duration,renderer");
#ifdef __SWITCH__
		consoleUpdate(NULL);
#elif defined(GEKKO)
		VIDEO_WaitVSync();
#endif
	}
	if (perfOpts.server) {
		didFail = !_mPerfRunServer(&args, &perfOpts);
	} else {
		didFail = !_mPerfRunCore(args.fname, &args, &perfOpts);
	}
	free(_outputBuffer);

	if (_savestate) {
		_savestate->close(_savestate);
	}
	cleanup:
	mArgumentsDeinit(&args);

#ifdef __3DS__
	gfxExit();
	acExit();
#elif defined(__SWITCH__)
	consoleExit(NULL);
#elif defined(GEKKO)
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
#elif defined(PSP2)
	sceKernelExitProcess(0);
#endif

	return didFail;
}

bool _mPerfRunCore(const char* fname, const struct mArguments* args, const struct PerfOpts* perfOpts) {
	struct mCore* core = mCoreFind(fname);
	if (!core) {
		return false;
	}

	// TODO: Put back debugger
	char gameCode[9] = { 0 };

	core->init(core);
	if (!perfOpts->noVideo) {
		core->setVideoBuffer(core, _outputBuffer, 256);
	}
	mCoreLoadFile(core, fname);
	mCoreConfigInit(&core->config, "perf");
	mCoreConfigLoad(&core->config);

	if (perfOpts->threadedVideo) {
		mCoreConfigSetOverrideIntValue(&core->config, "threadedVideo", 1);
	} else {
		mCoreConfigSetOverrideIntValue(&core->config, "threadedVideo", 0);
	}

	struct mCoreOptions opts = {};
	mCoreConfigMap(&core->config, &opts);
	opts.audioSync = false;
	opts.videoSync = false;
	mArgumentsApply(args, NULL, 0, &core->config);
	mCoreConfigLoadDefaults(&core->config, &opts);
	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");
	mCoreLoadConfig(core);

	core->reset(core);
	if (_savestate) {
		mCoreLoadStateNamed(core, _savestate, 0);
	}

	core->getGameCode(core, gameCode);

	int frames = perfOpts->frames;
	if (!frames) {
		frames = perfOpts->duration * 60;
	}
	struct timeval tv;
	gettimeofday(&tv, 0);
	uint64_t start = 1000000LL * tv.tv_sec + tv.tv_usec;
	_mPerfRunloop(core, &frames, perfOpts->csv);
	gettimeofday(&tv, 0);
	uint64_t end = 1000000LL * tv.tv_sec + tv.tv_usec;
	uint64_t duration = end - start;

	mCoreConfigFreeOpts(&opts);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	float scaledFrames = frames * 1000000.f;
	if (perfOpts->csv) {
		char buffer[256];
		const char* rendererName;
		if (perfOpts->noVideo) {
			rendererName = "none";
		} else if (perfOpts->threadedVideo) {
			rendererName = "threaded-software";
		} else {
			rendererName = "software";
		}
		snprintf(buffer, sizeof(buffer), "%s,%i,%" PRIu64 ",%s\n", gameCode, frames, duration, rendererName);
		printf("%s", buffer);
		if (_socket != INVALID_SOCKET) {
			SocketSend(_socket, buffer, strlen(buffer));
		}
	} else {
		printf("%u frames in %" PRIu64 " microseconds: %g fps (%gx)\n", frames, duration, scaledFrames / duration, scaledFrames / (duration * 60.f));
	}
#ifdef __SWITCH__
	consoleUpdate(NULL);
#endif

	return true;
}

static void _mPerfRunloop(struct mCore* core, int* frames, bool quiet) {
	struct timeval lastEcho;
	gettimeofday(&lastEcho, 0);
	int duration = *frames;
	*frames = 0;
	int lastFrames = 0;
	while (!_dispatchExiting) {
		core->runFrame(core);
		++*frames;
		++lastFrames;
		if (!quiet) {
			struct timeval currentTime;
			long timeDiff;
			gettimeofday(&currentTime, 0);
			timeDiff = currentTime.tv_sec - lastEcho.tv_sec;
			timeDiff *= 1000;
			timeDiff += (currentTime.tv_usec - lastEcho.tv_usec) / 1000;
			if (timeDiff >= 1000) {
				printf("\033[2K\rCurrent FPS: %g (%gx)", lastFrames / (timeDiff / 1000.0f), lastFrames / (float) (60 * (timeDiff / 1000.0f)));
				fflush(stdout);
#ifdef __SWITCH__
				consoleUpdate(NULL);
#endif
				lastEcho = currentTime;
				lastFrames = 0;
			}
		}
		if (duration > 0 && *frames == duration) {
			break;
		}
	}
	if (!quiet) {
		printf("\033[2K\r");
	}
}

static bool _mPerfRunServer(const struct mArguments* args, const struct PerfOpts* perfOpts) {
	SocketSubsystemInit();
	_server = SocketOpenTCP(7216, NULL);
	if (SOCKET_FAILED(_server)) {
		SocketSubsystemDeinit();
		return false;
	}
	if (SOCKET_FAILED(SocketListen(_server, 0))) {
		SocketClose(_server);
		SocketSubsystemDeinit();
		return false;
	}
	_socket = SocketAccept(_server, NULL);
	if (SOCKET_FAILED(_socket)) {
		SocketClose(_server);
		SocketSubsystemDeinit();
		return false;
	}
	if (perfOpts->csv) {
		const char* header = "game_code,frames,duration,renderer\n";
		SocketSend(_socket, header, strlen(header));
	}
	char path[PATH_MAX];
	memset(path, 0, sizeof(path));
	ssize_t i;
	while ((i = SocketRecv(_socket, path, sizeof(path) - 1)) > 0) {
		path[i] = '\0';
		char* nl = strchr(path, '\n');
		if (nl == path) {
			break;
		}
		if (nl) {
			nl[0] = '\0';
		}
		if (!_mPerfRunCore(path, args, perfOpts)) {
			break;
		}
		memset(path, 0, sizeof(path));
	}
	SocketClose(_socket);
	SocketClose(_server);
	SocketSubsystemDeinit();
	return true;
}

static void _mPerfShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
	SocketClose(_socket);
	SocketClose(_server);
}

static bool _parsePerfOpts(struct mSubParser* parser, int option, const char* arg) {
	struct PerfOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'D':
		opts->server = true;
		return true;
	case 'F':
		opts->frames = strtoul(arg, 0, 10);
		return !errno;
	case 'N':
		opts->noVideo = true;
		return true;
	case 'P':
		opts->csv = true;
		return true;
	case 'S':
		opts->duration = strtoul(arg, 0, 10);
		return !errno;
	case 'T':
		opts->threadedVideo = true;
		return true;
	case 'L':
		opts->savestate = strdup(arg);
		return true;
	default:
		return false;
	}
}

static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(log);
	UNUSED(category);
	UNUSED(level);
	UNUSED(format);
	UNUSED(args);
}
