#include "video-glsl.h"

#include <string.h>

#define UNIFORM_LOCATION(UNIFORM) (glGetUniformLocation(glslRenderer->program, UNIFORM))

static const GLfloat _vertices[4] = {
	-1, 0,
	1, 0
};

static const GLchar* _fragmentShader[] = {
	"uniform float y;",
	"uniform sampler2D palette;",

	"void main() {",
	"	gl_FragColor = texture2D(palette, vec2(0, y / 256.0));",
	"}"
};

static const GLchar* _vertexShader[] = {
	"attribute vec2 vert;",
	"uniform float y;",

	"void main() {",
	"	gl_Position = vec4(vert.x, 1.0 - y / 80.0, 0, 1.0);",
	"}"
};

static void GBAVideoGLSLRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLSLRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLSLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoGLSLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoGLSLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoGLSLRendererFinishFrame(struct GBAVideoRenderer* renderer);

void GBAVideoGLSLRendererCreate(struct GBAVideoGLSLRenderer* glslRenderer) {
	glslRenderer->d.init = GBAVideoGLSLRendererInit;
	glslRenderer->d.deinit = GBAVideoGLSLRendererDeinit;
	glslRenderer->d.writeVideoRegister = GBAVideoGLSLRendererWriteVideoRegister;
	glslRenderer->d.writePalette = GBAVideoGLSLRendererWritePalette;
	glslRenderer->d.drawScanline = GBAVideoGLSLRendererDrawScanline;
	glslRenderer->d.finishFrame = GBAVideoGLSLRendererFinishFrame;

	glslRenderer->d.turbo = 0;
	glslRenderer->d.framesPending = 0;
	glslRenderer->d.frameskip = 0;

	glslRenderer->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glslRenderer->vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glslRenderer->program = glCreateProgram();

	glShaderSource(glslRenderer->fragmentShader, 5, _fragmentShader, 0);
	glShaderSource(glslRenderer->vertexShader, 5, _vertexShader, 0);

	glAttachShader(glslRenderer->program, glslRenderer->vertexShader);
	glAttachShader(glslRenderer->program, glslRenderer->fragmentShader);
	char log[1024];
	glCompileShader(glslRenderer->fragmentShader);
	glCompileShader(glslRenderer->vertexShader);
	glGetShaderInfoLog(glslRenderer->fragmentShader, 1024, 0, log);
	glGetShaderInfoLog(glslRenderer->vertexShader, 1024, 0, log);
	glLinkProgram(glslRenderer->program);

	glGenTextures(1, &glslRenderer->vramTexture);
	glBindTexture(GL_TEXTURE_2D, glslRenderer->vramTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	memset(glslRenderer->vram, 0, sizeof (glslRenderer->vram));

	{
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		glslRenderer->mutex = mutex;
		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		glslRenderer->upCond = cond;
		glslRenderer->downCond = cond;
	}
}

void GBAVideoGLSLRendererProcessEvents(struct GBAVideoGLSLRenderer* glslRenderer) {
		glUseProgram(glslRenderer->program);
		glUniform1i(UNIFORM_LOCATION("palette"), 0);

		glEnable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, glslRenderer->vramTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, glslRenderer->vram);

		GLuint location = glGetAttribLocation(glslRenderer->program, "vert");
		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, _vertices);
		int y;
		for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
			glUniform1f(UNIFORM_LOCATION("y"), y);
			glDrawArrays(GL_LINES, 0, 2);
		}
		glDisableVertexAttribArray(location);
		glFlush();
}

static void GBAVideoGLSLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;

	glslRenderer->state = GLSL_NONE;
	glslRenderer->y = 0;

	pthread_mutex_init(&glslRenderer->mutex, 0);
	pthread_cond_init(&glslRenderer->upCond, 0);
	pthread_cond_init(&glslRenderer->downCond, 0);
}

static void GBAVideoGLSLRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;

	/*glDeleteShader(glslRenderer->fragmentShader);
	glDeleteShader(glslRenderer->vertexShader);
	glDeleteProgram(glslRenderer->program);

	glDeleteTextures(1, &glslRenderer->paletteTexture);*/

	pthread_mutex_lock(&glslRenderer->mutex);
	pthread_cond_broadcast(&glslRenderer->upCond);
	pthread_mutex_unlock(&glslRenderer->mutex);

	pthread_mutex_destroy(&glslRenderer->mutex);
	pthread_cond_destroy(&glslRenderer->upCond);
	pthread_cond_destroy(&glslRenderer->downCond);
}

static void GBAVideoGLSLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;
	GLshort color = 0;
	color |= (value & 0x001F) << 11;
	color |= (value & 0x03E0) << 1;
	color |= (value & 0x7C00) >> 9;
	glslRenderer->vram[(address >> 1) + glslRenderer->y * 512] = color;
}

static uint16_t GBAVideoGLSLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;
	(void)(glslRenderer);
	(void)(address);

	return value;
}

static void GBAVideoGLSLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;

	glslRenderer->y = y + 1;
	if (y + 1 < VIDEO_VERTICAL_PIXELS) {
		memcpy(&glslRenderer->vram[(y + 1) * 512], &glslRenderer->vram[y * 512], 1024);
	} else {
		glslRenderer->y = 0;
		memcpy(&glslRenderer->vram[0], &glslRenderer->vram[y * 512], 1024);
	}
}

static void GBAVideoGLSLRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;

	pthread_mutex_lock(&glslRenderer->mutex);
	glslRenderer->state = GLSL_NONE;
	if (renderer->frameskip > 0) {
		--renderer->frameskip;
	} else {
		renderer->framesPending++;
		pthread_cond_broadcast(&glslRenderer->upCond);
		if (!renderer->turbo) {
			pthread_cond_wait(&glslRenderer->downCond, &glslRenderer->mutex);
		}
	}
	pthread_mutex_unlock(&glslRenderer->mutex);
}
