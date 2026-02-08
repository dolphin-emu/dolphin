// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.app.Activity
import android.content.Intent
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper

class SettingsActivityResultLaunchers(
    private val fragment: Fragment, private val getAdapter: () -> SettingsAdapter?
) {
    val requestDirectory = fragment.registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result: ActivityResult ->
        val intent = result.data
        if (result.resultCode == Activity.RESULT_OK && intent != null) {
            val path = FileBrowserHelper.getSelectedPath(intent)
            if (path != null) {
                getAdapter()?.onFilePickerConfirmation(path)
            }
        }
    }

    val requestGameFile = fragment.registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result: ActivityResult ->
        onFileResult(
            result,
            FileBrowserHelper.GAME_EXTENSIONS,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )
    }

    val requestRawFile = fragment.registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result: ActivityResult ->
        onFileResult(
            result,
            FileBrowserHelper.RAW_EXTENSION,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
    }

    private fun onFileResult(result: ActivityResult, validExtensions: Set<String>, flags: Int) {
        val intent = result.data
        val uri = intent?.data
        val context = fragment.requireContext()
        if (result.resultCode == Activity.RESULT_OK && uri != null) {
            val canonicalizedUri = context.contentResolver.canonicalize(uri) ?: uri
            val takeFlags = flags and intent.flags
            FileBrowserHelper.runAfterExtensionCheck(context, canonicalizedUri, validExtensions) {
                context.contentResolver.takePersistableUriPermission(canonicalizedUri, takeFlags)
                getAdapter()?.onFilePickerConfirmation(canonicalizedUri.toString())
            }
        }
    }
}
