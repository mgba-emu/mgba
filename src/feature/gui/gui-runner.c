/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-runner.h"

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include "feature/gui/gui-config.h"
#include "feature/gui/cheats.h"
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/gba/interface.h>
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/memory.h>
#include <mgba-util/png-io.h>
#include <mgba-util/vfs.h>

#include <sys/time.h>

mLOG_DECLARE_CATEGORY(GUI_RUNNER);
mLOG_DEFINE_CATEGORY(GUI_RUNNER, "GUI Runner", "gui.runner");

#define AUTOSAVE_GRANULARITY 600
#define FPS_GRANULARITY 120
#define FPS_BUFFER_SIZE 3

enum {
	RUNNER_CONTINUE = 1,
	RUNNER_EXIT,
	RUNNER_SAVE_STATE,
	RUNNER_LOAD_STATE,
	RUNNER_SCREENSHOT,
	RUNNER_CONFIG,
	RUNNER_RESET,
	RUNNER_CHEATS,
	RUNNER_COMMAND_MASK = 0xFFFF
};

#define RUNNER_STATE(X) ((X) << 16)

static const struct mInputPlatformInfo _mGUIKeyInfo = {
	.platformName = "gui",
	.keyId = (const char*[GUI_INPUT_MAX]) {
		"Select",
		"Back",
		"Cancel",
		"Up",
		"Down",
		"Left",
		"Right",
		[mGUI_INPUT_INCREASE_BRIGHTNESS] = "Increase solar brightness",
		[mGUI_INPUT_DECREASE_BRIGHTNESS] = "Decrease solar brightness",
		[mGUI_INPUT_SCREEN_MODE] = "Screen mode",
		[mGUI_INPUT_SCREENSHOT] = "Take screenshot",
		[mGUI_INPUT_FAST_FORWARD_HELD] = "Fast forward (held)",
		[mGUI_INPUT_FAST_FORWARD_TOGGLE] = "Fast forward (toggle)",
		[mGUI_INPUT_MUTE_TOGGLE] = "Mute (toggle)",
	},
	.nKeys = GUI_INPUT_MAX
};

static void _log(struct mLogger*, int category, enum mLogLevel level, const char* format, va_list args);

static struct mGUILogger {
	struct mLogger d;
	struct VFile* vf;
	int logLevel;
} logger = {
	.d = {
		.log = _log
	},
	.vf = NULL,
	.logLevel = 0
};

static bool _testExtensions(const char* name) {
	char ext[PATH_MAX] = {};
	separatePath(name, NULL, NULL, ext);

	if (!strncmp(ext, "sav", PATH_MAX)) {
		return false;
	}
	if (!strncmp(ext, "png", PATH_MAX)) {
		return false;
	}
	if (!strncmp(ext, "ini", PATH_MAX)) {
		return false;
	}
	if (!strncmp(ext, "ss", 2)) {
		return false;
	}

	return true;
}

static void _drawBackground(struct GUIBackground* background, void* context) {
	UNUSED(context);
	struct mGUIBackground* gbaBackground = (struct mGUIBackground*) background;
	if (gbaBackground->p->drawFrame) {
		gbaBackground->p->drawFrame(gbaBackground->p, true);
	}
}

static void _drawState(struct GUIBackground* background, void* id) {
	struct mGUIBackground* gbaBackground = (struct mGUIBackground*) background;
	int stateId = ((int) id) >> 16;
	if (gbaBackground->p->drawScreenshot) {
		unsigned w, h;
		gbaBackground->p->core->desiredVideoDimensions(gbaBackground->p->core, &w, &h);
		if (gbaBackground->screenshot && gbaBackground->screenshotId == (int) id) {
			gbaBackground->p->drawScreenshot(gbaBackground->p, gbaBackground->screenshot, w, h, true);
			return;
		}
		struct VFile* vf = mCoreGetState(gbaBackground->p->core, stateId, false);
		color_t* pixels = gbaBackground->screenshot;
		if (!pixels) {
			pixels = anonymousMemoryMap(w * h * 4);
			gbaBackground->screenshot = pixels;
		}
		bool success = false;
		if (vf && isPNG(vf) && pixels) {
			png_structp png = PNGReadOpen(vf, PNG_HEADER_BYTES);
			png_infop info = png_create_info_struct(png);
			png_infop end = png_create_info_struct(png);
			if (png && info && end) {
				success = PNGReadHeader(png, info);
				success = success && PNGReadPixels(png, info, pixels, w, h, w);
				success = success && PNGReadFooter(png, end);
			}
			PNGReadClose(png, info, end);
		}
		if (vf) {
			vf->close(vf);
		}
		if (success) {
			gbaBackground->p->drawScreenshot(gbaBackground->p, pixels, w, h, true);
			gbaBackground->screenshotId = (int) id;
		} else if (gbaBackground->p->drawFrame) {
			gbaBackground->p->drawFrame(gbaBackground->p, true);
		}
	}
}

static void _updateLux(struct GBALuminanceSource* lux) {
	UNUSED(lux);
}

static uint8_t _readLux(struct GBALuminanceSource* lux) {
	struct mGUIRunnerLux* runnerLux = (struct mGUIRunnerLux*) lux;
	int value = 0x16;
	if (runnerLux->luxLevel > 0) {
		value += GBA_LUX_LEVELS[runnerLux->luxLevel - 1];
	}
	return 0xFF - value;
}

static void _tryAutosave(struct mGUIRunner* runner) {
	int autosave = false;
	mCoreConfigGetIntValue(&runner->config, "autosave", &autosave);
	if (!autosave) {
		return;
	}

#ifdef DISABLE_THREADING
	mCoreSaveState(runner->core, 0, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA);
#else
	if (!runner->autosave.buffer) {
		runner->autosave.buffer = VFileMemChunk(NULL, 0);
	}
	MutexLock(&runner->autosave.mutex);
	runner->autosave.core = runner->core;
	mCoreSaveStateNamed(runner->core, runner->autosave.buffer, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA);
	ConditionWake(&runner->autosave.cond);
	MutexUnlock(&runner->autosave.mutex);
#endif
}

void mGUIInit(struct mGUIRunner* runner, const char* port) {
	GUIInit(&runner->params);
	runner->port = port;
	runner->core = NULL;
	runner->luminanceSource.d.readLuminance = _readLux;
	runner->luminanceSource.d.sample = _updateLux;
	runner->luminanceSource.luxLevel = 0;
	runner->background.d.draw = _drawBackground;
	runner->background.p = runner;
	runner->fps = 0;
	runner->lastFpsCheck = 0;
	runner->totalDelta = 0;
	CircleBufferInit(&runner->fpsBuffer, FPS_BUFFER_SIZE * sizeof(uint32_t));

	mInputMapInit(&runner->params.keyMap, &_mGUIKeyInfo);
	mCoreConfigInit(&runner->config, runner->port);
	// TODO: Do we need to load more defaults?
	mCoreConfigSetDefaultIntValue(&runner->config, "volume", 0x100);
	mCoreConfigSetDefaultValue(&runner->config, "idleOptimization", "detect");
	mCoreConfigSetDefaultIntValue(&runner->config, "autoload", true);
#ifdef DISABLE_THREADING
	mCoreConfigSetDefaultIntValue(&runner->config, "autosave", false);
#else
	mCoreConfigSetDefaultIntValue(&runner->config, "autosave", true);
#endif
	mCoreConfigSetDefaultIntValue(&runner->config, "showOSD", true);
	mCoreConfigLoad(&runner->config);
	mCoreConfigGetIntValue(&runner->config, "logLevel", &logger.logLevel);

	char path[PATH_MAX];
	mCoreConfigDirectory(path, PATH_MAX);
	strncat(path, PATH_SEP "log", PATH_MAX - strlen(path));
	logger.vf = VFileOpen(path, O_CREAT | O_WRONLY | O_APPEND);
	mLogSetDefaultLogger(&logger.d);

	const char* lastPath = mCoreConfigGetValue(&runner->config, "lastDirectory");
	if (lastPath) {
		struct VDir* dir = VDirOpen(lastPath);
		if (dir) {
			dir->close(dir);
			strncpy(runner->params.currentPath, lastPath, PATH_MAX - 1);
			runner->params.currentPath[PATH_MAX - 1] = '\0';
		}
	}

#ifndef DISABLE_THREADING
	if (!runner->autosave.running) {
		runner->autosave.running = true;
		runner->autosave.core = NULL;
		MutexInit(&runner->autosave.mutex);
		ConditionInit(&runner->autosave.cond);
		ThreadCreate(&runner->autosave.thread, mGUIAutosaveThread, &runner->autosave);
	}
#endif
}

void mGUIDeinit(struct mGUIRunner* runner) {
#ifndef DISABLE_THREADING
	MutexLock(&runner->autosave.mutex);
	runner->autosave.running = false;
	ConditionWake(&runner->autosave.cond);
	MutexUnlock(&runner->autosave.mutex);

	ThreadJoin(&runner->autosave.thread);

	ConditionDeinit(&runner->autosave.cond);
	MutexDeinit(&runner->autosave.mutex);

	if (runner->autosave.buffer) {
		runner->autosave.buffer->close(runner->autosave.buffer);
	}
#endif

	if (runner->teardown) {
		runner->teardown(runner);
	}
	CircleBufferDeinit(&runner->fpsBuffer);
	mInputMapDeinit(&runner->params.keyMap);
	mCoreConfigDeinit(&runner->config);
	if (logger.vf) {
		logger.vf->close(logger.vf);
		logger.vf = NULL;
	}
}

static void _log(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	struct mGUILogger* guiLogger = (struct mGUILogger*) logger;
	if (!guiLogger->vf) {
		return;
	}
	if (!(guiLogger->logLevel & level)) {
		return;
	}

	char log[256] = {0};
	vsnprintf(log, sizeof(log) - 1, format, args);
	char log2[256] = {0};
	size_t len = snprintf(log2, sizeof(log2) - 1, "%s: %s\n", mLogCategoryName(category), log);
	if (len >= sizeof(log2)) {
		len = sizeof(log2) - 1;
	}
	if (guiLogger->vf->write(guiLogger->vf, log2, len) < 0) {
		char path[PATH_MAX];
		mCoreConfigDirectory(path, PATH_MAX);
		strncat(path, PATH_SEP "log", PATH_MAX - strlen(path));
		guiLogger->vf->close(guiLogger->vf);
		guiLogger->vf = VFileOpen(path, O_CREAT | O_WRONLY | O_APPEND);
		if (guiLogger->vf->write(guiLogger->vf, log2, len) < 0) {
			guiLogger->vf->close(guiLogger->vf);
			guiLogger->vf = NULL;
		}
	}
#ifdef GEKKO
	puts(log2);
#endif
}

static void _updateLoading(size_t read, size_t size, void* context) {
	struct mGUIRunner* runner = context;
	runner->params.drawStart();
	if (runner->params.guiPrepare) {
		runner->params.guiPrepare();
	}
	GUIFontPrintf(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_ALIGN_HCENTER, 0xFFFFFFFF, "Loading...%i%%", 100 * read / size);
	if (runner->params.guiFinish) {
		runner->params.guiFinish();
	}
	runner->params.drawEnd();
}

void mGUIRun(struct mGUIRunner* runner, const char* path) {
	struct mGUIBackground drawState = {
		.d = {
			.draw = _drawState
		},
		.p = runner,
		.screenshot = 0,
		.screenshotId = 0
	};
	struct GUIMenu pauseMenu = {
		.title = "Game Paused",
		.index = 0,
		.background = &runner->background.d
	};
	struct GUIMenu stateSaveMenu = {
		.title = "Save state",
		.index = 0,
		.background = &drawState.d
	};
	struct GUIMenu stateLoadMenu = {
		.title = "Load state",
		.index = 0,
		.background = &drawState.d
	};
	GUIMenuItemListInit(&pauseMenu.items, 0);
	GUIMenuItemListInit(&stateSaveMenu.items, 9);
	GUIMenuItemListInit(&stateLoadMenu.items, 10);
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Unpause", .data = (void*) RUNNER_CONTINUE };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Save state", .submenu = &stateSaveMenu };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Load state", .submenu = &stateLoadMenu };

	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(1)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(2)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(3)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(4)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(5)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(6)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(7)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(8)) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE(9)) };

	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "Autosave", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(0)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(1)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(2)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(3)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(4)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(5)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(6)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(7)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(8)) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE(9)) };

	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Take screenshot", .data = (void*) RUNNER_SCREENSHOT };
	if (runner->params.getText) {
		*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Cheats", .data = (void*) RUNNER_CHEATS };
	}
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Configure", .data = (void*) RUNNER_CONFIG };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Reset game", .data = (void*) RUNNER_RESET };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Exit game", .data = (void*) RUNNER_EXIT };

	runner->params.drawStart();
	if (runner->params.guiPrepare) {
		runner->params.guiPrepare();
	}
	GUIFontPrint(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_ALIGN_HCENTER, 0xFFFFFFFF, "Loading...");
	if (runner->params.guiFinish) {
		runner->params.guiFinish();
	}
	runner->params.drawEnd();

	bool found = false;
	mLOG(GUI_RUNNER, INFO, "Attempting to load %s", path);
	runner->core = mCoreFind(path);
	if (runner->core) {
		mLOG(GUI_RUNNER, INFO, "Found core");
		runner->core->init(runner->core);
		mCoreInitConfig(runner->core, runner->port);
		mInputMapInit(&runner->core->inputMap, &GBAInputInfo);

		struct VFile* rom = mDirectorySetOpenPath(&runner->core->dirs, path, runner->core->isROM);
		if (runner->setFrameLimiter) {
			runner->setFrameLimiter(runner, false);
		}
		found = mCorePreloadVFCB(runner->core, rom, _updateLoading, runner);
		if (runner->setFrameLimiter) {
			runner->setFrameLimiter(runner, true);
		}

#ifdef FIXED_ROM_BUFFER
		extern size_t romBufferSize;
		if (!found && rom && (size_t) rom->size(rom) > romBufferSize) {
			found = runner->core->loadROM(runner->core, rom);
		}
#endif
		if (!found) {
			if (rom) {
				rom->close(rom);
			}
			mLOG(GUI_RUNNER, WARN, "Failed to load %s!", path);
			mCoreConfigDeinit(&runner->core->config);
			runner->core->deinit(runner->core);
		}
	}

	if (!found) {
		mLOG(GUI_RUNNER, WARN, "Failed to find core for %s!", path);
		GUIShowMessageBox(&runner->params, GUI_MESSAGE_BOX_OK, 240, "Load failed!");
		return;
	}
	if (runner->core->platform(runner->core) == mPLATFORM_GBA) {
		runner->core->setPeripheral(runner->core, mPERIPH_GBA_LUMINANCE, &runner->luminanceSource.d);
	}
	mLOG(GUI_RUNNER, DEBUG, "Loading config...");
	mCoreLoadForeignConfig(runner->core, &runner->config);

	mLOG(GUI_RUNNER, DEBUG, "Loading save...");
	mCoreAutoloadSave(runner->core);
	mCoreAutoloadCheats(runner->core);
	if (runner->setup) {
		mLOG(GUI_RUNNER, DEBUG, "Setting up runner...");
		runner->setup(runner);
	}
	if (runner->config.port && runner->keySources) {
		mLOG(GUI_RUNNER, DEBUG, "Loading key sources for %s...", runner->config.port);
		size_t i;
		for (i = 0; runner->keySources[i].id; ++i) {
			mInputMapLoad(&runner->core->inputMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
		}
	}
	mLOG(GUI_RUNNER, DEBUG, "Reseting...");
	runner->core->reset(runner->core);
	mLOG(GUI_RUNNER, DEBUG, "Reset!");


	int autoload = false;
	mCoreConfigGetIntValue(&runner->config, "autoload", &autoload);
	if (autoload) {
		mCoreLoadState(runner->core, 0, SAVESTATE_SCREENSHOT | SAVESTATE_RTC);
	}

	int showOSD = true;
	mCoreConfigGetIntValue(&runner->config, "showOSD", &showOSD);

	int drawFps = false;
	mCoreConfigGetIntValue(&runner->config, "fpsCounter", &drawFps);

	int mute = false;
	mCoreConfigGetIntValue(&runner->config, "mute", &mute);

	int fastForwardMute = false;
	mCoreConfigGetIntValue(&runner->config, "fastForwardMute", &fastForwardMute);

	bool running = true;

#ifndef DISABLE_THREADING
	MutexLock(&runner->autosave.mutex);
	runner->autosave.core = runner->core;
	MutexUnlock(&runner->autosave.mutex);
#endif

	if (runner->gameLoaded) {
		runner->gameLoaded(runner);
	}
	mLOG(GUI_RUNNER, INFO, "Game starting");
	while (running) {
		CircleBufferClear(&runner->fpsBuffer);
		runner->totalDelta = 0;
		runner->fps = 0;
		struct timeval tv;
		gettimeofday(&tv, 0);
		runner->lastFpsCheck = 1000000LL * tv.tv_sec + tv.tv_usec;

		int frame = 0;
		bool fastForward = false;
		while (running) {
			if (runner->running) {
				running = runner->running(runner);
				if (!running) {
					break;
				}
			}
			uint32_t guiKeys;
			uint32_t heldKeys;
			GUIPollInput(&runner->params, &guiKeys, &heldKeys);
			if (guiKeys & (1 << GUI_INPUT_CANCEL)) {
				break;
			}
			if (guiKeys & (1 << mGUI_INPUT_INCREASE_BRIGHTNESS)) {
				if (runner->luminanceSource.luxLevel < 10) {
					++runner->luminanceSource.luxLevel;
				}
			}
			if (guiKeys & (1 << mGUI_INPUT_DECREASE_BRIGHTNESS)) {
				if (runner->luminanceSource.luxLevel > 0) {
					--runner->luminanceSource.luxLevel;
				}
			}
			if (guiKeys & (1 << mGUI_INPUT_SCREEN_MODE) && runner->incrementScreenMode) {
				runner->incrementScreenMode(runner);
			}
			if (guiKeys & (1 << mGUI_INPUT_SCREENSHOT)) {
				mCoreTakeScreenshot(runner->core);
			}
			bool muteTogglePressed = guiKeys & (1 << mGUI_INPUT_MUTE_TOGGLE);
			if (muteTogglePressed) {
				mute = !mute;
				mCoreConfigSetUIntValue(&runner->config, "mute", mute);
				runner->core->reloadConfigOption(runner->core, "mute", &runner->config);
			}
			if (runner->setFrameLimiter) {
				if (guiKeys & (1 << mGUI_INPUT_FAST_FORWARD_TOGGLE)) {
					fastForward = !fastForward;
				}
				bool fastForwarding = fastForward || (heldKeys & (1 << mGUI_INPUT_FAST_FORWARD_HELD));
				if (fastForwarding) {
					if (fastForwardMute && !mute && !muteTogglePressed) {
						mCoreConfigSetUIntValue(&runner->core->config, "mute", fastForwardMute);
						runner->core->reloadConfigOption(runner->core, "mute", NULL);
					}

					runner->setFrameLimiter(runner, false);
				} else {
					runner->setFrameLimiter(runner, true);

					if (fastForwardMute && !mute && !muteTogglePressed) {
						mCoreConfigSetUIntValue(&runner->core->config, "mute", !fastForwardMute);
						runner->core->reloadConfigOption(runner->core, "mute", NULL);
					}
				}
			}
			uint16_t keys = runner->pollGameInput(runner);
			if (runner->prepareForFrame) {
				runner->prepareForFrame(runner);
			}
			runner->core->setKeys(runner->core, keys);
			runner->core->runFrame(runner->core);
			if (runner->drawFrame) {
				runner->params.drawStart();
				runner->drawFrame(runner, false);
				if (showOSD || drawFps) {
					if (runner->params.guiPrepare) {
						runner->params.guiPrepare();
					}
					if (drawFps) {
						GUIFontPrintf(runner->params.font, 0, GUIFontHeight(runner->params.font), GUI_ALIGN_LEFT, 0x7FFFFFFF, "%.2f fps", runner->fps);
					}
					if (showOSD) {
						unsigned origin = runner->params.width - GUIFontHeight(runner->params.font) / 2;
						unsigned w;
						if (fastForward || (heldKeys & (1 << mGUI_INPUT_FAST_FORWARD_HELD))) {
							GUIFontDrawIcon(runner->params.font, origin, GUIFontHeight(runner->params.font) / 2, GUI_ALIGN_RIGHT, 0, 0x7FFFFFFF, GUI_ICON_STATUS_FAST_FORWARD);
							GUIFontIconMetrics(runner->params.font, GUI_ICON_STATUS_FAST_FORWARD, &w, NULL);
							origin -= w + GUIFontHeight(runner->params.font) / 2;
						}
						if (runner->core->opts.mute) {
							GUIFontDrawIcon(runner->params.font, origin, GUIFontHeight(runner->params.font) / 2, GUI_ALIGN_RIGHT, 0, 0x7FFFFFFF, GUI_ICON_STATUS_MUTE);
							GUIFontIconMetrics(runner->params.font, GUI_ICON_STATUS_MUTE, &w, NULL);
							origin -= w + GUIFontHeight(runner->params.font) / 2;
						}
					}
					if (runner->params.guiFinish) {
						runner->params.guiFinish();
					}
				}
				runner->params.drawEnd();

				if (runner->core->frameCounter(runner->core) % FPS_GRANULARITY == 0) {
					if (drawFps) {
						struct timeval tv;
						gettimeofday(&tv, 0);
						uint64_t t = 1000000LL * tv.tv_sec + tv.tv_usec;
						uint64_t delta = t - runner->lastFpsCheck;
						runner->lastFpsCheck = t;
						if (delta > 0x7FFFFFFFLL) {
							CircleBufferClear(&runner->fpsBuffer);
							runner->fps = 0;
						}
						if (CircleBufferSize(&runner->fpsBuffer) == CircleBufferCapacity(&runner->fpsBuffer)) {
							int32_t last;
							CircleBufferRead32(&runner->fpsBuffer, &last);
							runner->totalDelta -= last;
						}
						CircleBufferWrite32(&runner->fpsBuffer, delta);
						runner->totalDelta += delta;
						runner->fps = (CircleBufferSize(&runner->fpsBuffer) * FPS_GRANULARITY * 1000000.0f) / (runner->totalDelta * sizeof(uint32_t));
					}
				}
				if (frame == AUTOSAVE_GRANULARITY) {
					frame = 0;
					_tryAutosave(runner);
				}
				++frame;
			}
		}
		if (!running) {
			break;
		}

		if (runner->paused) {
			runner->paused(runner);
		}
		if (runner->setFrameLimiter) {
			runner->setFrameLimiter(runner, true);
		}

		GUIInvalidateKeys(&runner->params);
		uint32_t keys = 0xFFFFFFFF; // Huge hack to avoid an extra variable!
		struct GUIMenuItem* item;
		enum GUIMenuExitReason reason = GUIShowMenu(&runner->params, &pauseMenu, &item);
		if (reason == GUI_MENU_EXIT_ACCEPT) {
			switch (((int) item->data) & RUNNER_COMMAND_MASK) {
			case RUNNER_EXIT:
				running = false;
				keys = 0;
				break;
			case RUNNER_RESET:
				runner->core->reset(runner->core);
				break;
			case RUNNER_SAVE_STATE:
				mCoreSaveState(runner->core, ((int) item->data) >> 16, SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA);
				break;
			case RUNNER_LOAD_STATE:
				mCoreLoadState(runner->core, ((int) item->data) >> 16, SAVESTATE_SCREENSHOT | SAVESTATE_RTC);
				break;
			case RUNNER_SCREENSHOT:
				mCoreTakeScreenshot(runner->core);
				break;
			case RUNNER_CONFIG:
				mGUIShowConfig(runner, runner->configExtra, runner->nConfigExtra);
				break;
			case RUNNER_CHEATS:
				mGUIShowCheats(runner);
				break;
			case RUNNER_CONTINUE:
				break;
			}
		}
		int frames = 0;
		GUIPollInput(&runner->params, 0, &keys);
		while (keys && frames < 30) {
#ifdef _3DS
			if (!frames) {
#endif
				runner->params.drawStart();
				runner->drawFrame(runner, true);
				runner->params.drawEnd();
#ifdef _3DS
			} else {
				// XXX: Why does this fix #1294?
				usleep(15000);
			}
#endif
			++frames;
			GUIPollInput(&runner->params, 0, &keys);
		}
		if (runner->unpaused) {
			runner->unpaused(runner);
		}
		mCoreConfigGetIntValue(&runner->config, "fpsCounter", &drawFps);
		mCoreConfigGetIntValue(&runner->config, "showOSD", &showOSD);
		mCoreConfigGetIntValue(&runner->config, "mute", &mute);
		mCoreConfigGetIntValue(&runner->config, "fastForwardMute", &fastForwardMute);
#ifdef M_CORE_GB
		if (runner->core->platform(runner->core) == mPLATFORM_GB) {
			runner->core->reloadConfigOption(runner->core, "gb.pal", &runner->config);
		}
#endif
	}
	mLOG(GUI_RUNNER, DEBUG, "Shutting down...");
	if (runner->gameUnloaded) {
		runner->gameUnloaded(runner);
	}
#ifndef DISABLE_THREADING
	MutexLock(&runner->autosave.mutex);
	runner->autosave.core = NULL;
	MutexUnlock(&runner->autosave.mutex);
#endif

	int autosave = false;
	mCoreConfigGetIntValue(&runner->config, "autosave", &autosave);
	if (autosave) {
		mCoreSaveState(runner->core, 0, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA);
	}

	mLOG(GUI_RUNNER, DEBUG, "Unloading game...");
	runner->core->unloadROM(runner->core);
	drawState.screenshotId = 0;
	if (drawState.screenshot) {
		unsigned w, h;
		runner->core->desiredVideoDimensions(runner->core, &w, &h);
		mappedMemoryFree(drawState.screenshot, w * h * 4);
	}

	if (runner->config.port) {
		mLOG(GUI_RUNNER, DEBUG, "Saving key sources...");
		if (runner->keySources) {
			size_t i;
			for (i = 0; runner->keySources[i].id; ++i) {
				mInputMapSave(&runner->core->inputMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
				mInputMapSave(&runner->params.keyMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
			}
		}
		mCoreConfigSave(&runner->config);
	}
	mInputMapDeinit(&runner->core->inputMap);
	mLOG(GUI_RUNNER, DEBUG, "Deinitializing core...");
	mCoreConfigDeinit(&runner->core->config);
	runner->core->deinit(runner->core);
	runner->core = NULL;

	GUIMenuItemListDeinit(&pauseMenu.items);
	GUIMenuItemListDeinit(&stateSaveMenu.items);
	GUIMenuItemListDeinit(&stateLoadMenu.items);
	mLOG(GUI_RUNNER, INFO, "Game stopped!");
}

void mGUIRunloop(struct mGUIRunner* runner) {
	if (runner->keySources) {
		mLOG(GUI_RUNNER, DEBUG, "Loading key sources for %s...", runner->config.port);
		size_t i;
		for (i = 0; runner->keySources[i].id; ++i) {
			mInputMapLoad(&runner->params.keyMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
		}
	}
	while (!runner->running || runner->running(runner)) {
		char path[PATH_MAX];
		const char* preselect = mCoreConfigGetValue(&runner->config, "lastGame");
		if (preselect) {
			preselect = strrchr(preselect, '/');
		}
		if (preselect) {
			++preselect;
		}
		if (!GUISelectFile(&runner->params, path, sizeof(path), _testExtensions, NULL, preselect)) {
			break;
		}
		mCoreConfigSetValue(&runner->config, "lastDirectory", runner->params.currentPath);
		mCoreConfigSetValue(&runner->config, "lastGame", path);
		mCoreConfigSave(&runner->config);
		mGUIRun(runner, path);
	}
}

#ifndef DISABLE_THREADING
THREAD_ENTRY mGUIAutosaveThread(void* context) {
	struct mGUIAutosaveContext* autosave = context;
	MutexLock(&autosave->mutex);
	while (autosave->running) {
		ConditionWait(&autosave->cond, &autosave->mutex);
		if (autosave->running && autosave->core) {
			struct VFile* vf = mCoreGetState(autosave->core, 0, true);
			void* mem = autosave->buffer->map(autosave->buffer, autosave->buffer->size(autosave->buffer), MAP_READ);
			vf->write(vf, mem, autosave->buffer->size(autosave->buffer));
			autosave->buffer->unmap(autosave->buffer, mem, autosave->buffer->size(autosave->buffer));
			vf->close(vf);
		}
	}
	MutexUnlock(&autosave->mutex);
}
#endif
