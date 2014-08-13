#include "debugger/debugger.h"
#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"
#include "sdl-audio.h"
#include "sdl-events.h"

#include <SDL/SDL.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <bcm_host.h>

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <sys/time.h>

struct GBAVideoEGLRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGL_DISPMANX_WINDOW_T window;
	GLuint tex;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;
	GLuint bufferObject;
	GLuint texLocation;
	GLuint positionLocation;
};

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

static int _GBAEGLInit(struct GBAVideoEGLRenderer* renderer);
static void _GBAEGLDeinit(struct GBAVideoEGLRenderer* renderer);
static void _GBAEGLRunloop(struct GBAThread* context, struct GBAVideoEGLRenderer* renderer);
static void _GBASDLStart(struct GBAThread* context);
static void _GBASDLClean(struct GBAThread* context);

int main(int argc, char** argv) {
	struct GBAVideoEGLRenderer renderer;

	struct StartupOptions opts;
	if (!parseCommandArgs(&opts, argc, argv, 0)) {
		usage(argv[0], 0);
		freeOptions(&opts);
		return 1;
	}

	if (!_GBAEGLInit(&renderer)) {
		return 1;
	}
	GBAVideoSoftwareRendererCreate(&renderer.d);

	struct GBAThread context = {
		.renderer = &renderer.d.d,
		.sync.videoFrameWait = 0,
		.sync.audioWait = 1,
		.startCallback = _GBASDLStart,
		.cleanCallback = _GBASDLClean,
		.userData = &renderer
	};

	context.debugger = createDebugger(&opts);

	GBAMapOptionsToContext(&opts, &context);

	renderer.audio.samples = context.audioBuffers;
	GBASDLInitAudio(&renderer.audio);

	renderer.events.bindings = &context.inputMap;
	GBASDLInitEvents(&renderer.events);

	GBAThreadStart(&context);

	_GBAEGLRunloop(&context, &renderer);

	GBAThreadJoin(&context);
	freeOptions(&opts);
	free(context.debugger);

	_GBAEGLDeinit(&renderer);

	return 0;
}

static int _GBAEGLInit(struct GBAVideoEGLRenderer* renderer) {
	if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
		return 0;
	}

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
		return 0;
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
		return 0;
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
	return 1;
}

static void _GBAEGLRunloop(struct GBAThread* context, struct GBAVideoEGLRenderer* renderer) {
	SDL_Event event;

	while (context->state < THREAD_EXITING) {
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

		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->events, &event);
		}
	}
}

static void _GBAEGLDeinit(struct GBAVideoEGLRenderer* renderer) {
	eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(renderer->display, renderer->surface);
	eglDestroyContext(renderer->display, renderer->context);
	eglTerminate(renderer->display);

	GBASDLDeinitEvents(&renderer->events);
	GBASDLDeinitAudio(&renderer->audio);
	SDL_Quit();

	bcm_host_deinit();
}

static void _GBASDLStart(struct GBAThread* threadContext) {
	struct GBAVideoEGLRenderer* renderer = threadContext->userData;
	renderer->audio.audio = &threadContext->gba->audio;
	renderer->audio.thread = threadContext;
}

static void _GBASDLClean(struct GBAThread* threadContext) {
	struct GBAVideoEGLRenderer* renderer = threadContext->userData;
	renderer->audio.audio = 0;
}
