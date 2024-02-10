// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.color.MaterialColors
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.ActivityRiivolutionBootBinding
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class RiivolutionBootActivity : AppCompatActivity() {
    private var patches: RiivolutionPatches? = null
    private lateinit var binding: ActivityRiivolutionBootBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityRiivolutionBootBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        val path = intent.getStringExtra(ARG_GAME_PATH)
        val gameId = intent.getStringExtra(ARG_GAME_ID)
        val revision = intent.getIntExtra(ARG_REVISION, -1)
        val discNumber = intent.getIntExtra(ARG_DISC_NUMBER, -1)

        var loadPath = StringSetting.MAIN_LOAD_PATH.string
        if (loadPath.isEmpty()) loadPath = DirectoryInitialization.getUserDirectory() + "/Load"

        binding.textSdRoot.text = getString(R.string.riivolution_sd_root, "$loadPath/Riivolution")
        binding.buttonStart.setOnClickListener {
            if (patches != null) patches!!.saveConfig()
            EmulationActivity.launch(this, path!!, true)
        }

        lifecycleScope.launch {
            withContext(Dispatchers.IO) {
                val patches = RiivolutionPatches(gameId!!, revision, discNumber)
                patches.loadConfig()
                withContext(Dispatchers.Main) {
                    populateList(patches)
                }
            }
        }

        binding.toolbarRiivolution.title = getString(R.string.riivolution_riivolution)
        setSupportActionBar(binding.toolbarRiivolution)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        setInsets()
        ThemeHelper.enableScrollTint(this, binding.toolbarRiivolution, binding.appbarRiivolution)
    }

    override fun onStop() {
        super.onStop()
        if (patches != null) patches!!.saveConfig()
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    private fun populateList(patches: RiivolutionPatches) {
        this.patches = patches
        binding.recyclerView.adapter = RiivolutionAdapter(this, patches)
        binding.recyclerView.layoutManager = LinearLayoutManager(this)
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.appbarRiivolution) { v: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.insetAppBar(insets, binding.appbarRiivolution)

            binding.scrollViewRiivolution.setPadding(insets.left, 0, insets.right, insets.bottom)

            InsetsHelper.applyNavbarWorkaround(insets.bottom, binding.workaroundView)
            ThemeHelper.setNavigationBarColor(
                this,
                MaterialColors.getColor(binding.appbarRiivolution, R.attr.colorSurface)
            )

            windowInsets
        }
    }

    companion object {
        private const val ARG_GAME_PATH = "game_path"
        private const val ARG_GAME_ID = "game_id"
        private const val ARG_REVISION = "revision"
        private const val ARG_DISC_NUMBER = "disc_number"

        @JvmStatic
        fun launch(
            context: Context,
            gamePath: String?,
            gameId: String?,
            revision: Int,
            discNumber: Int
        ) {
            val launcher = Intent(context, RiivolutionBootActivity::class.java)
            launcher.putExtra(ARG_GAME_PATH, gamePath)
            launcher.putExtra(ARG_GAME_ID, gameId)
            launcher.putExtra(ARG_REVISION, revision)
            launcher.putExtra(ARG_DISC_NUMBER, discNumber)
            context.startActivity(launcher)
        }
    }
}
