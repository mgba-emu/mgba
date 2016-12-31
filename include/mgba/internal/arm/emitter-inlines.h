/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef EMITTER_INLINES_H
#define EMITTER_INLINES_H

#define DO_4(DIRECTIVE) \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE

#define DO_8(DIRECTIVE) \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE

#define DO_256(DIRECTIVE) \
	DO_4(DO_8(DO_8(DIRECTIVE)))

#define DO_INTERLACE(LEFT, RIGHT) \
	LEFT, \
	RIGHT

#endif
