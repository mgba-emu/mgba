/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_ALGO_H
#define ARM_ALGO_H

#ifdef __arm__
#if defined(__ARM_NEON)
void _neon2x(void* dest, void* src, int width, int height);
void _neon4x(void* dest, void* src, int width, int height);
#endif
#endif

#endif
