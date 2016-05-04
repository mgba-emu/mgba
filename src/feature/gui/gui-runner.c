/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-runner.h"

#include "core/core.h"
#include "feature/gui/gui-config.h"
#include "gba/input.h"
#include "gba/interface.h"
#include "gba/serialize.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/gui/menu.h"
#include "util/memory.h"
#include "util/png-io.h"
#include "util/vfs.h"

#ifdef _3DS
#include <3ds.h>
#endif

#include <sys/time.h>

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
	RUNNER_COMMAND_MASK = 0xFFFF,

	RUNNER_STATE_1 = 0x10000,
	RUNNER_STATE_2 = 0x20000,
	RUNNER_STATE_3 = 0x30000,
	RUNNER_STATE_4 = 0x40000,
	RUNNER_STATE_5 = 0x50000,
	RUNNER_STATE_6 = 0x60000,
	RUNNER_STATE_7 = 0x70000,
	RUNNER_STATE_8 = 0x80000,
	RUNNER_STATE_9 = 0x90000,
};

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
		if (gbaBackground->screenshot && gbaBackground->screenshotId == (int) id) {
			gbaBackground->p->drawScreenshot(gbaBackground->p, gbaBackground->screenshot, true);
			return;
		}
		struct VFile* vf = mCoreGetState(gbaBackground->p->core, stateId, false);
		uint32_t* pixels = gbaBackground->screenshot;
		if (!pixels) {
			pixels = anonymousMemoryMap(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4);
			gbaBackground->screenshot = pixels;
		}
		bool success = false;
		if (vf && isPNG(vf) && pixels) {
			png_structp png = PNGReadOpen(vf, PNG_HEADER_BYTES);
			png_infop info = png_create_info_struct(png);
			png_infop end = png_create_info_struct(png);
			if (png && info && end) {
				success = PNGReadHeader(png, info);
				success = success && PNGReadPixels(png, info, pixels, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, VIDEO_HORIZONTAL_PIXELS);
				success = success && PNGReadFooter(png, end);
			}
			PNGReadClose(png, info, end);
		}
		if (vf) {
			vf->close(vf);
		}
		if (success) {
			gbaBackground->p->drawScreenshot(gbaBackground->p, pixels, true);
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
}

void mGUIDeinit(struct mGUIRunner* runner) {
	if (runner->teardown) {
		runner->teardown(runner);
	}
	CircleBufferDeinit(&runner->fpsBuffer);
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
	GUIMenuItemListInit(&stateLoadMenu.items, 9);
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Unpause", .data = (void*) RUNNER_CONTINUE };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Save state", .submenu = &stateSaveMenu };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Load state", .submenu = &stateLoadMenu };

	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_1) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_2) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_3) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_4) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_5) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_6) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_7) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_8) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_9) };

	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_1) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_2) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_3) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_4) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_5) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_6) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_7) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_8) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_9) };

	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Take screenshot", .data = (void*) RUNNER_SCREENSHOT };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Configure", .data = (void*) RUNNER_CONFIG };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Reset game", .data = (void*) RUNNER_RESET };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Exit game", .data = (void*) RUNNER_EXIT };

	// TODO: Message box API
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
	runner->core = mCoreFind(path);
	if (runner->core) {
		runner->core->init(runner->core);
		mInputMapInit(&runner->core->inputMap, &GBAInputInfo);
		mCoreInitConfig(runner->core, runner->port);
		found = mCoreLoadFile(runner->core, path);
		if (!found) {
			runner->core->deinit(runner->core);
		}
	}

	if (!found) {
		int i;
		for (i = 0; i < 240; ++i) {
			runner->params.drawStart();
			if (runner->params.guiPrepare) {
				runner->params.guiPrepare();
			}
			GUIFontPrint(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_ALIGN_HCENTER, 0xFFFFFFFF, "Load failed!");
			if (runner->params.guiFinish) {
				runner->params.guiFinish();
			}
			runner->params.drawEnd();
		}
		return;
	}
	if (runner->core->platform(runner->core) == PLATFORM_GBA) {
		((struct GBA*) runner->core->board)->luminanceSource = &runner->luminanceSource.d;
	}
	if (runner->core->config.port && runner->keySources) {
		size_t i;
		for (i = 0; runner->keySources[i].id; ++i) {
			mInputMapLoad(&runner->core->inputMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->core->config));
		}
	}
	// TODO: Do we need to load more defaults?
	mCoreConfigSetDefaultIntValue(&runner->core->config, "volume", 0x100);
	mCoreConfigSetDefaultValue(&runner->core->config, "idleOptimization", "detect");
	mCoreLoadConfig(runner->core);
	mCoreAutoloadSave(runner->core);
	if (runner->setup) {
		runner->setup(runner);
	}
	runner->core->reset(runner->core);
	bool running = true;
	if (runner->gameLoaded) {
		runner->gameLoaded(runner);
	}
	while (running) {
		CircleBufferClear(&runner->fpsBuffer);
		runner->totalDelta = 0;
		runner->fps = 0;
		struct timeval tv;
		gettimeofday(&tv, 0);
		runner->lastFpsCheck = 1000000LL * tv.tv_sec + tv.tv_usec;

		while (true) {
#ifdef _3DS
			running = aptMainLoop();
			if (!running) {
				break;
			}
#endif
			uint32_t guiKeys;
			GUIPollInput(&runner->params, &guiKeys, 0);
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
			uint16_t keys = runner->pollGameInput(runner);
			if (runner->prepareForFrame) {
				runner->prepareForFrame(runner);
			}
			runner->core->setKeys(runner->core, keys);
			runner->core->runFrame(runner->core);
			if (runner->drawFrame) {
				int drawFps = false;
				mCoreConfigGetIntValue(&runner->core->config, "fpsCounter", &drawFps);

				runner->params.drawStart();
				runner->drawFrame(runner, false);
				if (drawFps) {
					if (runner->params.guiPrepare) {
						runner->params.guiPrepare();
					}
					GUIFontPrintf(runner->params.font, 0, GUIFontHeight(runner->params.font), GUI_ALIGN_LEFT, 0x7FFFFFFF, "%.2f fps", runner->fps);
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
			}
		}

		if (runner->paused) {
			runner->paused(runner);
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
				mCoreSaveState(runner->core, ((int) item->data) >> 16, SAVESTATE_SCREENSHOT);
				break;
			case RUNNER_LOAD_STATE:
				mCoreLoadState(runner->core, ((int) item->data) >> 16, SAVESTATE_SCREENSHOT);
				break;
			case RUNNER_SCREENSHOT:
				mCoreTakeScreenshot(runner->core);
				break;
			case RUNNER_CONFIG:
				mGUIShowConfig(runner, runner->configExtra, runner->nConfigExtra);
				mCoreLoadConfig(runner->core);
				break;
			case RUNNER_CONTINUE:
				break;
			}
		}
		int frames = 0;
		GUIPollInput(&runner->params, 0, &keys);
		while (keys && frames < 30) {
			++frames;
			runner->params.drawStart();
			runner->drawFrame(runner, true);
			runner->params.drawEnd();
			GUIPollInput(&runner->params, 0, &keys);
		}
		if (runner->unpaused) {
			runner->unpaused(runner);
		}
	}
	if (runner->gameUnloaded) {
		runner->gameUnloaded(runner);
	}
	runner->core->unloadROM(runner->core);
	drawState.screenshotId = 0;
	if (drawState.screenshot) {
		mappedMemoryFree(drawState.screenshot, VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4);
	}

	if (runner->core->config.port) {
		if (runner->keySources) {
			size_t i;
			for (i = 0; runner->keySources[i].id; ++i) {
				mInputMapSave(&runner->core->inputMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->core->config));
			}
		}
		mCoreConfigSave(&runner->core->config);
	}
	runner->core->deinit(runner->core);

	GUIMenuItemListDeinit(&pauseMenu.items);
	GUIMenuItemListDeinit(&stateSaveMenu.items);
	GUIMenuItemListDeinit(&stateLoadMenu.items);
}

void mGUIRunloop(struct mGUIRunner* runner) {
	while (true) {
		char path[PATH_MAX];
		if (!GUISelectFile(&runner->params, path, sizeof(path), 0)) {
			break;
		}
		mGUIRun(runner, path);
	}
}
