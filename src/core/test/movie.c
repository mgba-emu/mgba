/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/core.h>
#include <mgba/core/movie.h>
#include <mgba-util/vfs.h>

static void _testSetKeys(struct mCore* core, uint32_t keys) {
	// Mock implementation
	core->opts.frameskip = keys; // Hack to store keys in opts for verification
}

static uint32_t _testGetKeys(struct mCore* core) {
	// Mock implementation
	return core->opts.frameskip;
}

static void _setupCore(struct mCore* core) {
	core->setKeys = _testSetKeys;
	core->getKeys = _testGetKeys;
	core->opts.frameskip = 0;
	// core->movie is already initialized if we were using a real core create,
	// but here we might need to manually init if we mock mCore.
	// Let's use mMovieCreate directly for unit testing mMovie logic.
}

M_TEST_DEFINE(movieRecordPlayback) {
	struct mMovie* movie = mMovieCreate();
	struct mCore core;
	_setupCore(&core);

	// Test Recording
	movie->mode = MOVIE_RECORD;
	core.opts.frameskip = 0x1; // simulate input
	mMovieHookRunFrame(movie, &core);
	core.opts.frameskip = 0x2;
	mMovieHookRunFrame(movie, &core);

	assert_int_equal(movie->frameCount, 2);
	assert_int_equal(movie->inputs[0], 0x1);
	assert_int_equal(movie->inputs[1], 0x2);

	// Test Playback
	movie->mode = MOVIE_PLAY;
	movie->currentFrame = 0;
	core.opts.frameskip = 0;

	mMovieHookRunFrame(movie, &core);
	assert_int_equal(core.opts.frameskip, 0x1);
	assert_int_equal(movie->currentFrame, 1);

	mMovieHookRunFrame(movie, &core);
	assert_int_equal(core.opts.frameskip, 0x2);
	assert_int_equal(movie->currentFrame, 2);
	assert_int_equal(movie->mode, MOVIE_PLAY); // It stops AFTER next frame check usually? 
	// The implementation checks currentFrame < frameCount at start.
	// So at frame 2 (0-indexed 0, 1), it is == frameCount.

	mMovieHookRunFrame(movie, &core);
	assert_int_equal(movie->mode, MOVIE_STOP);

	mMovieDestroy(movie);
}

M_TEST_DEFINE(movieIO) {
	struct mMovie* movie = mMovieCreate();
	movie->mode = MOVIE_RECORD;
	movie->frameCount = 2;
	movie->inputs[0] = 0xDEAD;
	movie->inputs[1] = 0xBEEF;

	struct VFile* vf = VFileMemChunk(NULL, 0); // Dynamic memory file
	assert_true(mMovieSave(movie, vf));
	
	// Rewind
	vf->seek(vf, 0, SEEK_SET);

	struct mMovie* loadedMovie = mMovieCreate();
	assert_true(mMovieLoad(loadedMovie, vf));

	assert_int_equal(loadedMovie->frameCount, 2);
	assert_int_equal(loadedMovie->inputs[0], 0xDEAD);
	assert_int_equal(loadedMovie->inputs[1], 0xBEEF);
	assert_int_equal(loadedMovie->mode, MOVIE_PLAY);

	// Test invalid magic
	vf->seek(vf, 0, SEEK_SET);
	uint32_t badMagic = 0;
	vf->write(vf, &badMagic, sizeof(badMagic));
	vf->seek(vf, 0, SEEK_SET);
	
	assert_false(mMovieLoad(loadedMovie, vf));

	mMovieDestroy(movie);
	mMovieDestroy(loadedMovie);
	vf->close(vf);
}

M_TEST_SUITE_DEFINE(mMovie,
	cmocka_unit_test(movieRecordPlayback),
	cmocka_unit_test(movieIO))
