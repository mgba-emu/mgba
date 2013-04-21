#include "video-software.h"

#include "gba.h"

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer);

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBAVideoSoftwareRendererInit;
	renderer->d.deinit = GBAVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoSoftwareRendererWriteVideoRegister;
	renderer->d.drawScanline = GBAVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoSoftwareRendererFinishFrame;

	{
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		renderer->mutex = mutex;
		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		renderer->cond = cond;
	}
}

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	pthread_mutex_init(&softwareRenderer->mutex, 0);
	pthread_cond_init(&softwareRenderer->cond, 0);
}

static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
}

static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	switch (address) {
	default:
			GBALog(GBA_LOG_STUB, "Stub video register write: %03x", address);
	}
	return value;
}

static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	int x;
	uint16_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	for (x = 0; x < 16; ++x) {
		row[(x * 15) + 0] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 1] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 2] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 3] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 4] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 5] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 6] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 7] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 8] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 9] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 10] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 11] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 12] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 13] = renderer->palette[x + (y / 5) * 16];
		row[(x * 15) + 14] = renderer->palette[x + (y / 5) * 16];
	}
}

static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	pthread_mutex_lock(&softwareRenderer->mutex);
	pthread_cond_wait(&softwareRenderer->cond, &softwareRenderer->mutex);
	pthread_mutex_unlock(&softwareRenderer->mutex);
}
