/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/canvas.h>

#include <mgba/feature/video-backend.h>
#include <mgba/script/base.h>
#include <mgba-util/image.h>

struct mScriptCanvasContext;
struct mScriptCanvasLayer {
	struct VideoBackend* backend;
	enum VideoLayer layer;
	struct mImage* image;
	int x;
	int y;
	unsigned scale;
	bool dirty;
	bool sizeDirty;
	bool dimsDirty;
	bool contentsDirty;
};

struct mScriptCanvasContext {
	struct mScriptCanvasLayer overlays[VIDEO_LAYER_OVERLAY_COUNT];
	struct VideoBackend* backend;
	struct mScriptContext* context;
	uint32_t frameCbid;
	unsigned scale;
};

mSCRIPT_DECLARE_STRUCT(mScriptCanvasContext);
mSCRIPT_DECLARE_STRUCT(mScriptCanvasLayer);

static void mScriptCanvasLayerDestroy(struct mScriptCanvasLayer* layer);
static void mScriptCanvasLayerUpdate(struct mScriptCanvasLayer* layer);

static int _getNextAvailableOverlay(struct mScriptCanvasContext* context) {
	if (!context->backend) {
		return -1;
	}
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		int w = -1, h = -1;
		context->backend->imageSize(context->backend, VIDEO_LAYER_OVERLAY0 + i, &w, &h);
		if (w <= 0 && h <= 0) {
			return i;
		}
	}
	return -1;
}

static void mScriptCanvasContextDeinit(struct mScriptCanvasContext* context) {
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		if (context->overlays[i].image) {
			mScriptCanvasLayerDestroy(&context->overlays[i]);
		}
	}
}

void mScriptCanvasUpdateBackend(struct mScriptContext* context, struct VideoBackend* backend) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "canvas");
	if (!value) {
		return;
	}
	struct mScriptCanvasContext* canvas = value->value.opaque;
	canvas->backend = backend;
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		struct mScriptCanvasLayer* layer = &canvas->overlays[i];
		layer->backend = backend;
		layer->dirty = true;
		layer->dimsDirty = true;
		layer->sizeDirty = true;
		layer->contentsDirty = true;
	}
}

void mScriptCanvasSetInternalScale(struct mScriptContext* context, unsigned scale) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "canvas");
	if (!value) {
		return;
	}
	struct mScriptCanvasContext* canvas = value->value.opaque;
	if (scale < 1) {
		scale = 1;
	}
	canvas->scale = scale;
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		canvas->overlays[i].scale = scale;
	}
}

static void _mScriptCanvasUpdate(struct mScriptCanvasContext* canvas) {
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		mScriptCanvasLayerUpdate(&canvas->overlays[i]);
	}
}

static unsigned _mScriptCanvasWidth(struct mScriptCanvasContext* canvas) {
	if (!canvas->backend) {
		return 0;
	}
	unsigned w, h;
	VideoBackendGetFrameSize(canvas->backend, &w, &h);
	return w / canvas->scale;
}

static unsigned _mScriptCanvasHeight(struct mScriptCanvasContext* canvas) {
	if (!canvas->backend) {
		return 0;
	}
	unsigned w, h;
	VideoBackendGetFrameSize(canvas->backend, &w, &h);
	return h / canvas->scale;
}

static int _mScriptCanvasScreenWidth(struct mScriptCanvasContext* canvas) {
	if (!canvas->backend) {
		return 0;
	}
	struct mRectangle dims;
	canvas->backend->layerDimensions(canvas->backend, VIDEO_LAYER_IMAGE, &dims);
	return dims.width / canvas->scale;
}

static int _mScriptCanvasScreenHeight(struct mScriptCanvasContext* canvas) {
	if (!canvas->backend) {
		return 0;
	}
	struct mRectangle dims;
	canvas->backend->layerDimensions(canvas->backend, VIDEO_LAYER_IMAGE, &dims);
	return dims.height / canvas->scale;
}

void mScriptCanvasUpdate(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "canvas");
	if (!value) {
		return;
	}
	struct mScriptCanvasContext* canvas = value->value.opaque;
	_mScriptCanvasUpdate(canvas);
}

static struct mScriptValue* mScriptCanvasLayerCreate(struct mScriptCanvasContext* context, int w, int h) {
	if (w <= 0 || h <= 0) {
		return NULL;
	}
	int next = _getNextAvailableOverlay(context);
	if (next < 0) {
		return NULL;
	}

	struct mScriptCanvasLayer* layer = &context->overlays[next];
	if (layer->image) {
		// This shouldn't exist yet
		abort();
	}

	layer->image = mImageCreate(w, h, mCOLOR_ABGR8);
	layer->dirty = true;
	layer->dimsDirty = true;
	layer->sizeDirty = true;
	layer->contentsDirty = true;
	layer->scale = context->scale;
	mScriptCanvasLayerUpdate(layer);

	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptCanvasLayer));
	value->value.opaque = layer;
	return value;
}

static void mScriptCanvasLayerDestroy(struct mScriptCanvasLayer* layer) {
	struct mRectangle frame = {0};
	if (layer->backend) {
		layer->backend->setLayerDimensions(layer->backend, layer->layer, &frame);
		layer->backend->setImageSize(layer->backend, layer->layer, 0, 0);
	}
	mImageDestroy(layer->image);
	layer->image = NULL;
}

static void mScriptCanvasLayerUpdate(struct mScriptCanvasLayer* layer) {
	if (!layer->dirty || !layer->image || !layer->backend) {
		return;
	}

	struct VideoBackend* backend = layer->backend;
	if (layer->sizeDirty) {
		backend->setImageSize(backend, layer->layer, layer->image->width, layer->image->height);
		layer->sizeDirty = false;
		// Resizing the image invalidates the contents in many backends
		layer->contentsDirty = true;
	}
	if (layer->dimsDirty) {
		struct mRectangle frame = {
			.x = layer->x * layer->scale,
			.y = layer->y * layer->scale,
			.width = layer->image->width * layer->scale,
			.height = layer->image->height * layer->scale,
		};
		backend->setLayerDimensions(backend, layer->layer, &frame);
		layer->dimsDirty = false;
	}
	if (layer->contentsDirty) {
		backend->setImage(backend, layer->layer, layer->image->data);
		layer->contentsDirty = false;
	}
	layer->dirty = false;
}


static void mScriptCanvasLayerSetPosition(struct mScriptCanvasLayer* layer, int32_t x, int32_t y) {
	layer->x = x;
	layer->y = y;
	layer->dimsDirty = true;
	layer->dirty = true;
}

static void mScriptCanvasLayerInvalidate(struct mScriptCanvasLayer* layer) {
	layer->contentsDirty = true;
	layer->dirty = true;
}

void mScriptContextAttachCanvas(struct mScriptContext* context) {
	struct mScriptCanvasContext* canvas = calloc(1, sizeof(*canvas));
	canvas->scale = 1;
	size_t i;
	for (i = 0; i < VIDEO_LAYER_OVERLAY_COUNT; ++i) {
		canvas->overlays[i].layer = VIDEO_LAYER_OVERLAY0 + i;
	}
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptCanvasContext));
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	value->value.opaque = canvas;

	canvas->context = context;
	struct mScriptValue* lambda = mScriptObjectBindLambda(value, "update", NULL);
	canvas->frameCbid = mScriptContextAddCallback(context, "frame", lambda);
	mScriptValueDeref(lambda);

	mScriptContextSetGlobal(context, "canvas", value);
	mScriptContextSetDocstring(context, "canvas", "Singleton instance of struct::mScriptCanvasContext");
}

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCanvasContext, _deinit, mScriptCanvasContextDeinit, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCanvasContext, W(mScriptCanvasLayer), newLayer, mScriptCanvasLayerCreate, 2, S32, width, S32, height);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCanvasContext, update, _mScriptCanvasUpdate, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCanvasContext, U32, width, _mScriptCanvasWidth, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCanvasContext, U32, height, _mScriptCanvasHeight, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCanvasContext, S32, screenWidth, _mScriptCanvasScreenWidth, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCanvasContext, S32, screenHeight, _mScriptCanvasScreenHeight, 0);

mSCRIPT_DEFINE_STRUCT(mScriptCanvasContext)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A canvas that can be used for drawing images on or around the screen."
	)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptCanvasContext)
	mSCRIPT_DEFINE_DOCSTRING("Create a new layer of a given size. If multiple layers overlap, the most recently created one takes priority.")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, newLayer)
	mSCRIPT_DEFINE_DOCSTRING("Update all layers marked as having pending changes")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, update)
	mSCRIPT_DEFINE_DOCSTRING("Get the width of the canvas")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, width)
	mSCRIPT_DEFINE_DOCSTRING("Get the height of the canvas")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, height)
	mSCRIPT_DEFINE_DOCSTRING("Get the width of the emulated screen")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, screenWidth)
	mSCRIPT_DEFINE_DOCSTRING("Get the height of the emulated screen")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasContext, screenHeight)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCanvasLayer, update, mScriptCanvasLayerInvalidate, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCanvasLayer, setPosition, mScriptCanvasLayerSetPosition, 2, S32, x, S32, y);

mSCRIPT_DEFINE_STRUCT(mScriptCanvasLayer)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An individual layer of a drawable canvas."
	)
	mSCRIPT_DEFINE_DOCSTRING("Mark the contents of the layer as needed to be repainted")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasLayer, update)
	mSCRIPT_DEFINE_DOCSTRING("Set the position of the layer in the canvas")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCanvasLayer, setPosition)
	mSCRIPT_DEFINE_DOCSTRING("The image that has the pixel contents of the image")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptCanvasLayer, PS(mImage), image)
	mSCRIPT_DEFINE_DOCSTRING("The current x (horizontal) position of this layer")
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(mScriptCanvasLayer, S32, x)
	mSCRIPT_DEFINE_DOCSTRING("The current y (vertical) position of this layer")
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(mScriptCanvasLayer, S32, y)
mSCRIPT_DEFINE_END;
