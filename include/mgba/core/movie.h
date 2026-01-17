/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_MOVIE_H
#define M_MOVIE_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct mCore;
struct mStateExtdata;
struct VFile;

enum mMovieMode {
	MOVIE_STOP = 0,
	MOVIE_PLAY,
	MOVIE_RECORD
};

struct mMovie {
	enum mMovieMode mode;
	uint32_t currentFrame;
	uint32_t frameCount;
	uint16_t* inputs;
	size_t capacity;
};

struct mMovie* mMovieCreate(void);
void mMovieDestroy(struct mMovie* movie);

void mMovieReset(struct mMovie* movie);
bool mMovieLoad(struct mMovie* movie, struct VFile* vf);
bool mMovieSave(struct mMovie* movie, struct VFile* vf);

void mMovieHookRunFrame(struct mMovie* movie, struct mCore* core);
void mMovieHookStateLoaded(struct mMovie* movie, const struct mStateExtdata* extdata);
void mMovieHookStateSaved(struct mMovie* movie, struct mStateExtdata* extdata);

CXX_GUARD_END

#endif
