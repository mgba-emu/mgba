#include "video-glsl.h"

#include "gba-io.h"

#include <string.h>

#define UNIFORM_LOCATION(UNIFORM) (glGetUniformLocation(glslRenderer->program, UNIFORM))

static const GLfloat _vertices[4] = {
	-1, 0,
	1, 0
};

static const GLchar* _fragmentShader =
	"varying float x;\n"
	"uniform float y;\n"
	"uniform sampler2D vram;\n"
	"uniform float dispcnt;\n"
	"uniform float bg0cnt;\n"
	"uniform float bg1cnt;\n"
	"uniform float bg2cnt;\n"
	"uniform float bg3cnt;\n"
	"uniform float bg0hofs;\n"
	"uniform float bg0vofs;\n"
	"uniform float bg1hofs;\n"
	"uniform float bg1vofs;\n"
	"uniform float bg2hofs;\n"
	"uniform float bg2vofs;\n"
	"uniform float bg3hofs;\n"
	"uniform float bg3vofs;\n"
	"#define PALETTE_INDEX(i) texture2D(vram, (vec2(mod(i + 1.0, 512.0) / 512.0 - 1.0 / 1024.0, (floor(i / 512.0) + 1.0) / 256.0 - 1.0 / 1024.0)))\n"
	"#define VRAM_INDEX(i) texture2D(vram, (vec2(mod(i + 1.0, 512.0) / 512.0 - 1.0 / 1024.0, (floor(i / 512.0) + 161.0) / 256.0 - 1.0 / 1024.0)))\n"
	"#define DESERIALIZE(vec) dot(vec4(63488.0, 1984.0, 62.0, 1.0), vec)\n"
	"#define IMOD(a, b) mod(floor(a), b)\n"
	"#define BIT_CHECK(a, b) (IMOD(a / b, 2.0) > 0.0)\n"
	"#define DEBUG(fl) return vec4(fract(fl / 256.0), fract(floor(fl / 256.0) / 256.0), fract(floor(fl / 65536.0) / 256.0), 1.0)\n"

	"vec4 backgroundMode0(float bgcnt, float hofs, float vofs) {\n"
	"   float charBase = IMOD(bgcnt / 4.0, 4.0) * 8192.0;\n"
	"   float screenBase = IMOD(bgcnt / 256.0, 32.0) * 1024.0;\n"
	"   float size = IMOD(bgcnt / 16384.0, 4.0);\n"
	"   vec2 local = vec2(hofs + x, vofs + y);\n"
	"   vec2 base = IMOD(local, 256.0);\n"
	"	base -= IMOD(local, 8.0);\n"
	"   if (size == 1.0) {\n"
	"       base.x += IMOD(local.x / 256.0, 2.0) * 8192.0;\n"
	"   } else if (size == 2.0) {\n"
	"      	base.y += IMOD(local.y / 256.0, 2.0) * 256.0;\n"
	"   } else if (size == 3.0) {\n"
	"       base += IMOD(local / 256.0, 2.0) * vec2(8192.0, 512.0);\n"
	"   }\n"
	"   screenBase += dot(base, vec2(1.0 / 8.0, 4.0));\n"
	"   float mapData = DESERIALIZE(VRAM_INDEX(screenBase));\n"
	"   charBase += IMOD(mapData, 1024.0) * 16.0 + dot(IMOD(local * vec2(1.0 / 4.0, 1.0), vec2(2.0, 8.0)), vec2(1.0, 2.0));\n"
	"   float tileData = DESERIALIZE(VRAM_INDEX(charBase));\n"
	"   tileData *= pow(0.5, IMOD(local.x, 4.0) * 4.0);\n"
	"   tileData = IMOD(tileData, 16.0);\n"
	"   if (tileData == 0.0) {\n"
	"       return vec4(0, 0, 0, 0);\n"
	"   }\n"
	"   return PALETTE_INDEX(tileData + floor(mapData / 4096.0) * 16.0);\n"
	"}\n"

	"void runPriority(float priority, inout vec4 color) {\n"
	"   if (color.a > 0.0) {\n"
	"       return;\n"
	"   }\n"
	"   if (BIT_CHECK(dispcnt, 256.0) && IMOD(bg0cnt, 4.0) == priority) {\n"
	"       color = backgroundMode0(bg0cnt, bg0hofs, bg0vofs);\n"
	"   }\n"
	"   if (color.a > 0.0) {\n"
	"       return;\n"
	"   }\n"
	"   if (BIT_CHECK(dispcnt, 512.0) && IMOD(bg1cnt, 4.0) == priority) {\n"
	"       color = backgroundMode0(bg1cnt, bg1hofs, bg1vofs);\n"
	"   }\n"
	"   if (color.a > 0.0) {\n"
	"       return;\n"
	"   }\n"
	"   if (BIT_CHECK(dispcnt, 1024.0) && IMOD(bg2cnt, 4.0) == priority) {\n"
	"       color = backgroundMode0(bg2cnt, bg2hofs, bg2vofs);\n"
	"   }\n"
	"   if (color.a > 0.0) {\n"
	"		return;\n"
	"	}\n"
	"   if (BIT_CHECK(dispcnt, 2048.0) && IMOD(bg3cnt, 4.0) == priority) {\n"
	"		color = backgroundMode0(bg3cnt, bg3hofs, bg3vofs);\n"
	"	}\n"
	"}\n"

	"void main() {\n"
	"	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);\n"
	"	runPriority(0.0, color);\n"
	"	runPriority(1.0, color);\n"
	"	runPriority(2.0, color);\n"
	"	runPriority(3.0, color);\n"
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
	"	gl_Position = vec4(vert.x, 1.0 - (y + 1.0) / 80.0, 0, 1.0);",
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
	glUniform1f(UNIFORM_LOCATION("dispcnt"), glslRenderer->io[0][REG_DISPCNT >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg0cnt"), glslRenderer->io[0][REG_BG0CNT >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg1cnt"), glslRenderer->io[0][REG_BG1CNT >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg2cnt"), glslRenderer->io[0][REG_BG2CNT >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg3cnt"), glslRenderer->io[0][REG_BG3CNT >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg0hofs"), glslRenderer->io[0][REG_BG0HOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg0vofs"), glslRenderer->io[0][REG_BG0VOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg1hofs"), glslRenderer->io[0][REG_BG1HOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg1vofs"), glslRenderer->io[0][REG_BG1VOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg2hofs"), glslRenderer->io[0][REG_BG2HOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg2vofs"), glslRenderer->io[0][REG_BG2VOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg3hofs"), glslRenderer->io[0][REG_BG3HOFS >> 1]);
	glUniform1f(UNIFORM_LOCATION("bg3vofs"), glslRenderer->io[0][REG_BG3VOFS >> 1]);

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
