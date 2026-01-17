/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/movie.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>
#include <stdlib.h>
#include <string.h>
#define INITIAL_CAPACITY 1024
struct mMovie* mMovieCreate(void) {
	struct mMovie* movie = malloc(sizeof(struct mMovie));
	movie->mode = MOVIE_STOP;
	movie->currentFrame = 0;
	movie->frameCount = 0;
	movie->capacity = INITIAL_CAPACITY;
	movie->inputs = malloc(sizeof(uint16_t) * movie->capacity);
	return movie;
}
void mMovieDestroy(struct mMovie* movie) {
	if (!movie) {
		return;
	}
	free(movie->inputs);
	free(movie);
}
void mMovieReset(struct mMovie* movie) {
	movie->mode = MOVIE_STOP;
	movie->currentFrame = 0;
	movie->frameCount = 0;
}
#define MOVIE_MAGIC 0x314D474D // "MGM1"
bool mMovieLoad(struct mMovie* movie, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	off_t size = vf->size(vf);
	if (size < (off_t) (sizeof(uint32_t) * 2)) {
		return false;
	}
	uint32_t magic;
	vf->read(vf, &magic, sizeof(uint32_t));
	if (magic != MOVIE_MAGIC) {
		return false;
	}
	vf->read(vf, &movie->frameCount, sizeof(uint32_t));
	if (movie->frameCount > movie->capacity) {
		movie->inputs = realloc(movie->inputs, sizeof(uint16_t) * movie->frameCount);
		movie->capacity = movie->frameCount;
	}
	vf->read(vf, movie->inputs, sizeof(uint16_t) * movie->frameCount);
	movie->currentFrame = 0;
	movie->mode = MOVIE_PLAY; // Default to play on load
	return true;
}
bool mMovieSave(struct mMovie* movie, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	uint32_t magic = MOVIE_MAGIC;
	vf->write(vf, &magic, sizeof(uint32_t));
	vf->write(vf, &movie->frameCount, sizeof(uint32_t));
	vf->write(vf, movie->inputs, sizeof(uint16_t) * movie->frameCount);
	return true;
}
void mMovieHookRunFrame(struct mMovie* movie, struct mCore* core) {
	if (movie->mode == MOVIE_STOP) {
		return;
	}
	if (movie->mode == MOVIE_PLAY) {
		if (movie->currentFrame < movie->frameCount) {
			core->setKeys(core, movie->inputs[movie->currentFrame]);
			movie->currentFrame++;
		} else {
			movie->mode = MOVIE_STOP; // End of movie
		}
	} else if (movie->mode == MOVIE_RECORD) {
		uint32_t keys = core->getKeys(core);
		if (movie->currentFrame >= movie->capacity) {
			movie->capacity *= 2;
			movie->inputs = realloc(movie->inputs, sizeof(uint16_t) * movie->capacity);
		}
		movie->inputs[movie->currentFrame] = keys;
		movie->currentFrame++;
		if (movie->currentFrame > movie->frameCount) {
			movie->frameCount = movie->currentFrame;
		}
	}
}
void mMovieHookStateLoaded(struct mMovie* movie, const struct mStateExtdata* extdata) {
	struct mStateExtdataItem item;
	if (mStateExtdataGet(extdata, EXTDATA_FEATURE_MOVIE, &item)) {
		if ((uint32_t) item.size == sizeof(uint32_t)) {
			memcpy(&movie->currentFrame, item.data, sizeof(uint32_t));
		}
	}
}
void mMovieHookStateSaved(struct mMovie* movie, struct mStateExtdata* extdata) {
	struct mStateExtdataItem item;
	item.size = sizeof(uint32_t);
	item.data = malloc(item.size);
	item.clean = free;
	memcpy(item.data, &movie->currentFrame, sizeof(uint32_t));
	mStateExtdataPut(extdata, EXTDATA_FEATURE_MOVIE, &item);
}
