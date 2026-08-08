
package org.mgba_emu.mgba.adapters

import android.net.Uri
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import java.net.URLDecoder
import org.mgba_emu.mgba.databinding.ItemFolderBinding

class FolderAdapter(
    private val onDelete: (Uri) -> Unit
) : ListAdapter<Uri, FolderAdapter.FolderViewHolder>(FolderDiffCallback()) {

    class FolderViewHolder(val binding: ItemFolderBinding) : RecyclerView.ViewHolder(binding.root) {
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): FolderViewHolder {
        val binding = ItemFolderBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return FolderViewHolder(binding)
    }

    override fun onBindViewHolder(holder: FolderViewHolder, position: Int) {
        val rawUri = getItem(position)
        val cleanPath = try {
            URLDecoder.decode(rawUri.toString(), "UTF-8").substringAfterLast(":")
        } catch (_: Exception) {
            rawUri.toString()
        }

        holder.binding.path.text = cleanPath
        holder.binding.delete.setOnClickListener { onDelete(rawUri) }
    }

    class FolderDiffCallback : DiffUtil.ItemCallback<Uri>() {
        override fun areItemsTheSame(oldItem: Uri, newItem: Uri): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: Uri, newItem: Uri): Boolean {
            return oldItem == newItem
        }
    }
}