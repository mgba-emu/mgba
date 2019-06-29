/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gba/supervisor/thread.h"

#include <linux/omapfb.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif

static bool mSDLInit(struct SDLSoftwareRenderer* renderer);
static void mSDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);
static void mSDLDeinit(struct SDLSoftwareRenderer* renderer);

void mSDLGLCreate(struct SDLSoftwareRenderer* renderer) {
	renderer->init = mSDLInit;
	renderer->deinit = mSDLDeinit;
	renderer->runloop = mSDLRunloop;
}

void mSDLSWCreate(struct SDLSoftwareRenderer* renderer) {
	renderer->init = mSDLInit;
	renderer->deinit = mSDLDeinit;
	renderer->runloop = mSDLRunloop;
}

bool mSDLInit(struct SDLSoftwareRenderer* renderer) {
	SDL_SetVideoMode(800, 480, 16, SDL_FULLSCREEN);

	renderer->odd = 0;
	renderer->fb = open("/dev/fb1", O_RDWR);
	if (renderer->fb < 0) {
		return false;
	}

	struct omapfb_plane_info plane;
	struct omapfb_mem_info mem;
	if (ioctl(renderer->fb, OMAPFB_QUERY_PLANE, &plane) < 0) {
		return false;
	}
	if (ioctl(renderer->fb, OMAPFB_QUERY_MEM, &mem) < 0) {
		return false;
	}

	if (plane.enabled) {
		plane.enabled = 0;
		ioctl(renderer->fb, OMAPFB_SETUP_PLANE, &plane);
	}

	mem.size = GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4;
	ioctl(renderer->fb, OMAPFB_SETUP_MEM, &mem);

	plane.enabled = 1;
	plane.pos_x = 40;
	plane.pos_y = 0;
	plane.out_width = 720;
	plane.out_height = 480;
	ioctl(renderer->fb, OMAPFB_SETUP_PLANE, &plane);

	struct fb_var_screeninfo info;
	ioctl(renderer->fb, FBIOGET_VSCREENINFO, &info);
	info.xres = GBA_VIDEO_HORIZONTAL_PIXELS;
	info.yres = GBA_VIDEO_VERTICAL_PIXELS;
	info.xres_virtual = GBA_VIDEO_HORIZONTAL_PIXELS;
	info.yres_virtual = GBA_VIDEO_VERTICAL_PIXELS * 2;
	info.bits_per_pixel = 16;
	ioctl(renderer->fb, FBIOPUT_VSCREENINFO, &info);

	renderer->odd = 0;
	renderer->base[0] = mmap(0, GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4, PROT_READ | PROT_WRITE, MAP_SHARED, renderer->fb, 0);
	renderer->base[1] = (uint16_t*) renderer->base[0] + GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS;

	renderer->d.outputBuffer = renderer->base[0];
	renderer->d.outputBufferStride = GBA_VIDEO_HORIZONTAL_PIXELS;
	return true;
}

void mSDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEventGBA(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->sync)) {
			struct fb_var_screeninfo info;
			ioctl(renderer->fb, FBIOGET_VSCREENINFO, &info);
			info.yoffset = GBA_VIDEO_VERTICAL_PIXELS * renderer->odd;
			ioctl(renderer->fb, FBIOPAN_DISPLAY, &info);

			int arg = 0;
			ioctl(renderer->fb, FBIO_WAITFORVSYNC, &arg);

			renderer->odd = !renderer->odd;
			renderer->d.outputBuffer = renderer->base[renderer->odd];
		}
		mCoreSyncWaitFrameEnd(&context->sync);
	}
}

void mSDLDeinit(struct SDLSoftwareRenderer* renderer) {
	munmap(renderer->base[0], GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4);

	struct omapfb_plane_info plane;
	struct omapfb_mem_info mem;
	ioctl(renderer->fb, OMAPFB_QUERY_PLANE, &plane);
	ioctl(renderer->fb, OMAPFB_QUERY_MEM, &mem);

	mem.size = 0;
	ioctl(renderer->fb, OMAPFB_SETUP_MEM, &mem);

	plane.enabled = 0;
	plane.pos_x = 0;
	plane.pos_y = 0;
	plane.out_width = 0;
	plane.out_height = 0;
	ioctl(renderer->fb, OMAPFB_SETUP_PLANE, &plane);

	close(renderer->fb);
}
