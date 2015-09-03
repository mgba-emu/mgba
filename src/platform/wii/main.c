/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define asm __asm__

#include <fat.h>
#include <gccore.h>
#include <malloc.h>
#include <wiiuse/wpad.h>

#include "util/common.h"

#include "gba/renderers/video-software.h"
#include "gba/context/context.h"
#include "gba/gui/gui-runner.h"
#include "util/gui.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/vfs.h"

#define SAMPLES 1024

static void GBAWiiLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);

static void _audioDMA(void);
static void _setRumble(struct GBARumble* rumble, int enable);
static void _sampleRotation(struct GBARotationSource* source);
static int32_t _readTiltX(struct GBARotationSource* source);
static int32_t _readTiltY(struct GBARotationSource* source);
static int32_t _readGyroZ(struct GBARotationSource* source);

static void _drawStart(void);
static void _drawEnd(void);
static uint32_t _pollInput(void);
static void _guiPrepare(void);
static void _guiFinish(void);

static void _setup(struct GBAGUIRunner* runner);
static void _gameLoaded(struct GBAGUIRunner* runner);
static void _gameUnloaded(struct GBAGUIRunner* runner);
static void _drawFrame(struct GBAGUIRunner* runner, bool faded);
static uint16_t _pollGameInput(struct GBAGUIRunner* runner);

static struct GBAVideoSoftwareRenderer renderer;
static struct GBARumble rumble;
static struct GBARotationSource rotation;
static FILE* logfile;
static GXRModeObj* mode;
static Mtx model, view, modelview;
static uint16_t* texmem;
static GXTexObj tex;
static int32_t tiltX;
static int32_t tiltY;
static int32_t gyroZ;

static void* framebuffer[2];
static int whichFb = 0;

static struct GBAStereoSample audioBuffer[3][SAMPLES] __attribute__((__aligned__(32)));
static volatile size_t audioBufferSize = 0;
static volatile int currentAudioBuffer = 0;

static struct GUIFont* font;

int main() {
	VIDEO_Init();
	PAD_Init();
	WPAD_Init();
	AUDIO_Init(0);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(_audioDMA);

	memset(audioBuffer, 0, sizeof(audioBuffer));

#if !defined(COLOR_16_BIT) && !defined(COLOR_5_6_5)
#error This pixel format is unsupported. Please use -DCOLOR_16-BIT -DCOLOR_5_6_5
#endif

	mode = VIDEO_GetPreferredMode(0);
	framebuffer[0] = SYS_AllocateFramebuffer(mode);
	framebuffer[1] = SYS_AllocateFramebuffer(mode);

	VIDEO_Configure(mode);
	VIDEO_SetNextFramebuffer(framebuffer[whichFb]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (mode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}

	GXColor bg = { 0, 0, 0, 0xFF };
	void* fifo = memalign(32, 0x40000);
	memset(fifo, 0, 0x40000);
	GX_Init(fifo, 0x40000);
	GX_SetCopyClear(bg, 0x00FFFFFF);
	GX_SetViewport(0, 0, mode->fbWidth, mode->efbHeight, 0, 1);

	f32 yscale = GX_GetYScaleFactor(mode->efbHeight, mode->xfbHeight);
	u32 xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, mode->viWidth, mode->viWidth);
	GX_SetDispCopySrc(0, 0, mode->fbWidth, mode->efbHeight);
	GX_SetDispCopyDst(mode->fbWidth, xfbHeight);
	GX_SetCopyFilter(mode->aa, mode->sample_pattern, GX_TRUE, mode->vfilter);
	GX_SetFieldMode(mode->field_rendering, ((mode->viHeight == 2 * mode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(framebuffer[whichFb], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_InvVtxCache();
	GX_InvalidateTexAll();

	guVector cam = { 0.0f, 0.0f, 0.0f };
	guVector up = { 0.0f, 1.0f, 0.0f };
	guVector look = { 0.0f, 0.0f, -1.0f };
	guLookAt(view, &cam, &up, &look);

	guMtxIdentity(model);
	guMtxTransApply(model, model, 0.0f, 0.0f, -6.0f);
	guMtxConcat(view, model, modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	texmem = memalign(32, 256 * 256 * BYTES_PER_PIXEL);
	memset(texmem, 0, 256 * 256 * BYTES_PER_PIXEL);
	GX_InitTexObj(&tex, texmem, 256, 256, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

	font = GUIFontCreate();

	fatInitDefault();

	logfile = fopen("/mgba.log", "w");

	rumble.setRumble = _setRumble;

	rotation.sample = _sampleRotation;
	rotation.readTiltX = _readTiltX;
	rotation.readTiltY = _readTiltY;
	rotation.readGyroZ = _readGyroZ;

	struct GBAGUIRunner runner = {
		.params = {
			352, 230,
			font, "/",
			_drawStart, _drawEnd, _pollInput,
			_guiPrepare, _guiFinish,

			GUI_PARAMS_TRAIL
		},
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = 0,
		.drawFrame = _drawFrame,
		.paused = _gameUnloaded,
		.unpaused = 0,
		.pollGameInput = _pollGameInput
	};
	GBAGUIInit(&runner, 0);
	GBAGUIRunloop(&runner);
	GBAGUIDeinit(&runner);

	fclose(logfile);
	free(fifo);

	free(renderer.outputBuffer);
	GUIFontDestroy(font);

	return 0;
}

void GBAWiiLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	UNUSED(level);
	if (!logfile) {
		return;
	}
	vfprintf(logfile, format, args);
	fprintf(logfile, "\n");
	fflush(logfile);
}

static void _audioDMA(void) {
	if (!audioBufferSize) {
		return;
	}
	DCFlushRange(audioBuffer[currentAudioBuffer], audioBufferSize * sizeof(struct GBAStereoSample));
	AUDIO_InitDMA((u32) audioBuffer[currentAudioBuffer], audioBufferSize * sizeof(struct GBAStereoSample));
	currentAudioBuffer = (currentAudioBuffer + 1) % 3;
	audioBufferSize = 0;
}

static void _drawStart(void) {
	VIDEO_WaitVSync();
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	GX_SetViewport(0, 0, mode->fbWidth, mode->efbHeight, 0, 1);
}

static void _drawEnd(void) {
	GX_DrawDone();

	whichFb = !whichFb;

	GX_CopyDisp(framebuffer[whichFb], GX_TRUE);
	VIDEO_SetNextFramebuffer(framebuffer[whichFb]);
	VIDEO_Flush();
}

static uint32_t _pollInput(void) {
	PAD_ScanPads();
	u16 padkeys = PAD_ButtonsHeld(0);

	WPAD_ScanPads();
	u32 wiiPad = WPAD_ButtonsHeld(0);
	u32 ext = 0;
	WPAD_Probe(0, &ext);

	int keys = 0;
	int x = PAD_StickX(0);
	int y = PAD_StickY(0);
	if (x < -0x40) {
		keys |= 1 << GUI_INPUT_LEFT;
	}
	if (x > 0x40) {
		keys |= 1 << GUI_INPUT_RIGHT;
	}
	if (y < -0x40) {
		keys |= 1 << GUI_INPUT_DOWN;
	}
	if (y > 0x40) {
		keys |= 1 << GUI_INPUT_UP;
	}
	if ((padkeys & PAD_BUTTON_A) || (wiiPad & WPAD_BUTTON_2) || 
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_Y)))) {
		keys |= 1 << GUI_INPUT_SELECT;
	}
	if ((padkeys & PAD_BUTTON_B) || (wiiPad & WPAD_BUTTON_1) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & (WPAD_CLASSIC_BUTTON_B | WPAD_CLASSIC_BUTTON_X)))) {
		keys |= 1 << GUI_INPUT_BACK;
	}
	if ((padkeys & PAD_TRIGGER_Z) || (wiiPad & WPAD_BUTTON_HOME) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & (WPAD_CLASSIC_BUTTON_HOME)))) {
		keys |= 1 << GUI_INPUT_CANCEL;
	}
	if ((padkeys & PAD_BUTTON_LEFT)|| (wiiPad & WPAD_BUTTON_UP) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_LEFT))) {
		keys |= 1 << GUI_INPUT_LEFT;
	}
	if ((padkeys & PAD_BUTTON_RIGHT) || (wiiPad & WPAD_BUTTON_DOWN) ||
	   ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_RIGHT))) {
		keys |= 1 << GUI_INPUT_RIGHT;
	}
	if ((padkeys & PAD_BUTTON_UP) || (wiiPad & WPAD_BUTTON_RIGHT) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_UP))) {
		keys |= 1 << GUI_INPUT_UP;
	}
	if ((padkeys & PAD_BUTTON_DOWN) || (wiiPad & WPAD_BUTTON_LEFT) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_DOWN))) {
		keys |= 1 << GUI_INPUT_DOWN;
	}
	return keys;
}

void _guiPrepare(void) {
	Mtx44 proj;
	guOrtho(proj, -20, 240, 0, 352, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _guiFinish(void) {
	Mtx44 proj;
	guOrtho(proj, -10, VIDEO_VERTICAL_PIXELS + 10, 0, VIDEO_HORIZONTAL_PIXELS, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

void _setup(struct GBAGUIRunner* runner) {
	struct GBAOptions opts = {
		.useBios = true,
		.logLevel = 0,
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&runner->context.config, &opts);
	runner->context.gba->logHandler = GBAWiiLog;
	runner->context.gba->rumble = &rumble;
	runner->context.gba->rotationSource = &rotation;

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = memalign(32, 256 * 256 * BYTES_PER_PIXEL);
	renderer.outputBufferStride = 256;
	runner->context.renderer = &renderer.d;

	GBAAudioResizeBuffer(&runner->context.gba->audio, SAMPLES);

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	double ratio = GBAAudioCalculateRatio(1, 60, 1);
	blip_set_rates(runner->context.gba->audio.left,  GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);
	blip_set_rates(runner->context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 48000 * ratio);
#endif
}

void _gameUnloaded(struct GBAGUIRunner* runner) {
	UNUSED(runner);
	AUDIO_StopDMA();
}

void _gameLoaded(struct GBAGUIRunner* runner) {
	WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC);
	if (runner->context.gba->memory.hw.devices & HW_GYRO) {
		int i;
		for (i = 0; i < 6; ++i) {
			u32 result = WPAD_SetMotionPlus(0, 1);
			if (result == WPAD_ERR_NONE) {
				break;
			}
			sleep(1);
		}
	}
}

void _drawFrame(struct GBAGUIRunner* runner, bool faded) {
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	int available = blip_samples_avail(runner->context.gba->audio.left);
	if (available + audioBufferSize > SAMPLES) {
		available = SAMPLES - audioBufferSize;
	}
	available &= ~((32 / sizeof(struct GBAStereoSample)) - 1); // Force align to 32 bytes
	if (available > 0) {
		blip_read_samples(runner->context.gba->audio.left, &audioBuffer[currentAudioBuffer][audioBufferSize].left, available, true);
		blip_read_samples(runner->context.gba->audio.right, &audioBuffer[currentAudioBuffer][audioBufferSize].right, available, true);
		audioBufferSize += available;
	}
	if (audioBufferSize == SAMPLES && !AUDIO_GetDMAEnableFlag()) {
		_audioDMA();
		AUDIO_StartDMA();
	}
#endif

	uint32_t color = 0xFFFFFF3F;
	if (!faded) {
		color |= 0xC0;
	}
	size_t x, y;
	uint64_t* texdest = (uint64_t*) texmem;
	uint64_t* texsrc = (uint64_t*) renderer.outputBuffer;
	for (y = 0; y < VIDEO_VERTICAL_PIXELS; y += 4) {
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS >> 2; ++x) {
			texdest[0 + x * 4 + y * 64] = texsrc[0   + x + y * 64];
			texdest[1 + x * 4 + y * 64] = texsrc[64  + x + y * 64];
			texdest[2 + x * 4 + y * 64] = texsrc[128 + x + y * 64];
			texdest[3 + x * 4 + y * 64] = texsrc[192 + x + y * 64];
		}
	}
	DCFlushRange(texdest, 256 * 256 * BYTES_PER_PIXEL);

	if (faded) {
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	} else {
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	}
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_InvalidateTexAll();
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(0, 256);
	GX_Color1u32(color);
	GX_TexCoord2s16(0, 1);

	GX_Position2s16(256, 256);
	GX_Color1u32(color);
	GX_TexCoord2s16(1, 1);

	GX_Position2s16(256, 0);
	GX_Color1u32(color);
	GX_TexCoord2s16(1, 0);

	GX_Position2s16(0, 0);
	GX_Color1u32(color);
	GX_TexCoord2s16(0, 0);
	GX_End();
}

uint16_t _pollGameInput(struct GBAGUIRunner* runner) {
	UNUSED(runner);
	PAD_ScanPads();
	u16 padkeys = PAD_ButtonsHeld(0);
	WPAD_ScanPads();
	u32 wiiPad = WPAD_ButtonsHeld(0);
	u32 ext = 0;
	uint16_t keys = 0;
	WPAD_Probe(0, &ext);

	if ((padkeys & PAD_BUTTON_A) || (wiiPad & WPAD_BUTTON_2) || 
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & (WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_Y)))) {
		keys |= 1 << GBA_KEY_A;
	}
	if ((padkeys & PAD_BUTTON_B) || (wiiPad & WPAD_BUTTON_1) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & (WPAD_CLASSIC_BUTTON_B | WPAD_CLASSIC_BUTTON_X)))) {
		keys |= 1 << GBA_KEY_B;
	}
	if ((padkeys & PAD_TRIGGER_L) || (wiiPad & WPAD_BUTTON_B) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_FULL_L))) {
		keys |= 1 << GBA_KEY_L;
	}
	if ((padkeys & PAD_TRIGGER_R) || (wiiPad & WPAD_BUTTON_A) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_FULL_R))) {
		keys |= 1 << GBA_KEY_R;
	}
	if ((padkeys & PAD_BUTTON_START) || (wiiPad & WPAD_BUTTON_PLUS) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_PLUS))) {
		keys |= 1 << GBA_KEY_START;
	}
	if ((padkeys & (PAD_BUTTON_X | PAD_BUTTON_Y)) || (wiiPad & WPAD_BUTTON_MINUS) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_MINUS))) {
		keys |= 1 << GBA_KEY_SELECT;
	}
	if ((padkeys & PAD_BUTTON_LEFT) || (wiiPad & WPAD_BUTTON_UP) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_LEFT))) {
		keys |= 1 << GBA_KEY_LEFT;
	}
	if ((padkeys & PAD_BUTTON_RIGHT) || (wiiPad & WPAD_BUTTON_DOWN) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_RIGHT))) {
		keys |= 1 << GBA_KEY_RIGHT;
	}
	if ((padkeys & PAD_BUTTON_UP) || (wiiPad & WPAD_BUTTON_RIGHT) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_UP))) {
		keys |= 1 << GBA_KEY_UP;
	}
	if ((padkeys & PAD_BUTTON_DOWN) || (wiiPad & WPAD_BUTTON_LEFT) ||
	    ((ext == WPAD_EXP_CLASSIC) && (wiiPad & WPAD_CLASSIC_BUTTON_DOWN))) {
		keys |= 1 << GBA_KEY_DOWN;
	}
	int x = PAD_StickX(0);
	int y = PAD_StickY(0);
	if (x < -0x40) {
		keys |= 1 << GBA_KEY_LEFT;
	}
	if (x > 0x40) {
		keys |= 1 << GBA_KEY_RIGHT;
	}
	if (y < -0x40) {
		keys |= 1 << GBA_KEY_DOWN;
	}
	if (y > 0x40) {
		keys |= 1 << GBA_KEY_UP;
	}
	return keys;
}

void _setRumble(struct GBARumble* rumble, int enable) {
	UNUSED(rumble);
	WPAD_Rumble(0, enable);
	if (enable) {
		PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
	} else {
		PAD_ControlMotor(0, PAD_MOTOR_STOP);
	}
}

void _sampleRotation(struct GBARotationSource* source) {
	UNUSED(source);
	vec3w_t accel;
	WPAD_Accel(0, &accel);
	// These are swapped
	tiltX = (accel.y - 0x1EA) << 22;
	tiltY = (accel.x - 0x1EA) << 22;

	// This doesn't seem to work at all with -TR remotes
	struct expansion_t exp;
	WPAD_Expansion(0, &exp);
	if (exp.type != EXP_MOTION_PLUS) {
		return;
	}
	gyroZ = exp.mp.rz - 0x1FA0;
	gyroZ <<= 18;
}

int32_t _readTiltX(struct GBARotationSource* source) {
	UNUSED(source);
	return tiltX;
}

int32_t _readTiltY(struct GBARotationSource* source) {
	UNUSED(source);
	return tiltY;
}

int32_t _readGyroZ(struct GBARotationSource* source) {
	UNUSED(source);
	return gyroZ;
}
