/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MGBA_EXPORT_H
#define MGBA_EXPORT_H

#if defined(BUILD_STATIC) || !defined(_MSC_VER) || defined(MGBA_STANDALONE)
#define MGBA_EXPORT
#else
#ifdef MGBA_DLL
#define MGBA_EXPORT __declspec(dllexport)
#else
#define MGBA_EXPORT __declspec(dllimport)
#endif
#endif

#endif
