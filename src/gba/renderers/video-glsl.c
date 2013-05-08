#include "video-glsl.h"

#include "gba-io.h"

#include <string.h>

#define UNIFORM_LOCATION(UNIFORM) (glGetUniformLocation(glslRenderer->program, UNIFORM))

static const GLfloat _vertices[4] = {
	-1, 0,
	1, 0
};

static const GLchar* _fragmentShader =
	"#extension GL_EXT_gpu_shader4 : enable\n"
	"varying float x;\n"
	"uniform float y;\n"
	"uniform sampler2D vram;\n"
	"uniform int dispcnt;\n"
	"uniform int bg0cnt;\n"
	"uniform int bg1cnt;\n"
	"uniform int bg2cnt;\n"
	"uniform int bg3cnt;\n"
	"uniform int bg0hofs;\n"
	"uniform int bg0vofs;\n"
	"uniform int bg1hofs;\n"
	"uniform int bg1vofs;\n"
	"uniform int bg2hofs;\n"
	"uniform int bg2vofs;\n"
	"uniform int bg3hofs;\n"
	"uniform int bg3vofs;\n"
	"#define VRAM_INDEX(i) (vec2(mod(float(i + 1), 512.0) / 512.0 - 1.0 / 1024.0, (160.0 + floor(float(i) / 512.0)) / 255.0))\n"
	"#define DESERIALIZE(vec) int(dot(vec4(63488.0, 1984.0, 62.0, 1.0), vec))\n"

	"vec4 backgroundMode0(int bgcnt, int hofs, int vofs) {\n"
	"	int charBase = ((bgcnt / 4) & 3) * 8192;\n"
	"	int screenBase = ((bgcnt / 256) & 0x1F) * 1024;\n"
	"	int localX = hofs + int(x);\n"
	"	int localY = vofs + int(y);\n"
	"	int xBase = localX & 0xF8;\n"
	"	int yBase = localY & 0xF8;\n"
	"	screenBase = screenBase + (xBase / 8) + (yBase * 4);\n"
	"	int mapData = DESERIALIZE(texture2D(vram, VRAM_INDEX(screenBase)));\n"
	"	charBase = charBase + ((mapData & 0x3FF) * 16) + (localX & 0x4) / 4 + (localY & 0x7) * 2;\n"
	"	int tileData = DESERIALIZE(texture2D(vram, VRAM_INDEX(charBase)));\n"
	"	tileData = ((tileData >> ((localX & 3) * 4)) & 0xF);\n"
	"	if (tileData == 0) {\n"
	"		return vec4(0, 0, 0, 0);\n"
	"   }\n"
	"	return texture2D(vram, vec2(float(tileData + (mapData / 4096) * 16) / 512.0, y / 256.0));\n"
	"}\n"

	"void runPriority(int priority, inout vec4 color) {\n"
	"	if (color.a > 0.0) {\n"
	"		return;\n"
	"	}\n"
	"	if ((dispcnt & 0x100) != 0 && (bg0cnt & 0x3) == priority) {\n"
	"		color = backgroundMode0(bg0cnt, bg0hofs, bg0vofs);\n"
	"	}\n"
	"	if (color.a > 0.0) {\n"
	"		return;\n"
	"	}\n"
	"	if ((dispcnt & 0x200) != 0 && (bg1cnt & 0x3) == priority) {\n"
	"		color = backgroundMode0(bg1cnt, bg1hofs, bg1vofs);\n"
	"	}\n"
	"	if (color.a > 0.0) {\n"
	"		return;\n"
	"	}\n"
	"	if ((dispcnt & 0x400) != 0 && (bg2cnt & 0x3) == priority) {\n"
	"		color = backgroundMode0(bg2cnt, bg2hofs, bg2vofs);\n"
	"	}\n"
	"	if (color.a > 0.0) {\n"
	"		return;\n"
	"	}\n"
	"	if ((dispcnt & 0x800) != 0 && (bg3cnt & 0x3) == priority) {\n"
	"		color = backgroundMode0(bg3cnt, bg3hofs, bg3vofs);\n"
	"	}\n"
	"}\n"

	"void main() {\n"
	"	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);\n"
	"	runPriority(0, color);\n"
	"	runPriority(1, color);\n"
	"	runPriority(2, color);\n"
	"	runPriority(3, color);\n"
	"	if (color.a == 0.0) {\n"
	"		color = texture2D(vram, vec2(0.0, y / 256.0));\n"
	"	}\n"
	"	gl_FragColor = color;\n"
	"}\n";

static const GLchar* _vertexShader[] = {
	"varying float x;",
	"attribute vec2 vert;",
	"uniform float y;",

	"void main() {",
	"	x = vert.x * 120.0 + 120.0;",
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

	glShaderSource(glslRenderer->fragmentShader, 1, (const GLchar**) &_fragmentShader, 0);
	glShaderSource(glslRenderer->vertexShader, 7, _vertexShader, 0);

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
	glUniform1i(UNIFORM_LOCATION("vram"), 0);
	glUniform1i(UNIFORM_LOCATION("dispcnt"), glslRenderer->io[0][REG_DISPCNT >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg0cnt"), glslRenderer->io[0][REG_BG0CNT >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg1cnt"), glslRenderer->io[0][REG_BG1CNT >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg2cnt"), glslRenderer->io[0][REG_BG2CNT >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg3cnt"), glslRenderer->io[0][REG_BG3CNT >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg0hofs"), glslRenderer->io[0][REG_BG0HOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg0vofs"), glslRenderer->io[0][REG_BG0VOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg1hofs"), glslRenderer->io[0][REG_BG1HOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg1vofs"), glslRenderer->io[0][REG_BG1VOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg2hofs"), glslRenderer->io[0][REG_BG2HOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg2vofs"), glslRenderer->io[0][REG_BG2VOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg3hofs"), glslRenderer->io[0][REG_BG3HOFS >> 1]);
	glUniform1i(UNIFORM_LOCATION("bg3vofs"), glslRenderer->io[0][REG_BG3VOFS >> 1]);

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

	glslRenderer->oldVram = renderer->vram;
	renderer->vram = &glslRenderer->vram[512 * 160];

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

	renderer->vram = glslRenderer->oldVram;

	pthread_mutex_lock(&glslRenderer->mutex);
	pthread_cond_broadcast(&glslRenderer->upCond);
	pthread_mutex_unlock(&glslRenderer->mutex);

	pthread_mutex_destroy(&glslRenderer->mutex);
	pthread_cond_destroy(&glslRenderer->upCond);
	pthread_cond_destroy(&glslRenderer->downCond);
}

static void GBAVideoGLSLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;
	GLshort color = 1;
	color |= (value & 0x001F) << 11;
	color |= (value & 0x03E0) << 1;
	color |= (value & 0x7C00) >> 9;
	glslRenderer->vram[(address >> 1) + glslRenderer->y * 512] = color;
}

static uint16_t GBAVideoGLSLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;
	glslRenderer->io[glslRenderer->y][address >> 1] = value;

	return value;
}

static void GBAVideoGLSLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLSLRenderer* glslRenderer = (struct GBAVideoGLSLRenderer*) renderer;

	glslRenderer->y = y + 1;
	if (y + 1 < VIDEO_VERTICAL_PIXELS) {
		memcpy(&glslRenderer->vram[(y + 1) * 512], &glslRenderer->vram[y * 512], 1024);
		memcpy(glslRenderer->io[y + 1], glslRenderer->io[y], sizeof(*glslRenderer->io));
	} else {
		glslRenderer->y = 0;
		memcpy(&glslRenderer->vram[0], &glslRenderer->vram[y * 512], 1024);
		memcpy(glslRenderer->io[0], glslRenderer->io[y], sizeof(*glslRenderer->io));
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
