
package org.mgba_emu.mgba.fragments

import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import com.google.android.material.R
import com.google.android.material.color.MaterialColors
import org.mgba_emu.mgba.core.Platform
import org.mgba_emu.mgba.databinding.FragmentBiosManagerBinding
import org.mgba_emu.mgba.utils.BiosStore
import org.mgba_emu.mgba.utils.applySafePadding
import java.io.ByteArrayOutputStream

class BiosManagerFragment : Fragment() {

    private var _binding: FragmentBiosManagerBinding? = null
    private val binding get() = _binding!!

    private var pendingPlatform: Platform? = null


    private val filePickerLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        uri?.let {
            val platform = pendingPlatform ?: return@let
            importBiosFromUri(it, platform)
        }
        pendingPlatform = null
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (_binding == null) _binding = FragmentBiosManagerBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        binding.root.applySafePadding()
        binding.toolbar.setNavigationOnClickListener {
            findNavController().navigateUp()
        }

        setupClickListeners()
        updatePlatformUi(Platform.GB, BiosStore.has(Platform.GB), filename = "gb.bin")
        updatePlatformUi(Platform.GBC, BiosStore.has(Platform.GBC), filename = "gbc.bin")
        updatePlatformUi(Platform.SGB, BiosStore.has(Platform.SGB), filename = "sgb.bin")
        updatePlatformUi(Platform.GBA, BiosStore.has(Platform.GBA), filename = "gba.bin")
    }

    private fun setupClickListeners() {
        binding.btnImportGb.setOnClickListener { launchFilePicker(Platform.GB) }
        binding.btnImportGbc.setOnClickListener { launchFilePicker(Platform.GBC) }
        binding.btnImportSgb.setOnClickListener { launchFilePicker(Platform.SGB) }
        binding.btnImportGba.setOnClickListener { launchFilePicker(Platform.GBA) }
    }

    private fun launchFilePicker(platform: Platform) {
        pendingPlatform = platform
        filePickerLauncher.launch(arrayOf("*/*"))
    }

    private fun updatePlatformUi(platform: Platform, isLoaded: Boolean, filename: String = "") {
        val (statusView, button) = when (platform) {
            Platform.GB -> binding.statusGb to binding.btnImportGb
            Platform.GBC -> binding.statusGbc to binding.btnImportGbc
            Platform.SGB -> binding.statusSgb to binding.btnImportSgb
            Platform.GBA -> binding.statusGba to binding.btnImportGba
            else -> null to null
        }

        if (statusView == null || button == null) return

        if (isLoaded) {
            statusView.text = "Loaded: $filename"
            button.text = "Replace"
        } else {
            statusView.text = "Missing"
            button.text = "Import"
        }
    }

    private fun importBiosFromUri(uri: Uri, platform: Platform) {
        try {
            val bytes = requireContext().contentResolver.openInputStream(uri)?.use { input ->
                val output = ByteArrayOutputStream()
                input.copyTo(output)
                output.toByteArray()
            } ?: run {
                Toast.makeText(requireContext(), "Could not open BIOS file", Toast.LENGTH_SHORT)
                    .show()
                return
            }

            if (platform == Platform.GBA && bytes.size != GBA_BIOS_SIZE_BYTES) {
                Toast.makeText(
                    requireContext(),
                    "This doesn't look like a GBA BIOS (expected $GBA_BIOS_SIZE_BYTES bytes, got ${bytes.size})",
                    Toast.LENGTH_LONG
                ).show()
                return
            }

            val ok = BiosStore.import(platform, bytes)
            if (ok) updatePlatformUi(platform, isLoaded = true, filename = "${platform.name.lowercase()}.bin")
            Toast.makeText(
                requireContext(),
                if (ok) "BIOS imported, will be used for the next ROM you load" else "Failed to import BIOS",
                Toast.LENGTH_LONG
            ).show()
        } catch (e: Exception) {
            Toast.makeText(
                requireContext(),
                "Error reading BIOS file: ${e.message}",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        private const val GBA_BIOS_SIZE_BYTES = 16384
    }
}