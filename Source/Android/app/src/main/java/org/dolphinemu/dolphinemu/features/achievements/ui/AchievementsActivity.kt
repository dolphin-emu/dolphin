// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.achievements.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.color.MaterialColors
import com.google.android.material.elevation.ElevationOverlayProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivityAchievementsBinding
import org.dolphinemu.dolphinemu.features.achievements.model.AchievementProgressViewModel
import org.dolphinemu.dolphinemu.ui.TwoPaneOnBackPressedCallback
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class AchievementsActivity : AppCompatActivity() {
    private lateinit var viewModel: AchievementProgressViewModel

    private lateinit var binding: ActivityAchievementsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)
        enableEdgeToEdge()

        super.onCreate(savedInstanceState)

        MainPresenter.skipRescanningLibrary()

        title = getString(R.string.achievements_progress)

        viewModel = ViewModelProvider(this)[AchievementProgressViewModel::class.java]
        viewModel.load()

        binding = ActivityAchievementsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        onBackPressedDispatcher.addCallback(
            this,
            TwoPaneOnBackPressedCallback(binding.slidingPaneLayout)
        )

        setSupportActionBar(binding.toolbarAchievements)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        @ColorInt val color =
            ElevationOverlayProvider(binding.toolbarAchievements.context).compositeOverlay(
                MaterialColors.getColor(binding.toolbarAchievements, R.attr.colorSurface),
                resources.getDimensionPixelSize(R.dimen.elevated_app_bar).toFloat()
            )
        binding.toolbarAchievements.setBackgroundColor(color)
        ThemeHelper.setStatusBarColor(this, color)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    companion object {
        @JvmStatic
        fun launch(
            context: Context
        ) {
            val intent = Intent(context, AchievementsActivity::class.java)
            context.startActivity(intent)
        }
    }
}
