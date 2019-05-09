/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/gl.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>

static void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoGLRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static void GBAVideoGLRendererUpdateDISPCNT(struct GBAVideoGLRenderer* renderer);
static void GBAVideoGLRendererWriteBGCNT(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGX_LO(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGX_HI(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGY_LO(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGY_HI(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBLDCNT(struct GBAVideoGLRenderer* renderer, uint16_t value);

static const GLchar* const _gl3Header =
	"#version 130\n";

static const char* const _vertexShader =
	"attribute vec2 position;\n"
	"uniform int y;\n"
	"const ivec2 maxPos = ivec2(240, 160);\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	vec2 local = (position + vec2(0, y)) / vec2(1., maxPos.y);\n"
	"	gl_Position = vec4(local * 2. - 1., 0., 1.);\n"
	"	texCoord = local * maxPos;\n"
	"}";

static const char* const _renderTile16 =
	"vec4 renderTile(int tile, int tileBase, int paletteId, ivec2 localCoord) {\n"
	"	int address = tileBase + tile * 16 + (localCoord.x >> 2) + (localCoord.y << 1);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - (localCoord.x & 3)] * 16.);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, paletteId), 0);\n"
	"	if (entry > 0) {\n"
	"		color.a = 1.;\n"
	"	} else {\n"
	"		color.a = 0.;\n"
	"	}\n"
	"	return color;\n"
	"}";

static const char* const _renderMode0 =
	"varying vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform ivec2 offset;\n"

	"vec4 renderTile(int tile, int tileBase, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(texCoord) + offset;\n"
	"	int mapAddress = screenBase + (coord.x >> 3) + (coord.y >> 3) * 32;\n"
	"	vec4 map = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0);\n"
	"	int flags = int(map.g * 15.9);\n"
	"	if ((flags & 4) == 4) {\n"
	"		coord.x ^= 7;\n"
	"	}\n"
	"	if ((flags & 8) == 8) {\n"
	"		coord.y ^= 7;\n"
	"	}\n"
	"	int tile = int(map.a * 15.9) + int(map.b * 15.9) * 16 + (flags & 0x3) * 256;\n"
	"	gl_FragColor = renderTile(tile, charBase, int(map.r * 15.9), coord & 7);\n"
	"}";

static const GLint _vertices[] = {
	0, 0,
	0, 1,
	1, 1,
	1, 0,
};

void GBAVideoGLRendererCreate(struct GBAVideoGLRenderer* renderer) {
	renderer->d.init = GBAVideoGLRendererInit;
	renderer->d.reset = GBAVideoGLRendererReset;
	renderer->d.deinit = GBAVideoGLRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoGLRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBAVideoGLRendererWriteVRAM;
	renderer->d.writeOAM = GBAVideoGLRendererWriteOAM;
	renderer->d.writePalette = GBAVideoGLRendererWritePalette;
	renderer->d.drawScanline = GBAVideoGLRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoGLRendererFinishFrame;
	renderer->d.getPixels = GBAVideoGLRendererGetPixels;
	renderer->d.putPixels = GBAVideoGLRendererPutPixels;

	renderer->d.disableBG[0] = false;
	renderer->d.disableBG[1] = false;
	renderer->d.disableBG[2] = false;
	renderer->d.disableBG[3] = false;
	renderer->d.disableOBJ = false;
}

void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glGenFramebuffers(2, glRenderer->fbo);
	glGenTextures(2, glRenderer->layers);

	glGenTextures(1, &glRenderer->paletteTex);

	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &glRenderer->vramTex);

	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GBAVideoGLRendererReset(renderer);
}

void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glDeleteFramebuffers(2, glRenderer->fbo);
	glDeleteTextures(2, glRenderer->layers);
	glDeleteTextures(1, &glRenderer->paletteTex);
	glDeleteTextures(1, &glRenderer->vramTex);
	glDeleteTextures(1, &glRenderer->oamTex);
}

void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[1]);
	glBindTexture(GL_TEXTURE_2D, glRenderer->layers[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glRenderer->layers[1], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glRenderer->compositeProgram = glCreateProgram();
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

	const GLchar* shaderBuffer[3];
	const GLubyte* version = glGetString(GL_VERSION);
	shaderBuffer[0] = _gl3Header;
	shaderBuffer[1] = _vertexShader;
	glShaderSource(vs, 2, shaderBuffer, 0);
	shaderBuffer[1] = _renderMode0;
	shaderBuffer[2] = _renderTile16;
	glShaderSource(fs, 3, shaderBuffer, 0);

	glAttachShader(glRenderer->compositeProgram, vs);
	glAttachShader(glRenderer->compositeProgram, fs);
	char log[1024];

	glCompileShader(fs);
	glGetShaderInfoLog(fs, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Fragment shader compilation failure: %s", log);
	}

	glCompileShader(vs);
	glGetShaderInfoLog(vs, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Vertex shader compilation failure: %s", log);
	}

	glLinkProgram(glRenderer->compositeProgram);
	glGetProgramInfoLog(glRenderer->compositeProgram, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Program link failure: %s", log);
	}

	glRenderer->paletteDirty = true;
	glRenderer->vramDirty = 0xFFFFFF;

	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);
}

void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->vramDirty |= 1 << (address >> 12);
}

void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	UNUSED(oam);
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->oamDirty = true;
}

void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(address);
	UNUSED(value);
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->paletteDirty = true;
}

uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}

	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFF7;
		glRenderer->dispcnt = value;
		//GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG0CNT:
		value &= 0xDFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[0], value);
		//GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG1CNT:
		value &= 0xDFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[1], value);
		//GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG2CNT:
		value &= 0xFFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[2], value);
		//GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG3CNT:
		value &= 0xFFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[3], value);
		//GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG0HOFS:
		value &= 0x01FF;
		glRenderer->bg[0].x = value;
		break;
	case REG_BG0VOFS:
		value &= 0x01FF;
		glRenderer->bg[0].y = value;
		break;
	case REG_BG1HOFS:
		value &= 0x01FF;
		glRenderer->bg[1].x = value;
		break;
	case REG_BG1VOFS:
		value &= 0x01FF;
		glRenderer->bg[1].y = value;
		break;
	case REG_BG2HOFS:
		value &= 0x01FF;
		glRenderer->bg[2].x = value;
		break;
	case REG_BG2VOFS:
		value &= 0x01FF;
		glRenderer->bg[2].y = value;
		break;
	case REG_BG3HOFS:
		value &= 0x01FF;
		glRenderer->bg[3].x = value;
		break;
	case REG_BG3VOFS:
		value &= 0x01FF;
		glRenderer->bg[3].y = value;
		break;
	case REG_BG2PA:
		glRenderer->bg[2].dx = value;
		break;
	case REG_BG2PB:
		glRenderer->bg[2].dmx = value;
		break;
	case REG_BG2PC:
		glRenderer->bg[2].dy = value;
		break;
	case REG_BG2PD:
		glRenderer->bg[2].dmy = value;
		break;
	case REG_BG2X_LO:
		GBAVideoGLRendererWriteBGX_LO(&glRenderer->bg[2], value);
		break;
	case REG_BG2X_HI:
		GBAVideoGLRendererWriteBGX_HI(&glRenderer->bg[2], value);
		break;
	case REG_BG2Y_LO:
		GBAVideoGLRendererWriteBGY_LO(&glRenderer->bg[2], value);
		break;
	case REG_BG2Y_HI:
		GBAVideoGLRendererWriteBGY_HI(&glRenderer->bg[2], value);
		break;
	case REG_BG3PA:
		glRenderer->bg[3].dx = value;
		break;
	case REG_BG3PB:
		glRenderer->bg[3].dmx = value;
		break;
	case REG_BG3PC:
		glRenderer->bg[3].dy = value;
		break;
	case REG_BG3PD:
		glRenderer->bg[3].dmy = value;
		break;
	case REG_BG3X_LO:
		GBAVideoGLRendererWriteBGX_LO(&glRenderer->bg[3], value);
		break;
	case REG_BG3X_HI:
		GBAVideoGLRendererWriteBGX_HI(&glRenderer->bg[3], value);
		break;
	case REG_BG3Y_LO:
		GBAVideoGLRendererWriteBGY_LO(&glRenderer->bg[3], value);
		break;
	case REG_BG3Y_HI:
		GBAVideoGLRendererWriteBGY_HI(&glRenderer->bg[3], value);
		break;
	case REG_BLDCNT:
		GBAVideoGLRendererWriteBLDCNT(glRenderer, value);
		value &= 0x3FFF;
		break;
	case REG_BLDALPHA:
		glRenderer->blda = value & 0x1F;
		if (glRenderer->blda > 0x10) {
			glRenderer->blda = 0x10;
		}
		glRenderer->bldb = (value >> 8) & 0x1F;
		if (glRenderer->bldb > 0x10) {
			glRenderer->bldb = 0x10;
		}
		value &= 0x1F1F;
		break;
	case REG_BLDY:
		value &= 0x1F;
		if (value > 0x10) {
			value = 0x10;
		}
		if (glRenderer->bldy != value) {
			glRenderer->bldy = value;
		}
		break;
	case REG_WIN0H:
		/*glRenderer->winN[0].h.end = value;
		glRenderer->winN[0].h.start = value >> 8;
		if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[0].h.start > glRenderer->winN[0].h.end) {
			glRenderer->winN[0].h.start = 0;
		}
		if (glRenderer->winN[0].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[0].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[0].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN1H:
		/*glRenderer->winN[1].h.end = value;
		glRenderer->winN[1].h.start = value >> 8;
		if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[1].h.start > glRenderer->winN[1].h.end) {
			glRenderer->winN[1].h.start = 0;
		}
		if (glRenderer->winN[1].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[1].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[1].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN0V:
		/*glRenderer->winN[0].v.end = value;
		glRenderer->winN[0].v.start = value >> 8;
		if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[0].v.start > glRenderer->winN[0].v.end) {
			glRenderer->winN[0].v.start = 0;
		}
		if (glRenderer->winN[0].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[0].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[0].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN1V:
		/*glRenderer->winN[1].v.end = value;
		glRenderer->winN[1].v.start = value >> 8;
		if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[1].v.start > glRenderer->winN[1].v.end) {
			glRenderer->winN[1].v.start = 0;
		}
		if (glRenderer->winN[1].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[1].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[1].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}*/
		break;
	case REG_WININ:
		value &= 0x3F3F;
		//glRenderer->winN[0].control.packed = value;
		//glRenderer->winN[1].control.packed = value >> 8;
		break;
	case REG_WINOUT:
		value &= 0x3F3F;
		//glRenderer->winout.packed = value;
		//glRenderer->objwin.packed = value >> 8;
		break;
	case REG_MOSAIC:
		glRenderer->mosaic = value;
		break;
	case REG_GREENSWP:
		mLOG(GBA_VIDEO, STUB, "Stub video register write: 0x%03X", address);
		break;
	default:
		mLOG(GBA_VIDEO, GAME_ERROR, "Invalid video register: 0x%03X", address);
	}
	return value;
}

void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (glRenderer->paletteDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 16, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, glRenderer->d.palette);
		glRenderer->paletteDirty = false;
	}
	if (glRenderer->oamDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->oamTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 4, 128, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, glRenderer->d.oam);
		glRenderer->oamDirty = false;
	}
	int i;
	for (i = 0; i < 24; ++i) {
		if (!(glRenderer->vramDirty & (1 << i))) {
			continue;
		}
		// TODO: PBOs
		glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 8 * i, 256, 8, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &glRenderer->d.vram[2048 * i]);
	}
	glRenderer->vramDirty = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[1]);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUseProgram(glRenderer->compositeProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glUniform1i(0, y);
	glUniform1i(1, 0);
	glUniform1i(2, 1);
	glUniform1i(3, glRenderer->bg[0].screenBase);
	glUniform1i(4, glRenderer->bg[0].charBase);
	glUniform2i(5, glRenderer->bg[0].x, glRenderer->bg[0].y);
	glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glFinish();
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[1]);
	glPixelStorei(GL_PACK_ROW_LENGTH, glRenderer->outputBufferStride);
	glReadPixels(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, GL_RGBA, GL_UNSIGNED_BYTE, glRenderer->outputBuffer);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {

}

void GBAVideoGLRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {

}

static void GBAVideoGLRendererWriteBGCNT(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->priority = GBARegisterBGCNTGetPriority(value);
	bg->charBase = GBARegisterBGCNTGetCharBase(value) << 13;
	bg->mosaic = GBARegisterBGCNTGetMosaic(value);
	bg->multipalette = GBARegisterBGCNTGet256Color(value);
	bg->screenBase = GBARegisterBGCNTGetScreenBase(value) << 10;
	bg->overflow = GBARegisterBGCNTGetOverflow(value);
	bg->size = GBARegisterBGCNTGetSize(value);
}

static void GBAVideoGLRendererWriteBGX_LO(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0xFFFF0000) | value;
	bg->sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGX_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0x0000FFFF) | (value << 16);
	bg->refx <<= 4;
	bg->refx >>= 4;
	bg->sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGY_LO(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0xFFFF0000) | value;
	bg->sy = bg->refy;
}

static void GBAVideoGLRendererWriteBGY_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0x0000FFFF) | (value << 16);
	bg->refy <<= 4;
	bg->refy >>= 4;
	bg->sy = bg->refy;
}

static void GBAVideoGLRendererWriteBLDCNT(struct GBAVideoGLRenderer* renderer, uint16_t value) {
	renderer->bg[0].target1 = GBARegisterBLDCNTGetTarget1Bg0(value);
	renderer->bg[1].target1 = GBARegisterBLDCNTGetTarget1Bg1(value);
	renderer->bg[2].target1 = GBARegisterBLDCNTGetTarget1Bg2(value);
	renderer->bg[3].target1 = GBARegisterBLDCNTGetTarget1Bg3(value);
	renderer->bg[0].target2 = GBARegisterBLDCNTGetTarget2Bg0(value);
	renderer->bg[1].target2 = GBARegisterBLDCNTGetTarget2Bg1(value);
	renderer->bg[2].target2 = GBARegisterBLDCNTGetTarget2Bg2(value);
	renderer->bg[3].target2 = GBARegisterBLDCNTGetTarget2Bg3(value);

	renderer->blendEffect = GBARegisterBLDCNTGetEffect(value);
	renderer->target1Obj = GBARegisterBLDCNTGetTarget1Obj(value);
	renderer->target1Bd = GBARegisterBLDCNTGetTarget1Bd(value);
	renderer->target2Obj = GBARegisterBLDCNTGetTarget2Obj(value);
	renderer->target2Bd = GBARegisterBLDCNTGetTarget2Bd(value);
}