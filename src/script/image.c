/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script.h>

struct mScriptPainter {
	struct mPainter painter;
	struct mScriptValue* image;
	struct mPainter* ppainter; // XXX: Figure out how to not need this without breaking the type system
};

mSCRIPT_DECLARE_STRUCT(mPainter);
mSCRIPT_DECLARE_STRUCT(mScriptPainter);
#ifdef USE_FREETYPE
mSCRIPT_DECLARE_STRUCT(mTextRunMetrics);
#endif

static struct mScriptValue* _mImageNew(unsigned width, unsigned height) {
	// For various reasons, it's probably a good idea to limit the maximum image size scripts can make
	if (width >= 10000 || height >= 10000) {
		return NULL;
	}
	struct mImage* image = mImageCreate(width, height, mCOLOR_ABGR8);
	if (!image) {
		return NULL;
	}
	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mImage));
	result->value.opaque = image;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT;
	return result;
}

#ifdef ENABLE_VFS
static struct mScriptValue* _mImageLoad(const char* path) {
	struct mImage* image = mImageLoad(path);
	if (!image) {
		return NULL;
	}
	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mImage));
	result->value.opaque = image;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT;
	return result;
}
#endif

static struct mScriptValue* _mImageNewPainter(struct mScriptValue* image) {
	mScriptValueRef(image);
	struct mScriptPainter* painter = malloc(sizeof(*painter));
	mPainterInit(&painter->painter, image->value.opaque);
	painter->image = image;
	painter->ppainter = &painter->painter;
	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptPainter));
	result->value.opaque = painter;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT;
	return result;
}

mSCRIPT_DECLARE_STRUCT_C_METHOD(mImage, U32, getPixel, mImageGetPixel, 2, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, setPixel, mImageSetPixel, 3, U32, x, U32, y, U32, color);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, _deinit, mImageDestroy, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, drawImageOpaque, mImageBlit, 3, CS(mImage), image, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(mImage, drawImage, mImageCompositeWithAlpha, 4, CS(mImage), image, U32, x, U32, y, F32, alpha);

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mImage, drawImage)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_F32(1.0f)
mSCRIPT_DEFINE_DEFAULTS_END;

#ifdef ENABLE_VFS
mSCRIPT_DECLARE_STRUCT_C_METHOD_WITH_DEFAULTS(mImage, BOOL, save, mImageSave, 2, CHARP, path, CHARP, format);
mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mImage, save)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_CHARP("PNG")
mSCRIPT_DEFINE_DEFAULTS_END;
#endif

mSCRIPT_DEFINE_STRUCT(mImage)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A single, static image."
	)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mImage)
#ifdef ENABLE_VFS
	mSCRIPT_DEFINE_DOCSTRING("Save the image to a file. Currently, only `PNG` format is supported")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, save)
#endif
	mSCRIPT_DEFINE_DOCSTRING("Get the ARGB value of the pixel at a given coordinate")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, getPixel)
	mSCRIPT_DEFINE_DOCSTRING("Set the ARGB value of the pixel at a given coordinate")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, setPixel)
	mSCRIPT_DEFINE_DOCSTRING("Draw another image onto this image without any alpha blending, overwriting what was already there")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, drawImageOpaque)
	mSCRIPT_DEFINE_DOCSTRING("Draw another image onto this image with alpha blending as needed, optionally specifying a coefficient for adjusting the opacity")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, drawImage)
	mSCRIPT_DEFINE_DOCSTRING("The width of the image, in pixels")
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(mImage, U32, width)
	mSCRIPT_DEFINE_DOCSTRING("The height of the image, in pixels")
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(mImage, U32, height)
mSCRIPT_DEFINE_END;

mSCRIPT_BIND_FUNCTION(mImageNew_Binding, W(mImage), _mImageNew, 2, U32, width, U32, height);
#ifdef ENABLE_VFS
mSCRIPT_BIND_FUNCTION(mImageLoad_Binding, W(mImage), _mImageLoad, 1, CHARP, path);
#endif
mSCRIPT_BIND_FUNCTION(mImageNewPainter_Binding, W(mScriptPainter), _mImageNewPainter, 1, W(mImage), image);

void _mPainterSetBlend(struct mPainter* painter, bool enable) {
	painter->blend = enable;
}

void _mPainterSetFill(struct mPainter* painter, bool enable) {
	painter->fill = enable;
}

void _mPainterSetFillColor(struct mPainter* painter, uint32_t color) {
	painter->fillColor = color;
}

void _mPainterSetStrokeWidth(struct mPainter* painter, uint32_t width) {
	painter->strokeWidth = width;
}

void _mPainterSetStrokeColor(struct mPainter* painter, uint32_t color) {
	painter->strokeColor = color;
}

#ifdef USE_FREETYPE
void _mPainterLoadFont(struct mPainter* painter, const char* path) {
	struct mFont* font = mFontOpen(path);
	if (!font) {
		return;
	}
	if (painter->font) {
		mFontDestroy(painter->font);
	}
	painter->font = font;
}

void _mPainterSetFontSize(struct mPainter* painter, float pt) {
	if (!painter->font) {
		return;
	}
	mFontSetSize(painter->font, pt * (1 << mFONT_FRACT_BITS));
}

struct mScriptValue* _mPainterTextRunMetrics(struct mPainter* painter, const char* text) {
	struct mTextRunMetrics* metrics = malloc(sizeof(*metrics));
	mFontRunMetrics(painter->font, text, metrics);

	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mTextRunMetrics));
	result->value.opaque = metrics;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT | mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	return result;
}

struct mScriptValue* _mPainterTextBoxSize(struct mPainter* painter, const char* text) {
	struct mSize* size = malloc(sizeof(*size));
	mFontTextBoxSize(painter->font, text, 0, size);

	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mSize));
	result->value.opaque = size;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT | mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	return result;
}
#endif

static struct mScriptValue* _mScriptPainterGet(struct mScriptPainter* painter, const char* name) {
	struct mScriptValue val;
	struct mScriptValue realPainter = mSCRIPT_MAKE(S(mPainter), &painter->painter);
	if (!mScriptObjectGet(&realPainter, name, &val)) {
		return &mScriptValueNull;
	}

	struct mScriptValue* ret = malloc(sizeof(*ret));
	memcpy(ret, &val, sizeof(*ret));
	ret->refs = 1;
	return ret;
}

void _mScriptPainterDeinit(struct mScriptPainter* painter) {
	mScriptValueDeref(painter->image);
#ifdef USE_FREETYPE
	if (painter->painter.font) {
		mFontDestroy(painter->painter.font);
	}
#endif
	free(painter);
}

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setBlend, _mPainterSetBlend, 1, BOOL, enable);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setFill, _mPainterSetFill, 1, BOOL, enable);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setFillColor, _mPainterSetFillColor, 1, U32, color);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setStrokeWidth, _mPainterSetStrokeWidth, 1, U32, width);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setStrokeColor, _mPainterSetStrokeColor, 1, U32, color);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, drawRectangle, mPainterDrawRectangle, 4, S32, x, S32, y, S32, width, S32, height);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, drawLine, mPainterDrawLine, 4, S32, x1, S32, y1, S32, x2, S32, y2);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, drawCircle, mPainterDrawCircle, 3, S32, x, S32, y, S32, diameter);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, drawMask, mPainterDrawMask, 3, CS(mImage), mask, S32, x, S32, y);
#ifdef USE_FREETYPE
mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(mPainter, drawText, mPainterDrawText, 4, CHARP, text, S32, x, S32, y, S32, alignment);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, loadFont, _mPainterLoadFont, 1, CHARP, path);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mPainter, setFontSize, _mPainterSetFontSize, 1, F32, pt);
mSCRIPT_DECLARE_STRUCT_METHOD(mPainter, W(mTextRunMetrics), textRunMetrics, _mPainterTextRunMetrics, 1, CHARP, text);
mSCRIPT_DECLARE_STRUCT_METHOD(mPainter, W(mSize), textBoxSize, _mPainterTextBoxSize, 1, CHARP, text);


mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mPainter, drawText)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(mALIGN_TOP | mALIGN_LEFT)
mSCRIPT_DEFINE_DEFAULTS_END;
#endif

mSCRIPT_DEFINE_STRUCT(mPainter)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A stateful object useful for performing drawing operations on an struct::mImage."
	)
	mSCRIPT_DEFINE_DOCSTRING("Set whether or not alpha blending should be enabled when drawing")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setBlend)
	mSCRIPT_DEFINE_DOCSTRING("Set whether or not the fill color should be applied when drawing")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setFill)
	mSCRIPT_DEFINE_DOCSTRING("Set the fill color to be used when drawing")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setFillColor)
	mSCRIPT_DEFINE_DOCSTRING("Set the stroke width to be used when drawing, or 0 to disable")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setStrokeWidth)
	mSCRIPT_DEFINE_DOCSTRING("Set the stroke color to be used when drawing")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setStrokeColor)
	mSCRIPT_DEFINE_DOCSTRING("Draw a rectangle with the specified dimensions")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, drawRectangle)
	mSCRIPT_DEFINE_DOCSTRING("Draw a line with the specified endpoints")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, drawLine)
	mSCRIPT_DEFINE_DOCSTRING("Draw a circle with the specified diameter with the given origin at the top-left corner of the bounding box")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, drawCircle)
	mSCRIPT_DEFINE_DOCSTRING(
		"Draw a mask image with each color channel multiplied by the current fill color. This can "
		"be useful for displaying graphics with dynamic colors. By making a grayscale template "
		"image on a transparent background in advance, a script can set the fill color to a desired "
		"target color and use this function to draw it into a destination image."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, drawMask)
#ifdef USE_FREETYPE
	mSCRIPT_DEFINE_DOCSTRING("Draw text with the currently set font and fill color")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, drawText)
	mSCRIPT_DEFINE_DOCSTRING("Load a font from a given filename")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, loadFont)
	mSCRIPT_DEFINE_DOCSTRING("Set the font size")
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, setFontSize)
	mSCRIPT_DEFINE_DOCSTRING(
		"Get the struct::mTextRunMetrics for the first line of a given string rendered in the current "
		"font. If you want the bounding box for multiple lines, use struct::mPainter.textBoxSize instead."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, textRunMetrics)
	mSCRIPT_DEFINE_DOCSTRING(
		"Get the bounding box size for the given string rendered in the current font. This "
		"will take into account line breaks, unlike struct::mPainter.textRunMetrics."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mPainter, textBoxSize)
#endif
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT_METHOD(mScriptPainter, W(mPainter), _get, _mScriptPainterGet, 1, CHARP, name);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptPainter, _deinit, _mScriptPainterDeinit, 0);

mSCRIPT_DEFINE_STRUCT(mScriptPainter)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptPainter)
	mSCRIPT_DEFINE_STRUCT_MEMBER_NAMED(mScriptPainter, PS(mPainter), _painter, ppainter)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(mScriptPainter)
	mSCRIPT_DEFINE_STRUCT_CAST_TO_MEMBER(mScriptPainter, S(mPainter), _painter)
mSCRIPT_DEFINE_END;

#ifdef USE_FREETYPE
float _mTextRunHeight(const struct mTextRunMetrics* metrics) {
	return metrics->height / (float) (1 << mFONT_FRACT_BITS);
}

float _mTextRunWidth(const struct mTextRunMetrics* metrics) {
	return metrics->width / (float) (1 << mFONT_FRACT_BITS);
}

float _mTextRunDescender(const struct mTextRunMetrics* metrics) {
	return metrics->baseline / (float) (1 << mFONT_FRACT_BITS);
}

float _mTextRunAscender(const struct mTextRunMetrics* metrics) {
	return (metrics->height - metrics->baseline) / (float) (1 << mFONT_FRACT_BITS);
}

mSCRIPT_DECLARE_STRUCT_C_METHOD(mTextRunMetrics, F32, height, _mTextRunHeight, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mTextRunMetrics, F32, width, _mTextRunWidth, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mTextRunMetrics, F32, descender, _mTextRunDescender, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mTextRunMetrics, F32, ascender, _mTextRunAscender, 0);

mSCRIPT_DEFINE_STRUCT(mTextRunMetrics)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"Metrics for the size of a run of text. Generally, a run will represent up to a single line of text."
	)
	mSCRIPT_DEFINE_DOCSTRING("Get the height of the run of text, in pixels")
	mSCRIPT_DEFINE_STRUCT_METHOD(mTextRunMetrics, height)
	mSCRIPT_DEFINE_DOCSTRING("Get the width of the run of text, in pixels")
	mSCRIPT_DEFINE_STRUCT_METHOD(mTextRunMetrics, width)
	mSCRIPT_DEFINE_DOCSTRING("Get the distance from the baseline to the bottom of the line, in pixels")
	mSCRIPT_DEFINE_STRUCT_METHOD(mTextRunMetrics, descender)
	mSCRIPT_DEFINE_DOCSTRING("Get the distance from the baseline to the top of the line, in pixels")
	mSCRIPT_DEFINE_STRUCT_METHOD(mTextRunMetrics, ascender)
mSCRIPT_DEFINE_END;
#endif

void mScriptContextAttachImage(struct mScriptContext* context) {
	mScriptContextExportNamespace(context, "image", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(new, &mImageNew_Binding),
#ifdef ENABLE_VFS
		mSCRIPT_KV_PAIR(load, &mImageLoad_Binding),
#endif
		mSCRIPT_KV_PAIR(newPainter, &mImageNewPainter_Binding),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextSetDocstring(context, "image", "Methods for creating struct::mImage and struct::mPainter instances");
	mScriptContextSetDocstring(context, "image.new", "Create a new image with the given dimensions");
#ifdef ENABLE_VFS
	mScriptContextSetDocstring(context, "image.load", "Load an image from a path. Currently, only `PNG` format is supported");
#endif
	mScriptContextSetDocstring(context, "image.newPainter", "Create a new painter from an existing image");

	mScriptContextExportConstants(context, "ALIGN", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mALIGN, LEFT),
		mSCRIPT_CONSTANT_PAIR(mALIGN, HCENTER),
		mSCRIPT_CONSTANT_PAIR(mALIGN, RIGHT),
		mSCRIPT_CONSTANT_PAIR(mALIGN, TOP),
		mSCRIPT_CONSTANT_PAIR(mALIGN, VCENTER),
		mSCRIPT_CONSTANT_PAIR(mALIGN, BOTTOM),
		mSCRIPT_CONSTANT_PAIR(mALIGN, BASELINE),
		mSCRIPT_KV_SENTINEL
	});
}
