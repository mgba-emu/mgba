
package org.mgba_emu.mgba.fragments

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import org.mgba_emu.mgba.adapters.FolderAdapter
import org.mgba_emu.mgba.databinding.FragmentSearchLocationsBinding
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.applySafePadding

class SearchLocationsFragment : Fragment() {
    private var _binding: FragmentSearchLocationsBinding? = null
    private val binding get() = _binding!!

    private lateinit var adapter: FolderAdapter
    private val folderList = mutableListOf<Uri>()

    private val dirPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            val takeFlags: Int = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            requireContext().contentResolver.takePersistableUriPermission(it, takeFlags)
            addFolder(it)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View? {
        if (_binding == null) _binding = FragmentSearchLocationsBinding.inflate(inflater, container, false)
        binding.root.applySafePadding()
        loadFolders()
        binding.folderList.layoutManager = LinearLayoutManager(requireContext())
        adapter = FolderAdapter { uri ->
            removeFolder(uri)
        }
        binding.folderList.adapter = adapter
        adapter.submitList(folderList.toList())

        binding.addFolder.setOnClickListener {
            dirPickerLauncher.launch(null)
        }

        return binding.root
    }

    private fun loadFolders() {
        folderList.clear()
        folderList.addAll(SearchLocationHelper.getGameFolders())
    }

    private fun addFolder(uri: Uri) {
        if (!folderList.contains(uri)) {
            folderList.add(uri)
            SearchLocationHelper.saveFolderUri(uri)
            adapter.submitList(folderList.toList())
        }
    }

    private fun removeFolder(uri: Uri) {
        folderList.remove(uri)
        SearchLocationHelper.removeFolder(uri)
        adapter.submitList(folderList.toList())

        try {
            requireContext().contentResolver.releasePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}