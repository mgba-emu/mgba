
package org.mgba_emu.mgba.fragments

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.snackbar.Snackbar
import org.mgba_emu.mgba.EmulationActivity
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentGamesBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.GameCacheManager
import org.mgba_emu.mgba.utils.SearchLocationHelper
import org.mgba_emu.mgba.utils.applySafePadding
import org.mgba_emu.mgba.viewmodel.GamesViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class GamesFragment : Fragment() {

    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!
    private lateinit var gameAdapter: GameAdapter

    private val viewModel: GamesViewModel by viewModels()

    private val folderPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            if (SearchLocationHelper.isFolderExists(it)) {
                Snackbar.make(
                    binding.root,
                    "Folder already added to library",
                    Snackbar.LENGTH_SHORT
                ).setAnchorView(binding.add).show()
                return@let
            }
            requireContext().contentResolver.takePersistableUriPermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            SearchLocationHelper.saveFolderUri(it)
            viewModel.loadGames(listOf(it), clearExisting = false)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (_binding == null) _binding = FragmentGamesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        gameAdapter = GameAdapter { game ->
            launchEmulationActivity(game)
        }

        binding.gamesList.layoutManager = LinearLayoutManager(requireContext())
        binding.gamesList.adapter = gameAdapter
        binding.gamesList.applySafePadding()

        binding.add.setOnClickListener {
            folderPickerLauncher.launch(null)
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.gameList.collect { gamesList ->
                gameAdapter.submitList(gamesList)
            }
        }
    }

    private fun launchEmulationActivity(game: GameModel) {
        game.lastPlayed = System.currentTimeMillis()
        lifecycleScope.launch(Dispatchers.IO) {
            GameCacheManager.saveGame(game)
        }

        val intent = Intent(requireContext(), EmulationActivity::class.java).apply {
            putExtra("gameUri", game.uri.toString())
        }
        startActivity(intent)
    }

    override fun onResume() {
        super.onResume()
        viewModel.checkUpdatedList()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}