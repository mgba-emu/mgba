/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef TEXT_CODEC_H
#define TEXT_CODEC_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct TextCodecNode;
struct TextCodec {
	struct TextCodecNode* forwardRoot;
	struct TextCodecNode* reverseRoot;
};

struct TextCodecIterator {
	struct TextCodecNode* root;
	struct TextCodecNode* current;
};

struct VFile;
bool TextCodecLoadTBL(struct TextCodec*, struct VFile*, bool createReverse);
void TextCodecDeinit(struct TextCodec*);

void TextCodecStartDecode(struct TextCodec*, struct TextCodecIterator*);
void TextCodecStartEncode(struct TextCodec*, struct TextCodecIterator*);

ssize_t TextCodecAdvance(struct TextCodecIterator*, uint8_t byte, uint8_t* output, size_t outputLength);
ssize_t TextCodecFinish(struct TextCodecIterator*, uint8_t* output, size_t outputLength);

CXX_GUARD_END

#endif
