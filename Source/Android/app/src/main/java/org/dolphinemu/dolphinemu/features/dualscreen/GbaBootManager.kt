// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.content.DialogInterface
import androidx.core.net.toUri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.features.settings.model.SharedPreferenceStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.ui.platform.Platform
import org.dolphinemu.dolphinemu.utils.ContentHandler
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import java.io.File

object GbaBootManager {
    fun isGbaRomPath(path: String): Boolean {
        val extension = FileBrowserHelper.getExtension(displayNameFor(path), false)
        return extension != null &&
                FileBrowserHelper.GBA_ROM_EXTENSIONS.contains(extension.lowercase())
    }

    fun titleFor(path: String): String {
        val displayName = displayNameFor(path)
        return displayName.substringBeforeLast('.', displayName)
    }

    fun displayNameFor(path: String): String {
        if (ContentHandler.isContentUri(path)) {
            val displayName = ContentHandler.getDisplayName(path)
            if (!displayName.isNullOrEmpty()) {
                return displayName
            }

            val lastPathSegment = path.toUri().lastPathSegment
            if (!lastPathSegment.isNullOrEmpty()) {
                return lastPathSegment.substringAfterLast('/')
            }
        }

        val fileName = File(path).name
        return fileName.ifEmpty { path.substringAfterLast('/') }
    }

    fun launchGameBoyPlayer(activity: FragmentActivity, gbaPath: String) {
        val bootPath = SharedPreferenceStringSetting.MAIN_ANDROID_GB_PLAYER_BOOT_PATH.string
        if (bootPath.isEmpty() || !isPathValid(bootPath)) {
            MaterialAlertDialogBuilder(activity)
                .setMessage(R.string.gb_player_boot_path_required)
                .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                    SettingsActivity.launch(activity, MenuTag.CONFIG_GAME_CUBE)
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
            return
        }

        StringSetting.MAIN_GB_PLAYER_ROM.setString(NativeConfig.LAYER_BASE, gbaPath)
        NativeConfig.save(NativeConfig.LAYER_BASE)
        EmulationActivity.launch(activity, bootPath, false)
    }

    fun continueAfterBiosCheck(
        activity: FragmentActivity,
        fromIntent: Boolean,
        filePaths: Array<String>?,
        continueCallback: Runnable
    ) {
        if (filePaths.isNullOrEmpty()) {
            continueCallback.run()
            return
        }

        activity.lifecycleScope.launch {
            val shouldWarn = withContext(Dispatchers.IO) {
                GameFile.parse(filePaths[0])?.getPlatform() == Platform.GAMECUBE.toInt() &&
                        GbaLinkSettings.shouldWarnAboutMissingBios()
            }

            if (!shouldWarn) {
                continueCallback.run()
                return@launch
            }

            MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.gba_bios_required_title)
                .setMessage(R.string.gba_bios_required_message)
                .setPositiveButton(R.string.configure_bios) { _: DialogInterface?, _: Int ->
                    if (fromIntent) {
                        activity.finish()
                    }
                    SettingsActivity.launch(activity, MenuTag.CONFIG_GAME_CUBE)
                }
                .setNeutralButton(R.string.continue_anyway) { _: DialogInterface?, _: Int ->
                    continueCallback.run()
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        }
    }

    private fun isPathValid(path: String): Boolean {
        return if (ContentHandler.isContentUri(path)) {
            ContentHandler.exists(path)
        } else {
            File(path).exists()
        }
    }
}
