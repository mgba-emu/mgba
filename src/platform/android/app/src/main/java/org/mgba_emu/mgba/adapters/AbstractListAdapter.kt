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
import androidx.recyclerview.widget.RecyclerView
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

abstract class AbstractListAdapter<Model : Any, Holder : AbstractViewHolder<Model>>(
    open var currentList: List<Model>
) : RecyclerView.Adapter<Holder>() {
    override fun onBindViewHolder(holder: Holder, position: Int) {
        holder.bind(currentList[position])
    }

    override fun getItemCount(): Int = currentList.size

    open fun addItem(item: Model, position: Int = -1, callback: ((position: Int) -> Unit)? = null) {
        val newList = currentList.toMutableList()
        val positionToUpdate: Int
        if (position == -1) {
            newList.add(item)
            currentList = newList
            positionToUpdate = currentList.size - 1
        } else {
            newList.add(position, item)
            currentList = newList
            positionToUpdate = position
        }
        onItemAdded(positionToUpdate, callback)
    }

    protected fun onItemAdded(position: Int, callback: ((Int) -> Unit)? = null) {
        notifyItemInserted(position)
        callback?.invoke(position)
    }

    fun changeItem(item: Model, position: Int, callback: ((position: Int) -> Unit)? = null) {
        val newList = currentList.toMutableList()
        newList[position] = item
        currentList = newList
        onItemChanged(position, callback)
    }

    protected fun onItemChanged(position: Int, callback: ((Int) -> Unit)? = null) {
        notifyItemChanged(position)
        callback?.invoke(position)
    }

    fun removeItem(position: Int, callback: ((position: Int) -> Unit)? = null) {
        val newList = currentList.toMutableList()
        newList.removeAt(position)
        currentList = newList
        onItemRemoved(position, callback)
    }

    protected fun onItemRemoved(position: Int, callback: ((Int) -> Unit)? = null) {
        notifyItemRemoved(position)
        callback?.invoke(position)
    }

    @SuppressLint("NotifyDataSetChanged")
    open fun replaceList(newList: List<Model>) {
        currentList = newList
        notifyDataSetChanged()
    }
}