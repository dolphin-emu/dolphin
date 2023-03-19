// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.Menu
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.appbar.CollapsingToolbarLayout
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivitySettingsBinding
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsFragment.Companion.newInstance
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.InsetsHelper
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable
import org.dolphinemu.dolphinemu.utils.ThemeHelper.enableScrollTint
import org.dolphinemu.dolphinemu.utils.ThemeHelper.setNavigationBarColor
import org.dolphinemu.dolphinemu.utils.ThemeHelper.setTheme

class SettingsActivity : AppCompatActivity(), SettingsActivityView {
    private var presenter: SettingsActivityPresenter? = null
    private var dialog: AlertDialog? = null
    private var toolbarLayout: CollapsingToolbarLayout? = null
    private var binding: ActivitySettingsBinding? = null

    override var isMappingAllDevices = false

    override val settings: Settings
        get() = ViewModelProvider(this)[SettingsViewModel::class.java].settings

    private val fragment: SettingsFragment?
        get() = supportFragmentManager.findFragmentByTag(FRAGMENT_TAG) as SettingsFragment?

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(this)

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

        WindowCompat.setDecorFitsSystemWindows(window, false)

        val launcher = intent
        var gameID = launcher.getStringExtra(ARG_GAME_ID)
        if (gameID == null) gameID = ""
        val revision = launcher.getIntExtra(ARG_REVISION, 0)
        val isWii = launcher.getBooleanExtra(ARG_IS_WII, true)
        val menuTag = launcher.serializable<MenuTag>(ARG_MENU_TAG)

        presenter = SettingsActivityPresenter(this, settings)
        presenter!!.onCreate(savedInstanceState, menuTag, gameID, revision, isWii, this)
        toolbarLayout = binding!!.toolbarSettingsLayout
        setSupportActionBar(binding!!.toolbarSettings)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        // TODO: Remove this when CollapsingToolbarLayouts are fixed by Google
        // https://github.com/material-components/material-components-android/issues/1310
        ViewCompat.setOnApplyWindowInsetsListener(toolbarLayout!!, null)
        setInsets()
        enableScrollTint(this, binding!!.toolbarSettings, binding!!.appbarSettings)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val inflater = menuInflater
        inflater.inflate(R.menu.menu_settings, menu)
        return true
    }

    override fun onSaveInstanceState(outState: Bundle) {
        // Critical: If super method is not called, rotations will be busted.
        super.onSaveInstanceState(outState)
        presenter!!.saveState(outState)
        outState.putBoolean(KEY_MAPPING_ALL_DEVICES, isMappingAllDevices)
    }

    override fun onStart() {
        super.onStart()
        presenter!!.onStart()
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
        menuTag: MenuTag,
        extras: Bundle?,
        addToStack: Boolean,
        gameId: String
    ) {
        if (!addToStack && fragment != null) return
        val transaction = supportFragmentManager.beginTransaction()
        if (addToStack) {
            if (areSystemAnimationsEnabled()) {
                transaction.setCustomAnimations(
                    R.anim.anim_settings_fragment_in,
                    R.anim.anim_settings_fragment_out,
                    0,
                    R.anim.anim_pop_settings_fragment_out
                )
            }
            transaction.addToBackStack(null)
        }
        transaction.replace(
            R.id.frame_content_settings,
            newInstance(menuTag, gameId, extras), FRAGMENT_TAG
        )
        transaction.commit()
    }

    override fun showDialogFragment(fragment: DialogFragment) {
        fragment.show(supportFragmentManager, FRAGMENT_DIALOG_TAG)
    }

    private fun areSystemAnimationsEnabled(): Boolean {
        val duration = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.ANIMATOR_DURATION_SCALE,
            1f
        )
        val transition = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.TRANSITION_ANIMATION_SCALE,
            1f
        )
        return duration != 0f && transition != 0f
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, result: Intent?) {
        super.onActivityResult(requestCode, resultCode, result)

        // If the user picked a file, as opposed to just backing out.
        if (resultCode == RESULT_OK) {
            if (requestCode != MainPresenter.REQUEST_DIRECTORY) {
                val uri = canonicalizeIfPossible(result!!.data!!)
                val validExtensions: Set<String> =
                    if (requestCode == MainPresenter.REQUEST_GAME_FILE) FileBrowserHelper.GAME_EXTENSIONS else FileBrowserHelper.RAW_EXTENSION
                var flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
                if (requestCode != MainPresenter.REQUEST_GAME_FILE) flags =
                    flags or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                val takeFlags = flags and result.flags
                FileBrowserHelper.runAfterExtensionCheck(this, uri, validExtensions) {
                    contentResolver.takePersistableUriPermission(uri, takeFlags)
                    fragment!!.adapter!!.onFilePickerConfirmation(uri.toString())
                }
            } else {
                val path = FileBrowserHelper.getSelectedPath(result)
                fragment!!.adapter!!.onFilePickerConfirmation(path!!)
            }
        }
    }

    private fun canonicalizeIfPossible(uri: Uri): Uri {
        val canonicalizedUri = contentResolver.canonicalize(uri)
        return canonicalizedUri ?: uri
    }

    override fun showLoading() {
        if (dialog == null) {
            dialog = MaterialAlertDialogBuilder(this)
                .setTitle(getString(R.string.load_settings))
                .setView(R.layout.dialog_indeterminate_progress)
                .create()
        }
        dialog!!.show()
    }

    override fun hideLoading() {
        dialog!!.dismiss()
    }

    override fun showGameIniJunkDeletionQuestion() {
        MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.game_ini_junk_title))
            .setMessage(getString(R.string.game_ini_junk_question))
            .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int -> presenter!!.clearGameSettings() }
            .setNegativeButton(R.string.no, null)
            .show()
    }

    override fun onSettingsFileLoaded(settings: Settings) {
        val fragment: SettingsFragmentView? = fragment
        fragment?.onSettingsFileLoaded(settings)
    }

    override fun onSettingsFileNotFound() {
        val fragment: SettingsFragmentView? = fragment
        fragment?.loadDefaultSettings()
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

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    override fun setToolbarTitle(title: String) {
        binding!!.toolbarSettingsLayout.title = title
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
            setNavigationBarColor(
                this,
                MaterialColors.getColor(binding!!.appbarSettings, R.attr.colorSurface)
            )

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

        @JvmStatic
        fun launch(
            context: Context,
            menuTag: MenuTag?,
            gameId: String?,
            revision: Int,
            isWii: Boolean
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
                ARG_IS_WII,
                !NativeLibrary.IsRunning() || NativeLibrary.IsEmulatingWii()
            )
            context.startActivity(settings)
        }
    }
}
