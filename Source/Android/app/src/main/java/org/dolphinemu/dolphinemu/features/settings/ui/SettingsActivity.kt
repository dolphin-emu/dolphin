// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.animation.PathInterpolator
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.appbar.CollapsingToolbarLayout
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivitySettingsBinding
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsFragment.Companion.newInstance
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable
import org.dolphinemu.dolphinemu.utils.ThemeHelper.enableScrollTint
import org.dolphinemu.dolphinemu.utils.ThemeHelper.setCorrectTheme
import org.dolphinemu.dolphinemu.utils.ThemeHelper.setTheme

class SettingsActivity : AppCompatActivity(), SettingsActivityView, ThemeProvider {
    private var presenter: SettingsActivityPresenter? = null
    private var dialog: AlertDialog? = null
    private var toolbarLayout: CollapsingToolbarLayout? = null
    private var binding: ActivitySettingsBinding? = null
    private lateinit var searchView: SearchView
    private var expandedToolbarHeight = 0
    private var toolbarStateGeneration = 0
    private var currentToolbarTitle: String? = null
    private var currentToolbarShowsHeadline = false
    private var currentToolbarShowsSearch = false
    private var currentToolbarShowsSearchMode = false
    override val settingsSearchQuery: String
        get() = presenter!!.settingsSearchQuery
    override val isSettingsSearchActive: Boolean
        get() = presenter!!.isSettingsSearchActive

    override var themeId: Int = 0
    override var isMappingAllDevices = false

    override val settings: Settings
        get() = ViewModelProvider(this)[SettingsViewModel::class.java].settings

    private val fragment: SettingsFragment?
        get() = supportFragmentManager.findFragmentByTag(FRAGMENT_TAG) as SettingsFragment?

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(this)
        enableEdgeToEdge()

        super.onCreate(savedInstanceState)

        // If we came here from the game list, we don't want to rescan when returning to the game list.
        // But if we came here after UserDataActivity restarted the app, we do want to rescan.
        if (savedInstanceState == null) {
            MainPresenter.skipRescanningLibrary()
        } else {
            isMappingAllDevices = savedInstanceState.getBoolean(KEY_MAPPING_ALL_DEVICES)
        }

        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding!!.root)

        val launcher = intent
        var gameID = launcher.getStringExtra(ARG_GAME_ID)
        if (gameID == null) gameID = ""
        val revision = launcher.getIntExtra(ARG_REVISION, 0)
        val isWii = launcher.getBooleanExtra(ARG_IS_WII, true)
        val menuTag = launcher.serializable<MenuTag>(ARG_MENU_TAG)

        presenter = SettingsActivityPresenter(this, settings)
        presenter!!.onCreate(savedInstanceState, menuTag, gameID, revision, isWii, this)
        toolbarLayout = binding!!.toolbarSettingsLayout
        expandedToolbarHeight = toolbarLayout!!.layoutParams.height
        setSupportActionBar(binding!!.toolbarSettings)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)
        setUpSettingsSearch()
        setUpBackNavigation()

        // TODO: Remove this when CollapsingToolbarLayouts are fixed by Google
        // https://github.com/material-components/material-components-android/issues/1310
        ViewCompat.setOnApplyWindowInsetsListener(toolbarLayout!!, null)
        setInsets()
        enableScrollTint(this, binding!!.toolbarSettings, binding!!.appbarSettings)
    }

    private fun setUpSettingsSearch() {
        searchView = binding!!.settingsSearch
        searchView.setQuery(settingsSearchQuery, false)
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean {
                searchView.clearFocus()
                return true
            }

            override fun onQueryTextChange(newText: String?): Boolean {
                presenter!!.onSettingsSearchQueryChanged(newText.orEmpty())
                return true
            }
        })
        binding!!.settingsSearchPreview.setOnClickListener { enterSettingsSearch() }
        binding!!.settingsSearchToolbar.setNavigationOnClickListener { exitSettingsSearch() }
    }

    private fun enterSettingsSearch() {
        if (!presenter!!.enterSettingsSearch()) {
            return
        }

        refreshToolbarState()
        val focusDelay =
            if (areSystemAnimationsEnabled()) SEARCH_FOCUS_DELAY_MS else 0L
        searchView.postDelayed({
            if (!isSettingsSearchActive) {
                return@postDelayed
            }
            searchView.requestFocus()
            WindowCompat.getInsetsController(window, searchView)
                .show(WindowInsetsCompat.Type.ime())
        }, focusDelay)
    }

    private fun exitSettingsSearch() {
        if (!presenter!!.exitSettingsSearch()) {
            return
        }

        searchView.setQuery("", false)
        searchView.clearFocus()
        WindowCompat.getInsetsController(window, searchView).hide(WindowInsetsCompat.Type.ime())
        refreshToolbarState()
    }

    private fun refreshToolbarState() {
        val title = currentToolbarTitle ?: getString(R.string.settings)
        setToolbarState(
            title,
            currentToolbarShowsHeadline,
            currentToolbarShowsSearch
        )
    }

    private fun setUpBackNavigation() {
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (supportFragmentManager.backStackEntryCount == 0 &&
                    isSettingsSearchActive
                ) {
                    exitSettingsSearch()
                    return
                }

                isEnabled = false
                onBackPressedDispatcher.onBackPressed()
                isEnabled = true
            }
        })
    }

    override fun onSaveInstanceState(outState: Bundle) {
        // Critical: If super method is not called, rotations will be busted.
        super.onSaveInstanceState(outState)
        outState.putBoolean(KEY_MAPPING_ALL_DEVICES, isMappingAllDevices)
        presenter!!.onSaveInstanceState(outState)
    }

    override fun onStart() {
        super.onStart()
        presenter!!.onStart()
    }

    override fun onResume() {
        setCorrectTheme(this)
        super.onResume()
    }

    override fun setTheme(themeId: Int) {
        super.setTheme(themeId)
        this.themeId = themeId
    }

    /**
     * If this is called, the user has left the settings screen (potentially through the
     * home button) and will expect their changes to be persisted.
     */
    override fun onStop() {
        super.onStop()
        presenter!!.onStop(isFinishing)
    }

    override fun onDestroy() {
        super.onDestroy()
        presenter!!.onDestroy()
    }

    override fun showSettingsFragment(
        menuTag: MenuTag, extras: Bundle?, addToStack: Boolean, gameId: String
    ) {
        replaceSettingsFragment(menuTag, extras, addToStack, gameId, false)
    }

    private fun replaceSettingsFragment(
        menuTag: MenuTag,
        extras: Bundle?,
        addToStack: Boolean,
        gameId: String,
        isSearchResult: Boolean
    ) {
        if (!addToStack && fragment != null) return
        val transaction = supportFragmentManager.beginTransaction()
        if (addToStack) {
            if (areSystemAnimationsEnabled()) {
                transaction.setCustomAnimations(
                    R.anim.anim_settings_fragment_in,
                    R.anim.anim_settings_fragment_out,
                    if (isSearchResult) R.anim.anim_settings_search_pop_in else 0,
                    if (isSearchResult) {
                        R.anim.anim_settings_search_pop_out
                    } else {
                        R.anim.anim_pop_settings_fragment_out
                    }
                )
            }
            transaction.addToBackStack(null)
        }
        transaction.replace(
            R.id.frame_content_settings, newInstance(menuTag, gameId, extras), FRAGMENT_TAG
        )
        transaction.commit()
    }

    override fun showDialogFragment(fragment: DialogFragment) {
        fragment.show(supportFragmentManager, FRAGMENT_DIALOG_TAG)
    }

    override fun showSearchResult(
        menuTag: MenuTag, settingPosition: Int, gameId: String, extras: Bundle?
    ) {
        val navigationExtras = extras?.let(::Bundle) ?: Bundle()
        navigationExtras.putInt(
            SettingsFragment.ARGUMENT_SCROLL_TO_SETTING_POSITION, settingPosition
        )
        replaceSettingsFragment(menuTag, navigationExtras, true, gameId, true)
    }

    private fun areSystemAnimationsEnabled(): Boolean {
        val duration = android.provider.Settings.Global.getFloat(
            contentResolver, android.provider.Settings.Global.ANIMATOR_DURATION_SCALE, 1f
        )
        val transition = android.provider.Settings.Global.getFloat(
            contentResolver, android.provider.Settings.Global.TRANSITION_ANIMATION_SCALE, 1f
        )
        return duration != 0f && transition != 0f
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        ControllerInterface.dispatchKeyEvent(event)
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        ControllerInterface.dispatchGenericMotionEvent(event)
        return super.dispatchGenericMotionEvent(event)
    }

    override fun showLoading() {
        if (dialog == null) {
            dialog = MaterialAlertDialogBuilder(this).setTitle(getString(R.string.load_settings))
                .setView(R.layout.dialog_indeterminate_progress).create()
        }
        dialog!!.show()
    }

    override fun hideLoading() {
        dialog!!.dismiss()
    }

    override fun showGameIniJunkDeletionQuestion() {
        MaterialAlertDialogBuilder(this).setTitle(getString(R.string.game_ini_junk_title))
            .setMessage(getString(R.string.game_ini_junk_question))
            .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int -> presenter!!.clearGameSettings() }
            .setNegativeButton(R.string.no, null).show()
    }

    override fun onSettingsFileLoaded(settings: Settings) {
        val fragment: SettingsFragmentView? = fragment
        fragment?.onSettingsFileLoaded(settings)
    }

    override fun showToastMessage(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    override fun onSettingChanged() {
        presenter!!.onSettingChanged()
    }

    override fun onControllerSettingsChanged() {
        fragment!!.onControllerSettingsChanged()
    }

    override fun onMenuTagAction(menuTag: MenuTag, value: Int) {
        presenter!!.onMenuTagAction(menuTag, value)
    }

    override fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean {
        return presenter!!.hasMenuTagActionForValue(menuTag, value)
    }

    override fun getMenuTagActionExtras(menuTag: MenuTag, value: Int): Bundle? {
        return presenter!!.getMenuTagActionExtras(menuTag, value)
    }

    override fun filterSettings(query: String) {
        fragment?.filterSettings(query)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressedDispatcher.onBackPressed()
        return true
    }

    override fun setToolbarState(title: String, showHeadline: Boolean, showSearch: Boolean) {
        val appBar = binding!!.appbarSettings
        val generation = ++toolbarStateGeneration
        val showSearchMode = showSearch && isSettingsSearchActive
        val stateChanged =
            currentToolbarTitle != title || currentToolbarShowsHeadline != showHeadline || currentToolbarShowsSearch != showSearch || currentToolbarShowsSearchMode != showSearchMode
        appBar.animate().cancel()

        if (!appBar.isLaidOut || !stateChanged) {
            applyToolbarState(title, showHeadline, showSearch)
            appBar.alpha = 1f
            return
        }

        if (!showSearch) {
            searchView.clearFocus()
        }

        appBar.animate().alpha(0f).setDuration(APP_BAR_FADE_OUT_DURATION_MS)
            .setInterpolator(APP_BAR_FADE_OUT_INTERPOLATOR).withEndAction {
                if (generation != toolbarStateGeneration) {
                    return@withEndAction
                }

                applyToolbarState(title, showHeadline, showSearch)
                appBar.post {
                    if (generation != toolbarStateGeneration) {
                        return@post
                    }

                    appBar.animate().alpha(1f).setDuration(APP_BAR_FADE_IN_DURATION_MS)
                        .setInterpolator(APP_BAR_FADE_IN_INTERPOLATOR).start()
                }
            }.start()
    }

    private fun applyToolbarState(title: String, showHeadline: Boolean, showSearch: Boolean) {
        val showSearchMode = showSearch && isSettingsSearchActive
        toolbarLayout!!.isTitleEnabled = showHeadline
        supportActionBar!!.title = title
        if (showHeadline) {
            toolbarLayout!!.title = title
        }
        toolbarLayout!!.layoutParams = toolbarLayout!!.layoutParams.apply {
            height = if (showHeadline) {
                expandedToolbarHeight
            } else {
                binding!!.toolbarSettings.layoutParams.height
            }
        }
        toolbarLayout!!.visibility = if (showSearchMode) View.GONE else View.VISIBLE
        binding!!.settingsSearchContainer.visibility =
            if (showSearch && !showSearchMode) View.VISIBLE else View.GONE
        binding!!.settingsSearchModeContainer.visibility =
            if (showSearchMode) View.VISIBLE else View.GONE
        currentToolbarTitle = title
        currentToolbarShowsHeadline = showHeadline
        currentToolbarShowsSearch = showSearch
        currentToolbarShowsSearchMode = showSearchMode
    }

    override fun setOldControllerSettingsWarningVisibility(visible: Boolean): Int {
        // We use INVISIBLE instead of GONE to avoid getting a stale height for the return value
        binding!!.oldControllerSettingsWarning.visibility =
            if (visible) View.VISIBLE else View.INVISIBLE
        return if (visible) binding!!.oldControllerSettingsWarning.height else 0
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding!!.appbarSettings) { _: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.insetAppBar(insets, binding!!.appbarSettings)

            binding!!.frameContentSettings.setPadding(insets.left, 0, insets.right, 0)
            val textPadding = resources.getDimensionPixelSize(R.dimen.spacing_large)
            binding!!.oldControllerSettingsWarning.setPadding(
                textPadding + insets.left,
                textPadding,
                textPadding + insets.right,
                textPadding + insets.bottom
            )

            InsetsHelper.applyNavbarWorkaround(insets.bottom, binding!!.workaroundView)

            windowInsets
        }
    }

    companion object {
        private const val ARG_MENU_TAG = "menu_tag"
        private const val ARG_GAME_ID = "game_id"
        private const val ARG_REVISION = "revision"
        private const val ARG_IS_WII = "is_wii"
        private const val KEY_MAPPING_ALL_DEVICES = "all_devices"
        private const val FRAGMENT_TAG = "settings"
        private const val FRAGMENT_DIALOG_TAG = "settings_dialog"
        private const val APP_BAR_FADE_OUT_DURATION_MS = 90L
        private const val APP_BAR_FADE_IN_DURATION_MS = 180L
        private const val SEARCH_FOCUS_DELAY_MS =
            APP_BAR_FADE_OUT_DURATION_MS + APP_BAR_FADE_IN_DURATION_MS
        private val APP_BAR_FADE_OUT_INTERPOLATOR = PathInterpolator(0.4f, 0f, 1f, 1f)
        private val APP_BAR_FADE_IN_INTERPOLATOR = PathInterpolator(0f, 0f, 0.2f, 1f)

        @JvmStatic
        fun launch(
            context: Context, menuTag: MenuTag?, gameId: String?, revision: Int, isWii: Boolean
        ) {
            val settings = Intent(context, SettingsActivity::class.java)
            settings.putExtra(ARG_MENU_TAG, menuTag)
            settings.putExtra(ARG_GAME_ID, gameId)
            settings.putExtra(ARG_REVISION, revision)
            settings.putExtra(ARG_IS_WII, isWii)
            context.startActivity(settings)
        }

        @JvmStatic
        fun launch(context: Context, menuTag: MenuTag?) {
            val settings = Intent(context, SettingsActivity::class.java)
            settings.putExtra(ARG_MENU_TAG, menuTag)
            settings.putExtra(
                ARG_IS_WII, !NativeLibrary.IsRunning() || NativeLibrary.IsEmulatingWii()
            )
            context.startActivity(settings)
        }
    }
}
