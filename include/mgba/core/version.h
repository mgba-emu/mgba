/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mgba-util/dllexports.h>

extern MGBA_EXPORT const char* const gitCommit;
extern MGBA_EXPORT const char* const gitCommitShort;
extern MGBA_EXPORT const char* const gitBranch;
extern MGBA_EXPORT const int gitRevision;
extern MGBA_EXPORT const char* const binaryName;
extern MGBA_EXPORT const char* const projectName;
extern MGBA_EXPORT const char* const projectVersion;

#ifdef __cplusplus
}
#endif

#endif
