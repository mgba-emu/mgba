/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.fragments

import android.os.Bundle
import android.view.View
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.DialogLicenseBinding
import org.mgba_emu.mgba.model.License
import org.mgba_emu.mgba.utils.SerializableHelper.parcelable

class LicenseBottomSheetDialogFragment : BottomSheetDialogFragment(R.layout.dialog_license) {
    private var _binding: DialogLicenseBinding? = null
    private val binding get() = _binding!!

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = DialogLicenseBinding.bind(view)
        BottomSheetBehavior.from<View>(view.parent as View).state = BottomSheetBehavior.STATE_HALF_EXPANDED

        val license = requireArguments().parcelable<License>(LICENSE)!!

        binding.apply {
            title.setText(license.titleId)
            link.setText(license.linkId)
            copyright.setText(license.copyrightId)
            licenseText.setText(license.licenseId)
        }
    }

    companion object {
        const val TAG = "LicenseBottomSheetDialogFragment"

        const val LICENSE = "License"

        fun newInstance(
            license: License
        ): LicenseBottomSheetDialogFragment {
            val dialog = LicenseBottomSheetDialogFragment()
            val bundle = Bundle()
            bundle.putParcelable(LICENSE, license)
            dialog.arguments = bundle
            return dialog
        }
    }
}