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
import android.content.SharedPreferences
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.edit
import androidx.core.content.withStyledAttributes
import com.google.android.material.materialswitch.MaterialSwitch
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.utils.GlobalConfig

class SwitchPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    private var prefKey: String? = null
    private var sharedPreferences: SharedPreferences =
        context.getSharedPreferences(GlobalConfig.PREFS_NAME, Context.MODE_PRIVATE)

    private val titleView: TextView
    private val subtitleView: TextView
    private val switchView: MaterialSwitch

    init {
        LayoutInflater.from(context).inflate(R.layout.switch_preference, this, true)

        titleView = findViewById(R.id.title)
        subtitleView = findViewById(R.id.subtitle)
        switchView = findViewById(R.id.materialSwitch)

        switchView.isSaveEnabled = false
        isSaveEnabled = false

        isClickable = true
        isFocusable = true

        attrs?.let {
            context.withStyledAttributes(it, R.styleable.SwitchPreference, 0, 0) {
                val titleText = getString(R.styleable.SwitchPreference_title)
                val subtitleText = getString(R.styleable.SwitchPreference_subtitle)
                prefKey = getString(R.styleable.SwitchPreference_key)
                val defaultValue = getBoolean(R.styleable.SwitchPreference_defaultValue, false)

                titleView.text = titleText

                if (subtitleText.isNullOrEmpty()) {
                    subtitleView.visibility = GONE
                } else {
                    subtitleView.text = subtitleText
                }

                prefKey?.let { key ->
                    switchView.isChecked = sharedPreferences.getBoolean(key, defaultValue)
                }
            }
        }

        setOnClickListener {
            switchView.toggle()
        }

        switchView.setOnCheckedChangeListener { _, isChecked ->
            prefKey?.let { key ->
                sharedPreferences.edit { putBoolean(key, isChecked) }
            }
        }
    }
}