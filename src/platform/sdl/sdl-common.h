/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_COMMON_H
#define SDL_COMMON_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define SDL_ENABLE_OLD_NAMES
#include <SDL.h>
// Altivec sometimes defines this
#ifdef vector
#undef vector
#endif
#ifdef bool
#undef bool
#define bool _Bool
#endif

#if SDL_VERSION_ATLEAST(3, 0, 0)
#define SDL_OK(X) (X)
#else
#define SDL_OK(X) ((X) >= 0)
#endif

CXX_GUARD_END

#endif
