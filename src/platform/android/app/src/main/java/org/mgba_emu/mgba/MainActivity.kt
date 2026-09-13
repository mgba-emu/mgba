/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba

import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import androidx.transition.Slide
import androidx.transition.TransitionManager
import com.google.android.material.color.MaterialColors
import com.google.android.material.navigation.NavigationBarView
import org.mgba_emu.mgba.databinding.ActivityMainBinding
import org.mgba_emu.mgba.utils.ThemeHelper

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        window.statusBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)
        window.navigationBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)

        val navHostFragment = supportFragmentManager.findFragmentById(R.id.nav_host_fragment) as NavHostFragment
        val navController = navHostFragment.navController
        (binding.navigationView as NavigationBarView).setupWithNavController(navController)

        navController.addOnDestinationChangedListener { _, destination, _ ->
            val slideTransition = Slide(Gravity.BOTTOM).apply {
                duration = 250
                addTarget(binding.navigationView)
            }

            TransitionManager.beginDelayedTransition(
                binding.navigationView.parent as ViewGroup,
                slideTransition
            )

            when (destination.id) {
                R.id.searchLocationsFragment -> {
                    binding.navigationView.visibility = View.GONE
                }
                R.id.biosManagerFragment -> {
                    binding.navigationView.visibility = View.GONE
                }
                R.id.gameAboutFragment -> {
                    binding.navigationView.visibility = View.GONE
                }
                else -> {
                    binding.navigationView.visibility = View.VISIBLE
                }
            }
        }

        binding.statusBarShade.setBackgroundColor(
            ThemeHelper.getColorWithOpacity(
                MaterialColors.getColor(
                    binding.root,
                    com.google.android.material.R.attr.colorSurfaceDim
                ),
                0.9f
            )
        )

        binding.statusBarShade.visibility = View.VISIBLE

        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val mlpStatusShade = binding.statusBarShade.layoutParams as ViewGroup.MarginLayoutParams
            mlpStatusShade.height = insets.top
            binding.statusBarShade.layoutParams = mlpStatusShade

            windowInsets
        }
    }
}