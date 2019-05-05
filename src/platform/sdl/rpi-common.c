/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/version.h>

void mRPIGLCommonSwap(struct VideoBackend* context) {
	struct mSDLRenderer* renderer = (struct mSDLRenderer*) context->user;
	eglSwapBuffers(renderer->eglDisplay, renderer->eglSurface);
}

void mRPIGLCommonInit(struct mSDLRenderer* renderer) {
	bcm_host_init();
	renderer->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	int major, minor;
	if (EGL_FALSE == eglInitialize(renderer->eglDisplay, &major, &minor)) {
		printf("Failed to initialize EGL");
		return false;
	}

	if (EGL_FALSE == eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("Failed to get GLES API");
		return false;
	}

	const EGLint requestConfig[] = {
		EGL_RED_SIZE, 5,
		EGL_GREEN_SIZE, 5,
		EGL_BLUE_SIZE, 5,
		EGL_ALPHA_SIZE, 1,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	EGLConfig config;
	EGLint numConfigs;

	if (EGL_FALSE == eglChooseConfig(renderer->eglDisplay, requestConfig, &config, 1, &numConfigs)) {
		printf("Failed to choose EGL config\n");
		return false;
	}

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	int dispWidth = 240, dispHeight = 160, adjWidth;
	renderer->eglContext = eglCreateContext(renderer->eglDisplay, config, EGL_NO_CONTEXT, contextAttributes);
	graphics_get_display_size(0, &dispWidth, &dispHeight);
	adjWidth = dispHeight / 2 * 3;

	DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);

	VC_RECT_T destRect = {
		.x = (dispWidth - adjWidth) / 2,
		.y = 0,
		.width = adjWidth,
		.height = dispHeight
	};

	VC_RECT_T srcRect = {
		.x = 0,
		.y = 0,
		.width = 240 << 16,
		.height = 160 << 16
	};

	DISPMANX_ELEMENT_HANDLE_T element = vc_dispmanx_element_add(update, display, 0, &destRect, 0, &srcRect, DISPMANX_PROTECTION_NONE, 0, 0, 0);
	vc_dispmanx_update_submit_sync(update);

	renderer->eglWindow.element = element;
	renderer->eglWindow.width = dispWidth;
	renderer->eglWindow.height = dispHeight;

	renderer->eglSurface = eglCreateWindowSurface(renderer->eglDisplay, config, &renderer->eglWindow, 0);
	if (EGL_FALSE == eglMakeCurrent(renderer->eglDisplay, renderer->eglSurface, renderer->eglSurface, renderer->eglContext)) {
		return false;
	}
}
