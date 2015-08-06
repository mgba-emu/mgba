/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define asm __asm__

#include <fat.h>
#include <gccore.h>
#include <malloc.h>

#include "util/common.h"

#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"
#include "gba/supervisor/overrides.h"
#include "gba/video.h"
#include "util/vfs.h"

#define SAMPLES 1024

static void GBAWiiLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);
static void GBAWiiFrame(void);
static bool GBAWiiLoadGame(const char* path);

static void _postVideoFrame(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
static void _audioDMA(void);

static struct GBA gba;
static struct ARMCore cpu;
static struct GBAVideoSoftwareRenderer renderer;
static struct VFile* rom;
static struct VFile* save;
static struct GBAAVStream stream;
static FILE* logfile;
static GXRModeObj* mode;
static Mtx model, view, modelview;
static uint16_t* texmem;
static GXTexObj tex;

static void* framebuffer[2];
static int whichFb = 0;

static struct GBAStereoSample audioBuffer[3][SAMPLES] __attribute__((__aligned__(32)));
static volatile size_t audioBufferSize = 0;
static volatile int currentAudioBuffer = 0;

int main() {
	VIDEO_Init();
	PAD_Init();
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

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_InvVtxCache();
	GX_InvalidateTexAll();

	Mtx44 proj;
	guOrtho(proj, 0, VIDEO_VERTICAL_PIXELS, 0, VIDEO_HORIZONTAL_PIXELS, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

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

	fatInitDefault();

	logfile = fopen("/mgba.log", "w");

	stream.postAudioFrame = 0;
	stream.postAudioBuffer = 0;
	stream.postVideoFrame = _postVideoFrame;

	GBACreate(&gba);
	ARMSetComponents(&cpu, &gba.d, 0, 0);
	ARMInit(&cpu);
	gba.logLevel = 0; // TODO: Settings
	gba.logHandler = GBAWiiLog;
	gba.stream = &stream;
	gba.idleOptimization = IDLE_LOOP_REMOVE; // TODO: Settings
	rom = 0;

	GBAVideoSoftwareRendererCreate(&renderer);
	renderer.outputBuffer = memalign(32, 256 * 256 * BYTES_PER_PIXEL);
	renderer.outputBufferStride = 256;
	GBAVideoAssociateRenderer(&gba.video, &renderer.d);

	GBAAudioResizeBuffer(&gba.audio, SAMPLES);

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	blip_set_rates(gba.audio.left,  GBA_ARM7TDMI_FREQUENCY, 48000);
	blip_set_rates(gba.audio.right, GBA_ARM7TDMI_FREQUENCY, 48000);
#endif

	if (!GBAWiiLoadGame("/rom.gba")) {
		return 1;
	}

	while (true) {
#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
		int available = blip_samples_avail(gba.audio.left);
		if (available + audioBufferSize > SAMPLES) {
			available = SAMPLES - audioBufferSize;
		}
		available &= ~((32 / sizeof(struct GBAStereoSample)) - 1); // Force align to 32 bytes
		if (available > 0) {
			blip_read_samples(gba.audio.left, &audioBuffer[currentAudioBuffer][audioBufferSize].left, available, true);
			blip_read_samples(gba.audio.right, &audioBuffer[currentAudioBuffer][audioBufferSize].right, available, true);
			audioBufferSize += available;
		}
		if (audioBufferSize == SAMPLES && !AUDIO_GetDMAEnableFlag()) {
			_audioDMA();
			AUDIO_StartDMA();
		}
#endif
		PAD_ScanPads();
		u16 padkeys = PAD_ButtonsHeld(0);
		int keys = 0;
		gba.keySource = &keys;
		if (padkeys & PAD_BUTTON_A) {
			keys |= 1 << GBA_KEY_A;
		}
		if (padkeys & PAD_BUTTON_B) {
			keys |= 1 << GBA_KEY_B;
		}
		if (padkeys & PAD_TRIGGER_L) {
			keys |= 1 << GBA_KEY_L;
		}
		if (padkeys & PAD_TRIGGER_R) {
			keys |= 1 << GBA_KEY_R;
		}
		if (padkeys & PAD_BUTTON_START) {
			keys |= 1 << GBA_KEY_START;
		}
		if (padkeys & (PAD_BUTTON_X | PAD_BUTTON_Y)) {
			keys |= 1 << GBA_KEY_SELECT;
		}
		if (padkeys & PAD_BUTTON_LEFT) {
			keys |= 1 << GBA_KEY_LEFT;
		}
		if (padkeys & PAD_BUTTON_RIGHT) {
			keys |= 1 << GBA_KEY_RIGHT;
		}
		if (padkeys & PAD_BUTTON_UP) {
			keys |= 1 << GBA_KEY_UP;
		}
		if (padkeys & PAD_BUTTON_DOWN) {
			keys |= 1 << GBA_KEY_DOWN;
		}
		if (padkeys & PAD_TRIGGER_Z) {
			break;
		}
		int frameCount = gba.video.frameCounter;
		while (gba.video.frameCounter == frameCount) {
			ARMRunLoop(&cpu);
		}
	}

	fclose(logfile);
	free(fifo);

	rom->close(rom);
	save->close(save);

	return 0;
}

static void GBAWiiFrame(void) {
	VIDEO_WaitVSync();

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

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	GX_SetViewport(0, 0, mode->fbWidth, mode->efbHeight, 0, 1);
	GX_InvalidateTexAll();
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(0, 256);
	GX_TexCoord2s16(0, 1);

	GX_Position2s16(256, 256);
	GX_TexCoord2s16(1, 1);

	GX_Position2s16(256, 0);
	GX_TexCoord2s16(1, 0);

	GX_Position2s16(0, 0);
	GX_TexCoord2s16(0, 0);
	GX_End();

	GX_DrawDone();

	whichFb = !whichFb;

	GX_CopyDisp(framebuffer[whichFb], GX_TRUE);
	VIDEO_SetNextFramebuffer(framebuffer[whichFb]);
	VIDEO_Flush();
}

bool GBAWiiLoadGame(const char* path) {
	rom = VFileOpen(path, O_RDONLY);

	if (!rom) {
		return false;
	}
	if (!GBAIsROM(rom)) {
		return false;
	}

	save = VFileOpen("test.sav", O_RDWR | O_CREAT);

	GBALoadROM(&gba, rom, save, path);

	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) gba.memory.rom;
	memcpy(override.id, &cart->id, sizeof(override.id));
	if (GBAOverrideFind(0, &override)) {
		GBAOverrideApply(&gba, &override);
	}

	ARMReset(&cpu);
	return true;
}

void GBAWiiLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	UNUSED(level);
	vfprintf(logfile, format, args);
	fprintf(logfile, "\n");
	fflush(logfile);
}

static void _postVideoFrame(struct GBAAVStream* stream, struct GBAVideoRenderer* renderer) {
	UNUSED(stream);
	UNUSED(renderer);
	GBAWiiFrame();
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
