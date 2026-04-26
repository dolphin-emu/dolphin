// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Environment
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.nononsenseapps.filepicker.FilePickerActivity
import com.nononsenseapps.filepicker.Utils
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.CustomFilePickerActivity
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import java.io.File

object FileBrowserHelper {
    @JvmField
    val GAME_EXTENSIONS: HashSet<String> = hashSetOf(
        "gcm",
        "tgc",
        "bin",
        "iso",
        "ciso",
        "gcz",
        "wbfs",
        "wia",
        "rvz",
        "nfs",
        "wad",
        "dol",
        "elf",
        "json"
    )

    @JvmField
    val GAME_LIKE_EXTENSIONS: HashSet<String> = HashSet(GAME_EXTENSIONS).apply {
        add("dff")
    }

    @JvmField
    val GBA_ROM_EXTENSIONS: HashSet<String> = hashSetOf(
        "gba", "gbc", "gb", "agb", "mb", "rom", "bin"
    )

    @JvmField
    val BIN_EXTENSION: HashSet<String> = hashSetOf("bin")

    @JvmField
    val RAW_EXTENSION: HashSet<String> = hashSetOf("raw")

    @JvmField
    val WAD_EXTENSION: HashSet<String> = hashSetOf("wad")

    @JvmStatic
    fun createDirectoryPickerIntent(
        activity: FragmentActivity, extensions: HashSet<String>
    ): Intent {
        val intent = Intent(activity, CustomFilePickerActivity::class.java)

        intent.putExtra(FilePickerActivity.EXTRA_ALLOW_MULTIPLE, false)
        intent.putExtra(FilePickerActivity.EXTRA_ALLOW_CREATE_DIR, false)
        intent.putExtra(FilePickerActivity.EXTRA_MODE, FilePickerActivity.MODE_DIR)
        intent.putExtra(
            FilePickerActivity.EXTRA_START_PATH, Environment.getExternalStorageDirectory().path
        )
        intent.putExtra(CustomFilePickerActivity.EXTRA_EXTENSIONS, extensions)

        return intent
    }

    @JvmStatic
    fun getSelectedPath(result: Intent): String? {
        // Use the provided utility method to parse the result
        val files = Utils.getSelectedFilesFromResult(result)
        if (files.isNotEmpty()) {
            val file = Utils.getFileForUri(files[0])
            return file.absolutePath
        }

        return null
    }

    @JvmStatic
    fun isPathEmptyOrValid(path: StringSetting): Boolean {
        return isPathEmptyOrValid(path.string)
    }

    /**
     * Returns true if at least one of the following applies:
     *
     * 1. The input is empty.
     * 2. The input is something which is not a content URI.
     * 3. The input is a content URI that points to a file that exists and we're allowed to access.
     */
    @JvmStatic
    fun isPathEmptyOrValid(path: String): Boolean {
        return !ContentHandler.isContentUri(path) || ContentHandler.exists(path)
    }

    @JvmStatic
    fun runAfterExtensionCheck(
        context: Context, uri: Uri, validExtensions: Set<String>, runnable: Runnable
    ) {
        var extension: String? = null

        val path = uri.lastPathSegment
        if (path != null) {
            extension = getExtension(File(path).name, false)
        }

        if (extension == null) {
            extension = getExtension(ContentHandler.getDisplayName(uri), false)
        }

        if (extension != null && validExtensions.contains(extension)) {
            runnable.run()
            return
        }

        val message = if (extension == null) {
            context.getString(R.string.no_file_extension)
        } else {
            val messageId = if (validExtensions.size == 1) {
                R.string.wrong_file_extension_single
            } else {
                R.string.wrong_file_extension_multiple
            }

            context.getString(
                messageId, extension, setToSortedDelimitedString(validExtensions)
            )
        }

        MaterialAlertDialogBuilder(context).setMessage(message)
            .setPositiveButton(R.string.yes) { _, _ -> runnable.run() }
            .setNegativeButton(R.string.no, null).setCancelable(false).show()
    }

    @JvmStatic
    fun getExtension(fileName: String?, includeDot: Boolean): String? {
        if (fileName == null) {
            return null
        }

        val dotIndex = fileName.lastIndexOf(".")
        if (dotIndex == -1) {
            return null
        }
        return fileName.substring(dotIndex + if (includeDot) 0 else 1)
    }

    @JvmStatic
    fun setToSortedDelimitedString(set: Set<String>): String {
        val list = ArrayList(set)
        list.sort()
        return list.joinToString(", ")
    }
}
