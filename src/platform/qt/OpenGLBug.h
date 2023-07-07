/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

namespace QGBA {

enum class OpenGLBug {
	CROSS_THREAD_FLUSH,       // mgba.io/i/2761
	GLTHREAD_BLOCKS_SWAP,     // mgba.io/i/2767
	IG4ICD_CRASH,             // mgba.io/i/2136
};

bool glContextHasBug(OpenGLBug);

}
