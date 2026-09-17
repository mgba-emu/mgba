/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.preferences

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.navigation.findNavController
import org.mgba_emu.mgba.R

class ImportBiosPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    init {
        LayoutInflater.from(context).inflate(R.layout.nav_preference, this, true)

        val titleView = findViewById<TextView>(R.id.title)
        val subtitleView = findViewById<TextView>(R.id.subtitle)

        context.obtainStyledAttributes(attrs, R.styleable.SwitchPreference, 0, 0).apply {
            titleView.text = getString(R.styleable.SwitchPreference_title)
            val subtitleText = getString(R.styleable.SwitchPreference_subtitle)
            if (subtitleText.isNullOrEmpty()) {
                subtitleView.visibility = GONE
            } else {
                subtitleView.text = subtitleText
            }
            recycle()
        }

        isClickable = true
        isFocusable = true

        setOnClickListener {
            try {
                findNavController().navigate(R.id.action_settingsFragment_to_biosManagerFragment)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}