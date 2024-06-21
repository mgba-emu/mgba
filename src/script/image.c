/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script.h>

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
mSCRIPT_DECLARE_STRUCT_C_METHOD(mImage, U32, getPixel, mImageGetPixel, 2, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, setPixel, mImageSetPixel, 3, U32, x, U32, y, U32, color);
mSCRIPT_DECLARE_STRUCT_C_METHOD_WITH_DEFAULTS(mImage, BOOL, save, mImageSave, 2, CHARP, path, CHARP, format);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, _deinit, mImageDestroy, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mImage, drawImageOpaque, mImageBlit, 3, CS(mImage), image, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(mImage, drawImage, mImageCompositeWithAlpha, 4, CS(mImage), image, U32, x, U32, y, F32, alpha);

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mImage, save)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_CHARP("PNG")
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mImage, drawImage)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_F32(1.0f)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT(mImage)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A single, static image."
	)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mImage)
	mSCRIPT_DEFINE_DOCSTRING("Save the image to a file. Currently, only `PNG` format is supported")
	mSCRIPT_DEFINE_STRUCT_METHOD(mImage, save)
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
mSCRIPT_BIND_FUNCTION(mImageLoad_Binding, W(mImage), _mImageLoad, 1, CHARP, path);

void mScriptContextAttachImage(struct mScriptContext* context) {
	mScriptContextExportNamespace(context, "image", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(new, &mImageNew_Binding),
		mSCRIPT_KV_PAIR(load, &mImageLoad_Binding),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextSetDocstring(context, "image", "Methods for creating struct::mImage instances");
	mScriptContextSetDocstring(context, "image.new", "Create a new image with the given dimensions");
	mScriptContextSetDocstring(context, "image.load", "Load an image from a path. Currently, only `PNG` format is supported");
}
