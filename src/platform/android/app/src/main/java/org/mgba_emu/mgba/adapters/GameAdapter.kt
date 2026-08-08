package org.mgba_emu.mgba.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import coil3.load
import coil3.request.crossfade
import coil3.request.error
import coil3.request.fallback
import coil3.request.transformations
import coil3.transform.RoundedCornersTransformation
import org.mgba_emu.mgba.databinding.ItemGameBinding
import org.mgba_emu.mgba.model.GameModel

class GameAdapter(
    private val onGameClick: (GameModel) -> Unit
) : ListAdapter<GameModel, GameAdapter.GameViewHolder>(GameDiffCallback()) {

    inner class GameViewHolder(val binding: ItemGameBinding) : RecyclerView.ViewHolder(binding.root) {
        init {
            binding.root.setOnClickListener {
                val position = bindingAdapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    onGameClick(getItem(position))
                }
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val binding = ItemGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return GameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        val game = getItem(position)

        holder.binding.title.text = game.title ?: game.fileName
        holder.binding.title.isSelected = true
        holder.binding.version.text = game.version ?: "Version: --"
        holder.binding.icon.load(game.iconUrl ?: "") {
            crossfade(true)
            fallback(android.R.drawable.ic_media_play)
            error(android.R.drawable.ic_media_play)
            transformations(RoundedCornersTransformation(16f))
        }
    }

    class GameDiffCallback : DiffUtil.ItemCallback<GameModel>() {
        override fun areItemsTheSame(oldItem: GameModel, newItem: GameModel): Boolean {
            return oldItem.uri == newItem.uri
        }

        override fun areContentsTheSame(oldItem: GameModel, newItem: GameModel): Boolean {
            return oldItem == newItem
        }
    }
}
