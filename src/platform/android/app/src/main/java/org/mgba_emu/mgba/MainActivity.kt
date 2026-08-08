
package org.mgba_emu.mgba

import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import androidx.transition.Slide
import androidx.transition.TransitionManager
import org.mgba_emu.mgba.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.statusBarColor = ContextCompat.getColor(applicationContext, android.R.color.transparent)
        window.navigationBarColor = ContextCompat.getColor(applicationContext, android.R.color.transparent)
        val navHostFragment = supportFragmentManager.findFragmentById(R.id.nav_host_fragment) as NavHostFragment
        val navController = navHostFragment.navController
        binding.bottomNavigation.setupWithNavController(navController)
        navController.addOnDestinationChangedListener { _, destination, _ ->
            val slideTransition = Slide(Gravity.BOTTOM).apply {
                duration = 250
                addTarget(binding.bottomNavigation)
            }

            TransitionManager.beginDelayedTransition(
                binding.bottomNavigation.parent as ViewGroup,
                slideTransition
            )

            when (destination.id) {
                R.id.searchLocationsFragment -> {
                    binding.bottomNavigation.visibility = View.GONE
                }
                R.id.biosManagerFragment -> {
                    binding.bottomNavigation.visibility = View.GONE
                }
                else -> {
                    binding.bottomNavigation.visibility = View.VISIBLE
                }
            }
        }
    }
}