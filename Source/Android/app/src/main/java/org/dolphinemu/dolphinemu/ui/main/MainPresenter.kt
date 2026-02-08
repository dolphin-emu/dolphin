// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import android.content.DialogInterface
import android.content.Intent
import androidx.activity.ComponentActivity
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.BuildConfig
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemMenuNotInstalledDialogFragment
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateProgressBarDialogFragment
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateViewModel
import org.dolphinemu.dolphinemu.fragments.AboutDialogFragment
import org.dolphinemu.dolphinemu.model.GameFileCache
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.BooleanSupplier
import org.dolphinemu.dolphinemu.utils.CompletableFuture
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.PermissionsHandler
import org.dolphinemu.dolphinemu.utils.ThreadUtil
import org.dolphinemu.dolphinemu.utils.WiiUtils
import java.util.Arrays
import java.util.concurrent.ExecutionException

class MainPresenter(private val mainView: MainView, private val activity: FragmentActivity) {
    private var dirToAdd: String? = null

    fun onCreate() {
        // Ask the user to grant write permission if relevant and not already granted
        if (DirectoryInitialization.isWaitingForWriteAccess(activity))
            PermissionsHandler.requestWritePermission(activity)

        val versionName = BuildConfig.VERSION_NAME
        mainView.setVersionString(versionName)

        GameFileCacheManager.getGameFiles().observe(activity) { mainView.showGames() }
        val refreshObserver =
            Observer<Boolean> { _: Boolean? -> mainView.setRefreshing(GameFileCacheManager.isLoadingOrRescanning()) }
        GameFileCacheManager.isLoading().observe(activity, refreshObserver)
        GameFileCacheManager.isRescanning().observe(activity, refreshObserver)
    }

    fun onFabClick() {
        AfterDirectoryInitializationRunner().runWithLifecycle(activity) { mainView.launchFileListActivity() }
    }

    fun handleOptionSelection(itemId: Int, activity: ComponentActivity): Boolean =
        when (itemId) {
            R.id.menu_settings -> {
                mainView.launchSettingsActivity(MenuTag.SETTINGS)
                true
            }

            R.id.menu_grid_options -> {
                mainView.showGridOptions()
                true
            }

            R.id.menu_refresh -> {
                mainView.setRefreshing(true)
                GameFileCacheManager.startRescan()
                true
            }

            R.id.button_add_directory -> {
                AfterDirectoryInitializationRunner().runWithLifecycle(activity) { mainView.launchFileListActivity() }
                true
            }

            R.id.menu_open_file -> {
                mainView.launchOpenFileActivity(REQUEST_GAME_FILE)
                true
            }

            R.id.menu_load_wii_system_menu -> {
                launchWiiSystemMenu()
                true
            }

            R.id.menu_online_system_update -> {
                AfterDirectoryInitializationRunner().runWithLifecycle(activity) { launchOnlineUpdate() }
                true
            }

            R.id.menu_install_wad -> {
                AfterDirectoryInitializationRunner().runWithLifecycle(
                    activity
                ) { mainView.launchOpenFileActivity(REQUEST_WAD_FILE) }
                true
            }

            R.id.menu_import_wii_save -> {
                AfterDirectoryInitializationRunner().runWithLifecycle(
                    activity
                ) { mainView.launchOpenFileActivity(REQUEST_WII_SAVE_FILE) }
                true
            }

            R.id.menu_import_nand_backup -> {
                AfterDirectoryInitializationRunner().runWithLifecycle(
                    activity
                ) { mainView.launchOpenFileActivity(REQUEST_NAND_BIN_FILE) }
                true
            }

            R.id.menu_about -> {
                showAboutDialog()
                false
            }

            else -> false
        }

    fun onResume() {
        if (dirToAdd != null) {
            GameFileCache.addGameFolder(dirToAdd!!)
            dirToAdd = null
        }

        if (shouldRescanLibrary) {
            GameFileCacheManager.startRescan()
        }

        shouldRescanLibrary = true
    }

    /**
     * Called when a selection is made using the legacy folder picker.
     */
    fun onDirectorySelected(dir: String?) {
        dirToAdd = dir
    }

    /**
     * Called when a selection is made using the Storage Access Framework folder picker.
     */
    fun onDirectorySelected(result: Intent) {
        var uri = result.data!!

        val recursive = BooleanSetting.MAIN_RECURSIVE_ISO_PATHS.boolean
        val childNames = ContentHandler.getChildNames(uri, recursive)
        if (Arrays.stream(childNames).noneMatch {
                FileBrowserHelper.GAME_EXTENSIONS
                    .contains(FileBrowserHelper.getExtension(it, false))
            }) {
            MaterialAlertDialogBuilder(activity)
                .setMessage(
                    activity.getString(
                        R.string.wrong_file_extension_in_directory,
                        FileBrowserHelper.setToSortedDelimitedString(FileBrowserHelper.GAME_EXTENSIONS)
                    )
                )
                .setPositiveButton(android.R.string.ok, null)
                .show()
        }

        val contentResolver = activity.contentResolver
        val canonicalizedUri = contentResolver.canonicalize(uri)
        if (canonicalizedUri != null)
            uri = canonicalizedUri

        val takeFlags = result.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
        activity.contentResolver.takePersistableUriPermission(uri, takeFlags)

        dirToAdd = uri.toString()
    }

    fun installWAD(path: String?) {
        ThreadUtil.runOnThreadAndShowResult(
            activity,
            R.string.import_in_progress,
            0,
            {
                val success = WiiUtils.installWAD(path!!)
                val message =
                    if (success) R.string.wad_install_success else R.string.wad_install_failure
                activity.getString(message)
            })
    }

    fun importWiiSave(path: String?) {
        val canOverwriteFuture = CompletableFuture<Boolean>()
        ThreadUtil.runOnThreadAndShowResult(
            activity,
            R.string.import_in_progress,
            0,
            {
                val canOverwrite = BooleanSupplier {
                    activity.runOnUiThread {
                        MaterialAlertDialogBuilder(activity)
                            .setMessage(R.string.wii_save_exists)
                            .setCancelable(false)
                            .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                                canOverwriteFuture.complete(true)
                            }
                            .setNegativeButton(R.string.no) { _: DialogInterface?, _: Int ->
                                canOverwriteFuture.complete(false)
                            }
                            .show()
                    }
                    try {
                        return@BooleanSupplier canOverwriteFuture.get()
                    } catch (e: ExecutionException) {
                        // Shouldn't happen
                        throw RuntimeException(e)
                    } catch (e: InterruptedException) {
                        throw RuntimeException(e)
                    }
                }

                val message: Int = when (WiiUtils.importWiiSave(path!!, canOverwrite)) {
                    WiiUtils.RESULT_SUCCESS -> R.string.wii_save_import_success
                    WiiUtils.RESULT_CORRUPTED_SOURCE -> R.string.wii_save_import_corruped_source
                    WiiUtils.RESULT_TITLE_MISSING -> R.string.wii_save_import_title_missing
                    WiiUtils.RESULT_CANCELLED -> return@runOnThreadAndShowResult null
                    else -> R.string.wii_save_import_error
                }
                activity.resources.getString(message)
            })
    }

    fun importNANDBin(path: String?) {
        MaterialAlertDialogBuilder(activity)
            .setMessage(R.string.nand_import_warning)
            .setNegativeButton(R.string.no) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
            .setPositiveButton(R.string.yes) { dialog: DialogInterface, _: Int ->
                dialog.dismiss()
                ThreadUtil.runOnThreadAndShowResult(
                    activity,
                    R.string.import_in_progress,
                    R.string.do_not_close_app,
                    {
                        // ImportNANDBin unfortunately doesn't provide any result value...
                        // It does however show a panic alert if something goes wrong.
                        WiiUtils.importNANDBin(path!!)
                        null
                    })
            }
            .show()
    }

    private fun launchOnlineUpdate() {
        if (WiiUtils.isSystemMenuInstalled()) {
            val viewModel = ViewModelProvider(activity)[SystemUpdateViewModel::class.java]
            viewModel.region = -1
            launchUpdateProgressBarFragment(activity)
        } else {
            SystemMenuNotInstalledDialogFragment().show(
                activity.supportFragmentManager,
                SystemMenuNotInstalledDialogFragment.TAG
            )
        }
    }

    private fun launchWiiSystemMenu() {
        AfterDirectoryInitializationRunner().runWithLifecycle(activity) {
            if (WiiUtils.isSystemMenuInstalled()) {
                EmulationActivity.launchSystemMenu(activity)
            } else {
                SystemMenuNotInstalledDialogFragment().show(
                    activity.supportFragmentManager,
                    SystemMenuNotInstalledDialogFragment.TAG
                )
            }
        }
    }

    private fun showAboutDialog() {
        AboutDialogFragment().show(activity.supportFragmentManager, AboutDialogFragment.TAG)
    }

    companion object {
        const val REQUEST_DIRECTORY = 1
        const val REQUEST_GAME_FILE = 2
        const val REQUEST_SD_FILE = 3
        const val REQUEST_WAD_FILE = 4
        const val REQUEST_WII_SAVE_FILE = 5
        const val REQUEST_NAND_BIN_FILE = 6
        const val REQUEST_GPU_DRIVER = 7

        private var shouldRescanLibrary = true

        @JvmStatic
        fun skipRescanningLibrary() {
            shouldRescanLibrary = false
        }

        @JvmStatic
        fun launchDiscUpdate(path: String, activity: FragmentActivity) {
            val viewModel = ViewModelProvider(activity)[SystemUpdateViewModel::class.java]
            viewModel.discPath = path
            launchUpdateProgressBarFragment(activity)
        }

        private fun launchUpdateProgressBarFragment(activity: FragmentActivity) {
            val progressBarFragment = SystemUpdateProgressBarDialogFragment()
            progressBarFragment
                .show(activity.supportFragmentManager, SystemUpdateProgressBarDialogFragment.TAG)
            progressBarFragment.isCancelable = false
        }
    }
}
