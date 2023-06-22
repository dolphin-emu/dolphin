// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import com.google.android.material.color.MaterialColors
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivityConvertBinding
import org.dolphinemu.dolphinemu.fragments.ConvertFragment
import org.dolphinemu.dolphinemu.fragments.ConvertFragment.Companion.newInstance
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class ConvertActivity : AppCompatActivity() {
    private lateinit var binding: ActivityConvertBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityConvertBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        val path = intent.getStringExtra(ARG_GAME_PATH)

        var fragment = supportFragmentManager
            .findFragmentById(R.id.fragment_convert) as ConvertFragment?
        if (fragment == null) {
            fragment = newInstance(path!!)
            supportFragmentManager.beginTransaction().add(R.id.fragment_convert, fragment).commit()
        }

        binding.toolbarConvertLayout.title = getString(R.string.convert_convert)
        setSupportActionBar(binding.toolbarConvert)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        setInsets()
        ThemeHelper.enableScrollTint(this, binding.toolbarConvert, binding.appbarConvert)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.appbarConvert) { _: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.insetAppBar(insets, binding.appbarConvert)

            binding.scrollViewConvert.setPadding(insets.left, 0, insets.right, insets.bottom)

            InsetsHelper.applyNavbarWorkaround(insets.bottom, binding.workaroundView)
            ThemeHelper.setNavigationBarColor(
                this,
                MaterialColors.getColor(binding.appbarConvert, R.attr.colorSurface)
            )

            windowInsets
        }
    }

    companion object {
        private const val ARG_GAME_PATH = "game_path"

        @JvmStatic
        fun launch(context: Context, gamePath: String) {
            val launcher = Intent(context, ConvertActivity::class.java)
            launcher.putExtra(ARG_GAME_PATH, gamePath)
            context.startActivity(launcher)
        }
    }
}
