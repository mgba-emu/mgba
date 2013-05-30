#include "debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-glsl.h"
#include "sdl-events.h"

#include <SDL.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <bcm_host.h>

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

struct GBAVideoEGLRenderer {
	struct GBAVideoGLSLRenderer d;

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
};

static int _GBAEGLInit(struct GBAVideoEGLRenderer* renderer);
static void _GBAEGLDeinit(struct GBAVideoEGLRenderer* renderer);
static void _GBAEGLRunloop(struct GBAThread* context, struct GBAVideoEGLRenderer* renderer);

int main(int argc, char** argv) {
	const char* fname = "test.rom";
	if (argc > 1) {
		fname = argv[1];
	}
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return 1;
	}

	struct GBAThread context;
	struct GBAVideoEGLRenderer renderer;

	if (!_GBAEGLInit(&renderer)) {
		return 1;
	}
	GBAVideoGLSLRendererCreate(&renderer.d);

	context.fd = fd;
	context.renderer = &renderer.d.d;
	GBAThreadStart(&context);

	_GBAEGLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	close(fd);

	_GBAEGLDeinit(&renderer);

	return 0;
}

static int _GBAEGLInit(struct GBAVideoEGLRenderer* renderer) {
	if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
		return 0;
	}

	GBASDLInitEvents();
	bcm_host_init();
	renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	int major, minor;
	if (EGL_FALSE == eglInitialize(renderer->display, &major, &minor)) {
		printf("Failed to initialize EGL");
		return 0;
	}

	if (EGL_FALSE == eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("Failed to get GLES API");
		return 0;
	}

	const EGLint requestConfig[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLConfig config;
	EGLint numConfigs;

	if (EGL_FALSE == eglChooseConfig(renderer->display, requestConfig, &config, 1, &numConfigs)) {
		printf("Failed to choose EGL config\n");
		return 0;
	}

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, contextAttributes);

	DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);

	VC_RECT_T destRect = {
		.x = 0,
		.y = 0,
		.width = 240,
		.height = 160
	};

	VC_RECT_T srcRect = {
		.x = 0,
		.y = 0,
		.width = 240,
		.height = 160
	};

	DISPMANX_ELEMENT_HANDLE_T element = vc_dispmanx_element_add(update, display, 0, &destRect, 0, &srcRect, DISPMANX_PROTECTION_NONE, 0, 0, DISPMANX_NO_ROTATE);
	vc_dispmanx_update_submit_sync(update);

	EGL_DISPMANX_WINDOW_T window = {
		.element = element,
		.width = 240,
		.height = 160
	};

	renderer->surface = eglCreateWindowSurface(renderer->display, config, &window, 0);
	return EGL_TRUE == eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context);
}

static void _GBAEGLRunloop(struct GBAThread* context, struct GBAVideoEGLRenderer* renderer) {
	SDL_Event event;

	while (context->started && context->debugger->state != DEBUGGER_EXITING) {
		GBAVideoGLSLRendererProcessEvents(&renderer->d);
		pthread_mutex_lock(&renderer->d.mutex);
		if (renderer->d.d.framesPending) {
			renderer->d.d.framesPending = 0;
			pthread_mutex_unlock(&renderer->d.mutex);

			eglSwapBuffers(renderer->display, renderer->surface);

			while (SDL_PollEvent(&event)) {
				GBASDLHandleEvent(context, &event);
			}
			pthread_mutex_lock(&renderer->d.mutex);
			pthread_cond_broadcast(&renderer->d.downCond);
		} else {
			pthread_cond_broadcast(&renderer->d.downCond);
			pthread_cond_wait(&renderer->d.upCond, &renderer->d.mutex);
		}
		pthread_mutex_unlock(&renderer->d.mutex);
	}
}

static void _GBAEGLDeinit(struct GBAVideoEGLRenderer* renderer) {
	eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(renderer->display, renderer->surface);
	eglDestroyContext(renderer->display, renderer->context);
	eglTerminate(renderer->display);

	GBASDLDeinitEvents();
	SDL_Quit();

	bcm_host_deinit();
}
