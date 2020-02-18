/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include <mgba/internal/gba/gba.h>
#include "feature/gui/gui-runner.h"
#include <mgba-util/gui.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/menu.h>

#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/sysmodule.h>
#include <psp2/system_param.h>
#include <psp2/touch.h>

#include <vita2d.h>

static void _drawStart(void) {
	static int vcount = 0;
	extern bool frameLimiter;
	int oldVCount = vcount;
	vcount = sceDisplayGetVcount();
	vita2d_set_vblank_wait(frameLimiter && vcount + 1 >= oldVCount);
	vita2d_start_drawing();
	vita2d_clear_screen();
}

static void _drawEnd(void) {
	vita2d_end_drawing();
	vita2d_swap_buffers();
}

static uint32_t _pollInput(const struct mInputMap* map) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositiveExt2(0, &pad, 1);
	int input = mInputMapKeyBits(map, PSP2_INPUT, pad.buttons, 0);

	if (pad.buttons & SCE_CTRL_UP || pad.ly < 64) {
		input |= 1 << GUI_INPUT_UP;
	}
	if (pad.buttons & SCE_CTRL_DOWN || pad.ly >= 192) {
		input |= 1 << GUI_INPUT_DOWN;
	}
	if (pad.buttons & SCE_CTRL_LEFT || pad.lx < 64) {
		input |= 1 << GUI_INPUT_LEFT;
	}
	if (pad.buttons & SCE_CTRL_RIGHT || pad.lx >= 192) {
		input |= 1 << GUI_INPUT_RIGHT;
	}

	return input;
}

static enum GUICursorState _pollCursor(unsigned* x, unsigned* y) {
	SceTouchData touch;
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
	if (touch.reportNum < 1) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	*x = touch.report[0].x / 2;
	*y = touch.report[0].y / 2;
	return GUI_CURSOR_DOWN;
}

static int _batteryState(void) {
	int charge = scePowerGetBatteryLifePercent();
	int adapter = scePowerIsPowerOnline();
	int state = 0;
	if (adapter) {
		state |= BATTERY_CHARGING;
	}
	charge /= 25;
	return state | charge;
}

int main() {
	vita2d_init();
	struct GUIFont* font = GUIFontCreate();
	struct mGUIRunner runner = {
		.params = {
			PSP2_HORIZONTAL_PIXELS, PSP2_VERTICAL_PIXELS,
			font, "",
			_drawStart, _drawEnd,
			_pollInput, _pollCursor,
			_batteryState,
			0, 0,
		},
		.configExtra = (struct GUIMenuItem[]) {
			{
				.title = "Screen mode",
				.data = "screenMode",
				.submenu = 0,
				.state = 0,
				.validStates = (const char*[]) {
					"With Background",
					"Without Background",
					"Stretched",
					"Fit Aspect Ratio",
				},
				.nStates = 4
			},
			{
				.title = "Camera",
				.data = "camera",
				.submenu = 0,
				.state = 1,
				.validStates = (const char*[]) {
					"None",
					"Front",
					"Back",
				},
				.nStates = 3
			}
		},
		.keySources = (struct GUIInputKeys[]) {
			{
				.name = "Vita Input",
				.id = PSP2_INPUT,
				.keyNames = (const char*[]) {
					"Select",
					"L3",
					"R3",
					"Start",
					"Up",
					"Right",
					"Down",
					"Left",
					"L2",
					"R2",
					"L1",
					"R1",
					"\1\xC",
					"\1\xA",
					"\1\xB",
					"\1\xD"
				},
				.nKeys = 16
			},
			{ .id = 0 }
		},
		.nConfigExtra = 2,
		.setup = mPSP2Setup,
		.teardown = mPSP2Teardown,
		.gameLoaded = mPSP2LoadROM,
		.gameUnloaded = mPSP2UnloadROM,
		.prepareForFrame = mPSP2Swap,
		.drawFrame = mPSP2Draw,
		.drawScreenshot = mPSP2DrawScreenshot,
		.paused = mPSP2Paused,
		.unpaused = mPSP2Unpaused,
		.incrementScreenMode = mPSP2IncrementScreenMode,
		.setFrameLimiter = mPSP2SetFrameLimiter,
		.pollGameInput = mPSP2PollInput,
		.running = mPSP2SystemPoll
	};

	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
	sceSysmoduleLoadModule(SCE_SYSMODULE_PHOTO_EXPORT);
	sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);

	mGUIInit(&runner, "psvita");

	int enterButton;
	SceAppUtilInitParam initParam;
	SceAppUtilBootParam bootParam;
	memset(&initParam, 0, sizeof(SceAppUtilInitParam));
	memset(&bootParam, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&initParam, &bootParam);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &enterButton);
	sceAppUtilShutdown();

	if (enterButton == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) {
		mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_CROSS, GUI_INPUT_BACK);
		mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_CIRCLE, GUI_INPUT_SELECT);
	} else {
		mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_CROSS, GUI_INPUT_SELECT);
		mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_CIRCLE, GUI_INPUT_BACK);
	}
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_TRIANGLE, GUI_INPUT_CANCEL);
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_UP, GUI_INPUT_UP);
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_DOWN, GUI_INPUT_DOWN);
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_LEFT, GUI_INPUT_LEFT);
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_RIGHT, GUI_INPUT_RIGHT);
	mPSP2MapKey(&runner.params.keyMap, SCE_CTRL_SQUARE, mGUI_INPUT_SCREEN_MODE);

	scePowerSetArmClockFrequency(444);
	mGUIRunloop(&runner);

	vita2d_fini();
	mGUIDeinit(&runner);

	int pgfLoaded = sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF);
	if (pgfLoaded != SCE_SYSMODULE_LOADED) {
		sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
	}
	GUIFontDestroy(font);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);

	sceKernelExitProcess(0);
	return 0;
}
