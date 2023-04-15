// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.slidingpanelayout.widget.SlidingPaneLayout
import androidx.slidingpanelayout.widget.SlidingPaneLayout.PanelSlideListener
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.elevation.ElevationOverlayProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivityCheatsBinding
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel
import org.dolphinemu.dolphinemu.features.cheats.model.GeckoCheat.Companion.downloadCodes
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.ui.TwoPaneOnBackPressedCallback
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.ThemeHelper

class CheatsActivity : AppCompatActivity(), PanelSlideListener {
    private var gameId: String? = null
    private var gameTdbId: String? = null
    private var revision = 0
    private var isWii = false
    private lateinit var viewModel: CheatsViewModel

    private var cheatListLastFocus: View? = null
    private var cheatDetailsLastFocus: View? = null

    private lateinit var binding: ActivityCheatsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        MainPresenter.skipRescanningLibrary()

        gameId = intent.getStringExtra(ARG_GAME_ID)
        gameTdbId = intent.getStringExtra(ARG_GAMETDB_ID)
        revision = intent.getIntExtra(ARG_REVISION, 0)
        isWii = intent.getBooleanExtra(ARG_IS_WII, true)

        title = getString(R.string.cheats_with_game_id, gameId)

        viewModel = ViewModelProvider(this)[CheatsViewModel::class.java]
        viewModel.load(gameId!!, revision)

        binding = ActivityCheatsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        cheatListLastFocus = binding.cheatList
        cheatDetailsLastFocus = binding.cheatDetails

        binding.slidingPaneLayout.addPanelSlideListener(this)

        onBackPressedDispatcher.addCallback(
            this,
            TwoPaneOnBackPressedCallback(binding.slidingPaneLayout)
        )

        viewModel.selectedCheat.observe(this) { selectedCheat: Cheat? ->
            onSelectedCheatChanged(
                selectedCheat
            )
        }
        onSelectedCheatChanged(viewModel.selectedCheat.value)

        viewModel.openDetailsViewEvent.observe(this) { open: Boolean -> openDetailsView(open) }

        setSupportActionBar(binding.toolbarCheats)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        setInsets()

        @ColorInt val color =
            ElevationOverlayProvider(binding.toolbarCheats.context).compositeOverlay(
                MaterialColors.getColor(binding.toolbarCheats, R.attr.colorSurface),
                resources.getDimensionPixelSize(R.dimen.elevated_app_bar).toFloat()
            )
        binding.toolbarCheats.setBackgroundColor(color)
        ThemeHelper.setStatusBarColor(this, color)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val inflater = menuInflater
        inflater.inflate(R.menu.menu_settings, menu)
        return true
    }

    override fun onStop() {
        super.onStop()
        viewModel.saveIfNeeded(gameId!!, revision)
    }

    override fun onPanelSlide(panel: View, slideOffset: Float) {}
    override fun onPanelOpened(panel: View) {
        val rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL
        cheatDetailsLastFocus!!.requestFocus(if (rtl) View.FOCUS_LEFT else View.FOCUS_RIGHT)
    }

    override fun onPanelClosed(panel: View) {
        val rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL
        cheatListLastFocus!!.requestFocus(if (rtl) View.FOCUS_RIGHT else View.FOCUS_LEFT)
    }

    private fun onSelectedCheatChanged(selectedCheat: Cheat?) {
        val cheatSelected = selectedCheat != null
        if (!cheatSelected && binding.slidingPaneLayout.isOpen) binding.slidingPaneLayout.close()

        binding.slidingPaneLayout.lockMode =
            if (cheatSelected) SlidingPaneLayout.LOCK_MODE_UNLOCKED else SlidingPaneLayout.LOCK_MODE_LOCKED_CLOSED
    }

    fun onListViewFocusChange(hasFocus: Boolean) {
        if (hasFocus) {
            cheatListLastFocus = binding.cheatList.findFocus()
            if (cheatListLastFocus == null) throw NullPointerException()
            binding.slidingPaneLayout.close()
        }
    }

    fun onDetailsViewFocusChange(hasFocus: Boolean) {
        if (hasFocus) {
            cheatDetailsLastFocus = binding.cheatDetails.findFocus()
            if (cheatDetailsLastFocus == null) throw NullPointerException()
            binding.slidingPaneLayout.open()
        }
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    private fun openDetailsView(open: Boolean) {
        if (open) binding.slidingPaneLayout.open()
    }

    fun loadGameSpecificSettings(): Settings {
        val settings = Settings()
        settings.loadSettings(gameId!!, revision, isWii)
        return settings
    }

    fun downloadGeckoCodes() {
        val progressDialog = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.cheats_downloading)
            .setView(R.layout.dialog_indeterminate_progress)
            .setCancelable(false)
            .show()

        lifecycleScope.launch {
            withContext(Dispatchers.IO) {
                val codes = downloadCodes(gameTdbId!!)
                withContext(Dispatchers.Main) {
                    progressDialog.dismiss()
                    if (codes == null) {
                        MaterialAlertDialogBuilder(binding.root.context)
                            .setMessage(getString(R.string.cheats_download_failed))
                            .setPositiveButton(R.string.ok, null)
                            .show()
                    } else if (codes.isEmpty()) {
                        MaterialAlertDialogBuilder(binding.root.context)
                            .setMessage(getString(R.string.cheats_download_empty))
                            .setPositiveButton(R.string.ok, null)
                            .show()
                    } else {
                        val cheatsAdded = viewModel.addDownloadedGeckoCodes(codes)
                        val message =
                            getString(R.string.cheats_download_succeeded, codes.size, cheatsAdded)
                        MaterialAlertDialogBuilder(binding.root.context)
                            .setMessage(message)
                            .setPositiveButton(R.string.ok, null)
                            .show()
                    }
                }
            }
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.appbarCheats) { _: View?, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val keyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime())

            InsetsHelper.insetAppBar(barInsets, binding.appbarCheats)
            binding.slidingPaneLayout.setPadding(barInsets.left, 0, barInsets.right, 0)

            // Set keyboard insets if the system supports smooth keyboard animations
            val mlpDetails = binding.cheatDetails.layoutParams as MarginLayoutParams
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
                if (keyboardInsets.bottom > 0) {
                    mlpDetails.bottomMargin = keyboardInsets.bottom
                } else {
                    mlpDetails.bottomMargin = barInsets.bottom
                }
            } else {
                if (mlpDetails.bottomMargin == 0) {
                    mlpDetails.bottomMargin = barInsets.bottom
                }
            }
            binding.cheatDetails.layoutParams = mlpDetails

            InsetsHelper.applyNavbarWorkaround(barInsets.bottom, binding.workaroundView)
            ThemeHelper.setNavigationBarColor(
                this,
                MaterialColors.getColor(binding.appbarCheats, R.attr.colorSurface)
            )

            windowInsets
        }

        // Update the layout for every frame that the keyboard animates in
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ViewCompat.setWindowInsetsAnimationCallback(
                binding.cheatDetails,
                object : WindowInsetsAnimationCompat.Callback(DISPATCH_MODE_STOP) {
                    var keyboardInsets = 0
                    var barInsets = 0
                    override fun onProgress(
                        insets: WindowInsetsCompat,
                        runningAnimations: List<WindowInsetsAnimationCompat>
                    ): WindowInsetsCompat {
                        val mlpDetails = binding.cheatDetails.layoutParams as MarginLayoutParams
                        keyboardInsets = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom
                        barInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom
                        mlpDetails.bottomMargin = keyboardInsets.coerceAtLeast(barInsets)
                        binding.cheatDetails.layoutParams = mlpDetails
                        return insets
                    }
                })
        }
    }

    companion object {
        private const val ARG_GAME_ID = "game_id"
        private const val ARG_GAMETDB_ID = "gametdb_id"
        private const val ARG_REVISION = "revision"
        private const val ARG_IS_WII = "is_wii"

        @JvmStatic
        fun launch(
            context: Context,
            gameId: String,
            gameTdbId: String,
            revision: Int,
            isWii: Boolean
        ) {
            val intent = Intent(context, CheatsActivity::class.java)
            intent.putExtra(ARG_GAME_ID, gameId)
            intent.putExtra(ARG_GAMETDB_ID, gameTdbId)
            intent.putExtra(ARG_REVISION, revision)
            intent.putExtra(ARG_IS_WII, isWii)
            context.startActivity(intent)
        }

        @JvmStatic
        fun setOnFocusChangeListenerRecursively(
            view: View,
            listener: View.OnFocusChangeListener?
        ) {
            view.onFocusChangeListener = listener
            if (view is ViewGroup) {
                for (i in 0 until view.childCount) {
                    val child = view.getChildAt(i)
                    setOnFocusChangeListenerRecursively(child, listener)
                }
            }
        }
    }
}
