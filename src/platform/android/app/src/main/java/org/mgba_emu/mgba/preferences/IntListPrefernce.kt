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
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.utils.GlobalConfig

class IntListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    private var prefKey: String? = null
    private var sharedPreferences: SharedPreferences =
        context.getSharedPreferences(GlobalConfig.PREFS_NAME, Context.MODE_PRIVATE)

    private val titleView: TextView
    private val subtitleView: TextView

    private var entries: Array<CharSequence> = emptyArray()
    private var entryValues: IntArray = intArrayOf()
    private var currentValue: Int = 0
    private var defaultSubtitle: String? = null

    init {
        LayoutInflater.from(context).inflate(R.layout.list_preference, this, true)

        titleView = findViewById(R.id.title)
        subtitleView = findViewById(R.id.subtitle)

        isClickable = true
        isFocusable = true

        val typedValue = android.util.TypedValue()
        context.theme.resolveAttribute(android.R.attr.selectableItemBackground, typedValue, true)

        setBackgroundResource(typedValue.resourceId)

        attrs?.let {
            context.withStyledAttributes(it, R.styleable.IntListPreference, 0, 0) {
                val titleText = getString(R.styleable.IntListPreference_title)
                defaultSubtitle = getString(R.styleable.IntListPreference_subtitle)
                prefKey = getString(R.styleable.IntListPreference_key)
                val defaultValue = getInt(R.styleable.IntListPreference_defaultValue, 0)

                entries = getTextArray(R.styleable.IntListPreference_entries) ?: emptyArray()

                val entryValuesResId = getResourceId(R.styleable.IntListPreference_entryValues, 0)
                if (entryValuesResId != 0) {
                    entryValues = resources.getIntArray(entryValuesResId)
                }

                titleView.text = titleText
                prefKey?.let { key ->
                    currentValue = sharedPreferences.getInt(key, defaultValue)
                }

                updateSubtitle()
            }
        }

        setOnClickListener {
            showPreferenceDialog()
        }
    }

    private fun updateSubtitle() {
        val selectedIndex = entryValues.indexOf(currentValue)

        if (selectedIndex in entries.indices) {
            subtitleView.text = entries[selectedIndex]
            subtitleView.visibility = VISIBLE
        } else if (!defaultSubtitle.isNullOrEmpty()) {
            subtitleView.text = defaultSubtitle
            subtitleView.visibility = VISIBLE
        } else {
            subtitleView.visibility = GONE
        }
    }

    private fun showPreferenceDialog() {
        if (entries.isEmpty() || entryValues.isEmpty()) return
        val selectedIndex = entryValues.indexOf(currentValue).takeIf { it >= 0 } ?: 0

        MaterialAlertDialogBuilder(context)
            .setTitle(titleView.text)
            .setSingleChoiceItems(entries, selectedIndex) { dialog, which ->
                currentValue = entryValues[which]
                prefKey?.let { key ->
                    sharedPreferences.edit { putInt(key, currentValue) }
                }

                updateSubtitle()
                dialog.dismiss()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }
}