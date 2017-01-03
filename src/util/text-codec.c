/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/text-codec.h>

#include <mgba-util/string.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

struct TextCodecNode {
	uint8_t* leaf;
	size_t leafLength;
	struct Table children;
};

static void _cleanTree(void* value) {
	struct TextCodecNode* node = value;
	if (node->leaf) {
		free(node->leaf);
	}
	TableDeinit(&node->children);
	free(node);
}

static struct TextCodecNode* _createNode(void) {
	struct TextCodecNode* node = malloc(sizeof(*node));
	node->leaf = NULL;
	node->leafLength = 0;
	TableInit(&node->children, 32, _cleanTree);
	return node;
}

static void _insertLeaf(struct TextCodecNode* node, uint8_t* word, size_t wordLength, uint8_t* output, size_t outputLength) {
	if (!wordLength) {
		node->leafLength = outputLength;
		node->leaf = malloc(outputLength);
		memcpy(node->leaf, output, outputLength);
		return;
	}
	struct TextCodecNode* subnode = TableLookup(&node->children, word[0]);
	if (!subnode) {
		subnode = _createNode();
		TableInsert(&node->children, word[0], subnode);
	}
	_insertLeaf(subnode, &word[1], wordLength - 1, output, outputLength);
}

bool TextCodecLoadTBL(struct TextCodec* codec, struct VFile* vf, bool createReverse) {
	codec->forwardRoot = _createNode();
	if (createReverse) {
		codec->reverseRoot = _createNode();
	} else {
		codec->reverseRoot = NULL;
	}

	char lineBuffer[128];
	uint8_t wordBuffer[5];
	ssize_t length;
	while ((length = vf->readline(vf, lineBuffer, sizeof(lineBuffer))) > 0) {
		memset(wordBuffer, 0, sizeof(wordBuffer));
		if (lineBuffer[length - 1] == '\n' || lineBuffer[length - 1] == '\r') {
			--length;
		}
		if (!length) {
			continue;
		}
		if (lineBuffer[length - 1] == '\r') {
			--length;
		}
		if (!length) {
			continue;
		}
		size_t i;
		for (i = 0; i < sizeof(wordBuffer) - 1 && i < (size_t) length; ++i) {
			if (!hex8(&lineBuffer[i * 2], &wordBuffer[i])) {
				break;
			}
		}
		if (!i) {
			uint8_t value;
			if (!hex8(lineBuffer, &value)) {
				switch (lineBuffer[0]) {
				case '*':
					lineBuffer[0] = '\n';
					break;
				case '\\':
					lineBuffer[0] = '\x1E';
					break;
				case '/':
					lineBuffer[0] = '\x1F';
					break;
				default:
					return false;
				}
				size_t start = 1;
				if (lineBuffer[1] == '=') {
					start = 2;
				}
				for (i = 0; i < sizeof(wordBuffer) - 1; ++i) {
					if (!hex8(&lineBuffer[start + i * 2], &wordBuffer[i])) {
						break;
					}
				}
				if (i == 0) {
					return false;
				}
				_insertLeaf(codec->forwardRoot, wordBuffer, i, (uint8_t*) lineBuffer, 1);
				if (codec->reverseRoot) {
					_insertLeaf(codec->reverseRoot, (uint8_t*) lineBuffer, 1, wordBuffer, i);
				}
			}
		} else {
			if (lineBuffer[i * 2] != '=') {
				return false;
			}
			size_t offset = i * 2 + 1;
			_insertLeaf(codec->forwardRoot, wordBuffer, i, (uint8_t*) &lineBuffer[offset], length - offset);
			if (codec->reverseRoot) {
				_insertLeaf(codec->reverseRoot, (uint8_t*) &lineBuffer[offset], length - offset, wordBuffer, i);
			}
		}
	}
	return length == 0;
}

void TextCodecDeinit(struct TextCodec* codec) {
	if (codec->forwardRoot) {
		_cleanTree(codec->forwardRoot);
		codec->forwardRoot = NULL;
	}
	if (codec->reverseRoot) {
		_cleanTree(codec->reverseRoot);
		codec->reverseRoot = NULL;
	}
}

void TextCodecStartDecode(struct TextCodec* codec, struct TextCodecIterator* iter) {
	iter->root = codec->forwardRoot;
	iter->current = iter->root;
}

void TextCodecStartEncode(struct TextCodec* codec, struct TextCodecIterator* iter) {
	iter->root = codec->reverseRoot;
	iter->current = iter->root;
}

static ssize_t _TextCodecFinishInternal(struct TextCodecNode* node, uint8_t* output, size_t outputLength) {
	if (outputLength > node->leafLength) {
		outputLength = node->leafLength;
	}
	if (node->leafLength == 0) {
		return -1;
	}
	memcpy(output, node->leaf, outputLength);
	return node->leafLength;
}

ssize_t TextCodecAdvance(struct TextCodecIterator* iter, uint8_t byte, uint8_t* output, size_t outputLength) {
	struct TextCodecNode* node = TableLookup(&iter->current->children, byte);
	if (!node) {
		ssize_t size = _TextCodecFinishInternal(iter->current, output, outputLength);
		if (size < 0) {
			size = 0;
		}
		output += size;
		outputLength -= size;
		if (!outputLength) {
			return size;
		}
		if (iter->current == iter->root) {
			return -1;
		}
		iter->current = iter->root;
		ssize_t newSize = TextCodecAdvance(iter, byte, output, outputLength);
		if (newSize < 0 && size > 0) {
			return size;
		}
		return newSize + size;
	}
	if (TableSize(&node->children) == 0) {
		iter->current = iter->root;
		return _TextCodecFinishInternal(node, output, outputLength);
	}
	iter->current = node;
	return 0;
}

ssize_t TextCodecFinish(struct TextCodecIterator* iter, uint8_t* output, size_t outputLength) {
	struct TextCodecNode* node = iter->current;
	iter->current = iter->root;
	if (node->leafLength == 0) {
		return 0;
	}
	return _TextCodecFinishInternal(node, output, outputLength);
}
