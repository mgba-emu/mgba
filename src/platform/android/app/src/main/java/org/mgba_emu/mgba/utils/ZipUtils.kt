/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.BuildConfig


object ZipUtils {
    external fun exportUserData(folderPath: String, outFd: Int, appId: String = BuildConfig.APPLICATION_ID): Boolean
    external fun importUserData(inFd: Int, targetExtractDir: String, expectedAppId: String = BuildConfig.APPLICATION_ID): Boolean
}

