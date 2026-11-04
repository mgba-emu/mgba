/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.app.UiModeManager
import android.content.Context
import android.content.res.Configuration
import android.text.TextUtils
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import org.mgba_emu.mgba.mGBAApplication

object ViewUtils {
    private val uiModeManager = mGBAApplication.context.getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
    val isTv = uiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION

    fun View.applySafePadding() {
        ViewCompat.setOnApplyWindowInsetsListener(this) { view, windowInsets ->
            val insets = windowInsets.getInsets(
                WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout()
            )

            view.updatePadding(
                left = insets.left,
                top = insets.top,
                right = insets.right,
                bottom = insets.bottom
            )

            windowInsets
        }
    }

    fun View.updateMargins(
        left: Int = -1,
        top: Int = -1,
        right: Int = -1,
        bottom: Int = -1
    ) {
        val layoutParams = this.layoutParams as ViewGroup.MarginLayoutParams
        layoutParams.apply {
            if (left != -1) {
                leftMargin = left
            }
            if (top != -1) {
                topMargin = top
            }
            if (right != -1) {
                rightMargin = right
            }
            if (bottom != -1) {
                bottomMargin = bottom
            }
        }
        this.layoutParams = layoutParams
    }

    fun TextView.marquee(delay: Long = 3000) {
        ellipsize = null
        marqueeRepeatLimit = -1
        isSingleLine = true
        postDelayed({
            ellipsize = TextUtils.TruncateAt.MARQUEE
            isSelected = true
        }, delay)
    }
}
