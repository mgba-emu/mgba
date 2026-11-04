/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.adapters

import android.annotation.SuppressLint
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

abstract class AbstractDiffAdapter<Model : Any, Holder : AbstractViewHolder<Model>>(
    exact: Boolean = true
) : ListAdapter<Model, Holder>(AsyncDifferConfig.Builder(DiffCallback<Model>(exact)).build()) {
    override fun onBindViewHolder(holder: Holder, position: Int) =
        holder.bind(currentList[position])

    private class DiffCallback<Model>(val exact: Boolean) : DiffUtil.ItemCallback<Model>() {
        override fun areItemsTheSame(oldItem: Model & Any, newItem: Model & Any): Boolean {
            if (exact) {
                return oldItem === newItem
            }
            return oldItem == newItem
        }

        @SuppressLint("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: Model & Any, newItem: Model & Any): Boolean {
            return oldItem == newItem
        }
    }
}