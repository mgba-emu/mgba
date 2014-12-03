/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

static const char* _vertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position;\n"
	"	texCoord = (position.st + vec2(1.0, -1.0)) * vec2(0.46875, -0.3125);\n"
	"}";

static const char* _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 1.;\n"
	"	gl_FragColor = color;"
	"}";

static const GLfloat _vertices[] = {
	-1.f, -1.f,
	-1.f, 1.f,
	1.f, 1.f,
	1.f, -1.f,
};

bool GBASDLInit(struct SDLSoftwareRenderer* renderer) {
	bcm_host_init();
	renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	int major, minor;
	if (EGL_FALSE == eglInitialize(renderer->display, &major, &minor)) {
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

	if (EGL_FALSE == eglChooseConfig(renderer->display, requestConfig, &config, 1, &numConfigs)) {
		printf("Failed to choose EGL config\n");
		return false;
	}

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	int dispWidth = 240, dispHeight = 160, adjWidth;
	renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, contextAttributes);
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

	renderer->window.element = element;
	renderer->window.width = dispWidth;
	renderer->window.height = dispHeight;

	renderer->surface = eglCreateWindowSurface(renderer->display, config, &renderer->window, 0);
	if (EGL_FALSE == eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
		return false;
	}

	renderer->d.outputBuffer = memalign(16, 256 * 256 * 4);
	renderer->d.outputBufferStride = 256;
	glGenTextures(1, &renderer->tex);
	glBindTexture(GL_TEXTURE_2D, renderer->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	renderer->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	renderer->vertexShader = glCreateShader(GL_VERTEX_SHADER);
	renderer->program = glCreateProgram();

	glShaderSource(renderer->fragmentShader, 1, (const GLchar**) &_fragmentShader, 0);
	glShaderSource(renderer->vertexShader, 1, (const GLchar**) &_vertexShader, 0);
	glAttachShader(renderer->program, renderer->vertexShader);
	glAttachShader(renderer->program, renderer->fragmentShader);
	char log[1024];
	glCompileShader(renderer->fragmentShader);
	glCompileShader(renderer->vertexShader);
	glGetShaderInfoLog(renderer->fragmentShader, 1024, 0, log);
	glGetShaderInfoLog(renderer->vertexShader, 1024, 0, log);
	glLinkProgram(renderer->program);
	glGetProgramInfoLog(renderer->program, 1024, 0, log);
	printf("%s\n", log);
	renderer->texLocation = glGetUniformLocation(renderer->program, "tex");
	renderer->positionLocation = glGetAttribLocation(renderer->program, "position");
	glClearColor(1.f, 0.f, 0.f, 1.f);
}

void GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->events, &event);
		}

		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
			glViewport(0, 0, 240, 160);
			glClear(GL_COLOR_BUFFER_BIT);
			glUseProgram(renderer->program);
			glUniform1i(renderer->texLocation, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, renderer->tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, renderer->d.outputBuffer);
			glVertexAttribPointer(renderer->positionLocation, 2, GL_FLOAT, GL_FALSE, 0, _vertices);
			glEnableVertexAttribArray(renderer->positionLocation);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glUseProgram(0);
			eglSwapBuffers(renderer->display, renderer->surface);
		}
		GBASyncWaitFrameEnd(&context->sync);
	}
}

void GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(renderer->display, renderer->surface);
	eglDestroyContext(renderer->display, renderer->context);
	eglTerminate(renderer->display);
	bcm_host_deinit();
}
