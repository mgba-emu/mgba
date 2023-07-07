/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <GLES3/gl3.h>

#define GLYPH_HEIGHT 24
#define CELL_HEIGHT 32
#define CELL_WIDTH 32
#define MAX_GLYPHS 1024

static const GLfloat _offsets[] = {
	0.f, 0.f,
	1.f, 0.f,
	1.f, 1.f,
	0.f, 1.f,
};

static const GLchar* const _gles3Header =
	"#version 300 es\n"
	"precision mediump float;\n";

static const char* const _vertexShader =
	"in vec2 offset;\n"
	"in vec3 origin;\n"
	"in vec2 glyph;\n"
	"in vec2 dims;\n"
	"in mat2 transform;\n"
	"in vec4 color;\n"
	"out vec4 fragColor;\n"
	"out vec2 texCoord;\n"

	"void main() {\n"
	"	texCoord = (glyph + offset * dims) / 512.0;\n"
	"	vec2 scaledOffset = (transform * (offset * 2.0 - vec2(1.0)) + vec2(1.0)) / 2.0 * dims;\n"
	"	fragColor = color;\n"
	"	gl_Position = vec4((origin.x + scaledOffset.x) / 640.0 - 1.0, -(origin.y + scaledOffset.y) / 360.0 + 1.0, origin.z, 1.0);\n"
	"}";

static const char* const _fragmentShader =
	"in vec2 texCoord;\n"
	"in vec4 fragColor;\n"
	"out vec4 outColor;\n"
	"uniform sampler2D tex;\n"
	"uniform float cutoff;\n"
	"uniform vec3 colorModulus;\n"

	"void main() {\n"
	"	vec4 texColor = texture2D(tex, texCoord);\n"
	"	texColor.a = clamp((texColor.a - cutoff) / (1.0 - cutoff), 0.0, 1.0);\n"
	"	texColor.rgb = fragColor.rgb * colorModulus;\n"
	"	texColor.a *= fragColor.a;\n"
	"	outColor = texColor;\n"
	"}";

struct GUIFont {
	GLuint font;
	int currentGlyph;
	GLuint program;
	GLuint vbo;
	GLuint vao;
	GLuint texLocation;
	GLuint cutoffLocation;
	GLuint colorModulusLocation;

	GLuint originLocation;
	GLuint glyphLocation;
	GLuint dimsLocation;
	GLuint transformLocation[2];
	GLuint colorLocation;

	GLuint originVbo;
	GLuint glyphVbo;
	GLuint dimsVbo;
	GLuint transformVbo[2];
	GLuint colorVbo;

	GLfloat originData[MAX_GLYPHS][3];
	GLfloat glyphData[MAX_GLYPHS][2];
	GLfloat dimsData[MAX_GLYPHS][2];
	GLfloat transformData[2][MAX_GLYPHS][2];
	GLfloat colorData[MAX_GLYPHS][4];
};

static bool _loadTexture(const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	png_structp png = PNGReadOpen(vf, 0);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	bool success = false;
	if (png && info && end) {
		success = PNGReadHeader(png, info);
	}
	void* pixels = NULL;
	if (success) {
		unsigned height = png_get_image_height(png, info);
		unsigned width = png_get_image_width(png, info);
		pixels = malloc(width * height);
		if (pixels) {
			success = PNGReadPixels8(png, info, pixels, width, height, width);
			success = success && PNGReadFooter(png, end);
		} else {
			success = false;
		}
		if (success) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels);
		}
	}
	PNGReadClose(png, info, end);
	if (pixels) {
		free(pixels);
	}
	vf->close(vf);
	return success;
}

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* font = malloc(sizeof(struct GUIFont));
	if (!font) {
		return NULL;
	}
	glGenTextures(1, &font->font);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->font);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (!_loadTexture("romfs:/font-new.png")) {
		GUIFontDestroy(font);
		return NULL;
	}

	font->currentGlyph = 0;
	font->program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* shaderBuffer[2];

	shaderBuffer[0] = _gles3Header;

	shaderBuffer[1] = _vertexShader;
	glShaderSource(vertexShader, 2, shaderBuffer, NULL);

	shaderBuffer[1] = _fragmentShader;
	glShaderSource(fragmentShader, 2, shaderBuffer, NULL);

	glAttachShader(font->program, vertexShader);
	glAttachShader(font->program, fragmentShader);

	glCompileShader(fragmentShader);

	GLint success;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar msg[512];
		glGetShaderInfoLog(fragmentShader, sizeof(msg), NULL, msg);
		puts(msg);
	}

	glCompileShader(vertexShader);

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar msg[512];
		glGetShaderInfoLog(vertexShader, sizeof(msg), NULL, msg);
		puts(msg);
	}

	glLinkProgram(font->program);

	glGetProgramiv(font->program, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar msg[512];
		glGetProgramInfoLog(font->program, sizeof(msg), NULL, msg);
		puts(msg);
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	font->texLocation = glGetUniformLocation(font->program, "tex");
	font->cutoffLocation = glGetUniformLocation(font->program, "cutoff");
	font->colorModulusLocation = glGetUniformLocation(font->program, "colorModulus");

	font->originLocation = glGetAttribLocation(font->program, "origin");
	font->glyphLocation = glGetAttribLocation(font->program, "glyph");
	font->dimsLocation = glGetAttribLocation(font->program, "dims");
	font->transformLocation[0] = glGetAttribLocation(font->program, "transform");
	font->transformLocation[1] = font->transformLocation[0] + 1;
	font->colorLocation = glGetAttribLocation(font->program, "color");

	GLuint offsetLocation = glGetAttribLocation(font->program, "offset");

	glGenVertexArrays(1, &font->vao);
	glBindVertexArray(font->vao);

	glGenBuffers(1, &font->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_offsets), _offsets, GL_STATIC_DRAW);
	glVertexAttribPointer(offsetLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(offsetLocation, 0);
	glEnableVertexAttribArray(offsetLocation);

	glGenBuffers(1, &font->originVbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->originVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->originLocation, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->originLocation, 1);
	glEnableVertexAttribArray(font->originLocation);

	glGenBuffers(1, &font->glyphVbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->glyphVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->glyphLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->glyphLocation, 1);
	glEnableVertexAttribArray(font->glyphLocation);

	glGenBuffers(1, &font->dimsVbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->dimsVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->dimsLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->dimsLocation, 1);
	glEnableVertexAttribArray(font->dimsLocation);

	glGenBuffers(2, font->transformVbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->transformVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->transformLocation[0], 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->transformLocation[0], 1);
	glEnableVertexAttribArray(font->transformLocation[0]);
	glBindBuffer(GL_ARRAY_BUFFER, font->transformVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->transformLocation[1], 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->transformLocation[1], 1);
	glEnableVertexAttribArray(font->transformLocation[1]);

	glGenBuffers(1, &font->colorVbo);
	glBindBuffer(GL_ARRAY_BUFFER, font->colorVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glVertexAttribPointer(font->colorLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glVertexAttribDivisor(font->colorLocation, 1);
	glEnableVertexAttribArray(font->colorLocation);

	glBindVertexArray(0);

	return font;
}

void GUIFontDestroy(struct GUIFont* font) {
	glDeleteBuffers(1, &font->vbo);
	glDeleteBuffers(1, &font->originVbo);
	glDeleteBuffers(1, &font->glyphVbo);
	glDeleteBuffers(1, &font->dimsVbo);
	glDeleteBuffers(2, font->transformVbo);
	glDeleteBuffers(1, &font->colorVbo);
	glDeleteProgram(font->program);
	glDeleteTextures(1, &font->font);
	glDeleteVertexArrays(1, &font->vao);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	UNUSED(font);
	return GLYPH_HEIGHT;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	UNUSED(font);
	if (glyph > 0x7F) {
		glyph = '?';
	}
	return defaultFontMetrics[glyph].width * 2;
}

void GUIFontIconMetrics(const struct GUIFont* font, enum GUIIcon icon, unsigned* w, unsigned* h) {
	UNUSED(font);
	if (icon >= GUI_ICON_MAX) {
		if (w) {
			*w = 0;
		}
		if (h) {
			*h = 0;
		}
	} else {
		if (w) {
			*w = defaultIconMetrics[icon].width * 2;
		}
		if (h) {
			*h = defaultIconMetrics[icon].height * 2;
		}
	}
}

void GUIFontDrawGlyph(struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	if (glyph > 0x7F) {
		glyph = '?';
	}
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];

	if (font->currentGlyph >= MAX_GLYPHS) {
		GUIFontDrawSubmit(font);
	}

	int offset = font->currentGlyph;

	font->originData[offset][0] = x;
	font->originData[offset][1] = y - GLYPH_HEIGHT + metric.padding.top * 2;
	font->originData[offset][2] = 0;
	font->glyphData[offset][0] = (glyph & 15) * CELL_WIDTH + metric.padding.left * 2;
	font->glyphData[offset][1] = (glyph >> 4) * CELL_HEIGHT + metric.padding.top * 2;
	font->dimsData[offset][0] = CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2;
	font->dimsData[offset][1] = CELL_HEIGHT - (metric.padding.top + metric.padding.bottom) * 2;
	font->transformData[0][offset][0] = 1.0f;
	font->transformData[0][offset][1] = 0.0f;
	font->transformData[1][offset][0] = 0.0f;
	font->transformData[1][offset][1] = 1.0f;
	font->colorData[offset][0] = (color & 0xFF) / 255.0f;
	font->colorData[offset][1] = ((color >> 8) & 0xFF) / 255.0f;
	font->colorData[offset][2] = ((color >> 16) & 0xFF) / 255.0f;
	font->colorData[offset][3] = ((color >> 24) & 0xFF) / 255.0f;

	++font->currentGlyph;
}

void GUIFontDrawIcon(struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];

	float hFlip = 1.0f;
	float vFlip = 1.0f;
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= metric.width;
		break;
	case GUI_ALIGN_RIGHT:
		x -= metric.width * 2;
		break;
	}
	switch (align & GUI_ALIGN_VCENTER) {
	case GUI_ALIGN_VCENTER:
		y -= metric.height;
		break;
	case GUI_ALIGN_BOTTOM:
		y -= metric.height * 2;
		break;
	}

	switch (orient) {
	case GUI_ORIENT_HMIRROR:
		hFlip = -1.0;
		break;
	case GUI_ORIENT_VMIRROR:
		vFlip = -1.0;
		break;
	case GUI_ORIENT_0:
	default:
		// TODO: Rotate
		break;
	}
	if (font->currentGlyph >= MAX_GLYPHS) {
		GUIFontDrawSubmit(font);
	}

	int offset = font->currentGlyph;

	font->originData[offset][0] = x;
	font->originData[offset][1] = y;
	font->originData[offset][2] = 0;
	font->glyphData[offset][0] = metric.x * 2;
	font->glyphData[offset][1] = metric.y * 2 + 256;
	font->dimsData[offset][0] = metric.width * 2;
	font->dimsData[offset][1] = metric.height * 2;
	font->transformData[0][offset][0] = hFlip;
	font->transformData[0][offset][1] = 0.0f;
	font->transformData[1][offset][0] = 0.0f;
	font->transformData[1][offset][1] = vFlip;
	font->colorData[offset][0] = (color & 0xFF) / 255.0f;
	font->colorData[offset][1] = ((color >> 8) & 0xFF) / 255.0f;
	font->colorData[offset][2] = ((color >> 16) & 0xFF) / 255.0f;
	font->colorData[offset][3] = ((color >> 24) & 0xFF) / 255.0f;

	++font->currentGlyph;
}

void GUIFontDrawIconSize(struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];

	if (!w) {
		w = metric.width * 2;
	}
	if (!h) {
		h = metric.height * 2;
	}

	if (font->currentGlyph >= MAX_GLYPHS) {
		GUIFontDrawSubmit(font);
	}

	int offset = font->currentGlyph;

	font->originData[offset][0] = x + w / 2 - metric.width;
	font->originData[offset][1] = y + h / 2 - metric.height;
	font->originData[offset][2] = 0;
	font->glyphData[offset][0] = metric.x * 2;
	font->glyphData[offset][1] = metric.y * 2 + 256;
	font->dimsData[offset][0] = metric.width * 2;
	font->dimsData[offset][1] = metric.height * 2;
	font->transformData[0][offset][0] = w * 0.5f / metric.width;
	font->transformData[0][offset][1] = 0.0f;
	font->transformData[1][offset][0] = 0.0f;
	font->transformData[1][offset][1] = h * 0.5f / metric.height;
	font->colorData[offset][0] = (color & 0xFF) / 255.0f;
	font->colorData[offset][1] = ((color >> 8) & 0xFF) / 255.0f;
	font->colorData[offset][2] = ((color >> 16) & 0xFF) / 255.0f;
	font->colorData[offset][3] = ((color >> 24) & 0xFF) / 255.0f;

	++font->currentGlyph;
}

void GUIFontDrawSubmit(struct GUIFont* font) {
	glUseProgram(font->program);
	glBindVertexArray(font->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->font);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform1i(font->texLocation, 0);

	glBindBuffer(GL_ARRAY_BUFFER, font->originVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 3 * font->currentGlyph, font->originData);

	glBindBuffer(GL_ARRAY_BUFFER, font->glyphVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 2 * font->currentGlyph, font->glyphData);

	glBindBuffer(GL_ARRAY_BUFFER, font->dimsVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 2 * font->currentGlyph, font->dimsData);

	glBindBuffer(GL_ARRAY_BUFFER, font->transformVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 2 * font->currentGlyph, font->transformData[0]);

	glBindBuffer(GL_ARRAY_BUFFER, font->transformVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 2 * font->currentGlyph, font->transformData[1]);

	glBindBuffer(GL_ARRAY_BUFFER, font->colorVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * MAX_GLYPHS, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 4 * font->currentGlyph, font->colorData);

	glUniform1f(font->cutoffLocation, 0.1f);
	glUniform3f(font->colorModulusLocation, 0.f, 0.f, 0.f);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, font->currentGlyph);

	glUniform1f(font->cutoffLocation, 0.7f);
	glUniform3f(font->colorModulusLocation, 1.f, 1.f, 1.f);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, font->currentGlyph);

	font->currentGlyph = 0;

	glBindVertexArray(0);
	glUseProgram(0);
}
