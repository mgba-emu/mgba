/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PSP2_CONTEXT_H
#define PSP2_CONTEXT_H

#include "psp2-common.h"

void GBAPSP2Setup(void);
void GBAPSP2Teardown(void);

bool GBAPSP2LoadROM(const char* path);
void GBAPSP2Runloop(void);
void GBAPSP2UnloadROM(void);

void GBAPSP2Draw(uint8_t alpha);

#endif
