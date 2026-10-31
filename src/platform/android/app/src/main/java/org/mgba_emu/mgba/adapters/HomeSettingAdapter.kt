/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.isVisible
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import com.google.android.material.shape.ShapeAppearanceModel
import kotlinx.coroutines.launch
import org.mgba_emu.mgba.R
import com.google.android.material.R as MaterialR
import org.mgba_emu.mgba.databinding.CardHomeOptionBinding
import org.mgba_emu.mgba.model.HomeSetting
import org.mgba_emu.mgba.utils.ViewUtils.marquee
import org.mgba_emu.mgba.viewholders.AbstractViewHolder

class HomeSettingAdapter(
    private val activity: AppCompatActivity,
    private val viewLifecycle: LifecycleOwner,
    options: List<HomeSetting>
) : AbstractListAdapter<HomeSetting, HomeSettingAdapter.HomeOptionViewHolder>(options) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HomeOptionViewHolder {
        CardHomeOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return HomeOptionViewHolder(it) }
    }

    inner class HomeOptionViewHolder(val binding: CardHomeOptionBinding) :
        AbstractViewHolder<HomeSetting>(binding) {
        override fun bind(model: HomeSetting) {
            val context = itemView.context
            val totalItems = itemCount

            val shapeResId = when {
                totalItems == 1 -> MaterialR.style.ShapeAppearance_Material3_ListItem_Single
                position == 0 -> MaterialR.style.ShapeAppearance_Material3_ListItem_First
                position == totalItems - 1 -> MaterialR.style.ShapeAppearance_Material3_ListItem_Last
                else -> MaterialR.style.ShapeAppearance_Material3_ListItem_Middle
            }

            val shapeAppearanceModel = ShapeAppearanceModel.builder(
                context,
                shapeResId,
                0
            ).build()

            binding.optionCard.shapeAppearanceModel = shapeAppearanceModel

            binding.optionTitle.text = activity.resources.getString(model.titleId)
            binding.optionDescription.text = activity.resources.getString(model.descriptionId)
            binding.optionIcon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    activity.resources,
                    model.iconId,
                    activity.theme
                )
            )

            if (!model.isEnabled.invoke()) {
                binding.optionTitle.alpha = 0.5f
                binding.optionDescription.alpha = 0.5f
                binding.optionIcon.alpha = 0.5f
            }

            viewLifecycle.lifecycleScope.launch {
                model.details.collect { updateOptionDetails(it) }
            }
            binding.optionDetail.marquee()

            binding.root.setOnClickListener { onClick(model) }
        }

        private fun onClick(model: HomeSetting) {
            model.onClick.invoke()
        }

        private fun updateOptionDetails(detailString: String) {
            if (detailString.isNotEmpty()) {
                binding.optionDetail.text = detailString
                binding.optionDetail.isVisible = true
            }
        }
    }
}