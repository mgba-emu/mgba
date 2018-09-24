/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/png-io.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <GLES3/gl3.h>

#define GLYPH_HEIGHT 24
#define CELL_HEIGHT 32
#define CELL_WIDTH 32

static const GLfloat _offsets[] = {
	0.f, 0.f,
	1.f, 0.f,
	1.f, 1.f,
	0.f, 1.f,
};

static const GLchar* const _gles2Header =
	"#version 100\n"
	"precision mediump float;\n";

static const char* const _vertexShader =
	"attribute vec2 offset;\n"
	"uniform vec3 origin;\n"
	"uniform vec2 glyph;\n"
	"uniform vec2 dims;\n"
	"uniform mat2 transform;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	texCoord = (glyph + offset * dims) / 512.0;\n"
	"	vec2 scaledOffset = (transform * (offset * 2.0 - vec2(1.0)) + vec2(1.0)) / 2.0 * dims;\n"
	"	gl_Position = vec4((origin.x + scaledOffset.x) / 640.0 - 1.0, -(origin.y + scaledOffset.y) / 360.0 + 1.0, origin.z, 1.0);\n"
	"}";

static const char* const _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"
	"uniform vec4 color;\n"
	"uniform float cutoff;\n"

	"void main() {\n"
	"	vec4 texColor = texture2D(tex, texCoord);\n"
	"	texColor.a = clamp((texColor.a - cutoff) / (1.0 - cutoff), 0.0, 1.0);\n"
	"	texColor.rgb = color.rgb;\n"
	"	texColor.a *= color.a;\n"
	"	gl_FragColor = texColor;\n"
	"}";

struct GUIFont {
	GLuint font;
	GLuint program;
	GLuint vbo;
	GLuint vao;
	GLuint texLocation;
	GLuint dimsLocation;
	GLuint transformLocation;
	GLuint colorLocation;
	GLuint originLocation;
	GLuint glyphLocation;
	GLuint cutoffLocation;
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (!_loadTexture("romfs:/font-new.png")) {
		GUIFontDestroy(font);
		return NULL;
	}

	font->program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* shaderBuffer[2];

	shaderBuffer[0] = _gles2Header;

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

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	font->texLocation = glGetUniformLocation(font->program, "tex");
	font->colorLocation = glGetUniformLocation(font->program, "color");
	font->dimsLocation = glGetUniformLocation(font->program, "dims");
	font->transformLocation = glGetUniformLocation(font->program, "transform");
	font->originLocation = glGetUniformLocation(font->program, "origin");
	font->glyphLocation = glGetUniformLocation(font->program, "glyph");
	font->cutoffLocation = glGetUniformLocation(font->program, "cutoff");
	GLuint offsetLocation = glGetAttribLocation(font->program, "offset");

	glGenBuffers(1, &font->vbo);
	glGenVertexArrays(1, &font->vao);
	glBindVertexArray(font->vao);
	glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_offsets), _offsets, GL_STATIC_DRAW);
	glVertexAttribPointer(offsetLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(offsetLocation);
	glBindVertexArray(0);

	return font;
}

void GUIFontDestroy(struct GUIFont* font) {
	glDeleteBuffers(1, &font->vbo);
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

void GUIFontDrawGlyph(const struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	if (glyph > 0x7F) {
		glyph = '?';
	}
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];

	glUseProgram(font->program);
	glBindVertexArray(font->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->font);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform1i(font->texLocation, 0);
	glUniform2f(font->glyphLocation, (glyph & 15) * CELL_WIDTH + metric.padding.left * 2, (glyph >> 4) * CELL_HEIGHT + metric.padding.top * 2);
	glUniform2f(font->dimsLocation, CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2, CELL_HEIGHT - (metric.padding.top + metric.padding.bottom) * 2);
	glUniform3f(font->originLocation, x, y - GLYPH_HEIGHT + metric.padding.top * 2, 0);
	glUniformMatrix2fv(font->transformLocation, 1, GL_FALSE, (float[4]) {1.0, 0.0, 0.0, 1.0});

	glUniform1f(font->cutoffLocation, 0.1f);
	glUniform4f(font->colorLocation, 0.0, 0.0, 0.0, ((color >> 24) & 0xFF) / 128.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUniform1f(font->cutoffLocation, 0.7f);
	glUniform4f(font->colorLocation, (color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindVertexArray(0);
	glUseProgram(0);
}

void GUIFontDrawIcon(const struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
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

	glUseProgram(font->program);
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

	glUseProgram(font->program);
	glBindVertexArray(font->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->font);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform1i(font->texLocation, 0);
	glUniform2f(font->glyphLocation, metric.x * 2, metric.y * 2 + 256);
	glUniform2f(font->dimsLocation, metric.width * 2, metric.height * 2);
	glUniform3f(font->originLocation, x, y, 0);
	glUniformMatrix2fv(font->transformLocation, 1, GL_FALSE, (float[4]) {hFlip, 0.0, 0.0, vFlip});

	glUniform1f(font->cutoffLocation, 0.1f);
	glUniform4f(font->colorLocation, 0.0, 0.0, 0.0, ((color >> 24) & 0xFF) / 128.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUniform1f(font->cutoffLocation, 0.7f);
	glUniform4f(font->colorLocation, (color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindVertexArray(0);
	glUseProgram(0);
}

void GUIFontDrawIconSize(const struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
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

	glUseProgram(font->program);
	glBindVertexArray(font->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font->font);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform1i(font->texLocation, 0);
	glUniform2f(font->glyphLocation, metric.x * 2, metric.y * 2 + 256);
	glUniform2f(font->dimsLocation, metric.width * 2, metric.height * 2);
	glUniform3f(font->originLocation, x + w / 2 - metric.width, y + h / 2 - metric.height, 0);
	glUniformMatrix2fv(font->transformLocation, 1, GL_FALSE, (float[4]) {w * 0.5f / metric.width, 0.0, 0.0, h * 0.5f / metric.height});

	glUniform1f(font->cutoffLocation, 0.1f);
	glUniform4f(font->colorLocation, 0.0, 0.0, 0.0, ((color >> 24) & 0xFF) / 128.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUniform1f(font->cutoffLocation, 0.7f);
	glUniform4f(font->colorLocation, ((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, (color & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindVertexArray(0);
	glUseProgram(0);
}
