// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.StorageLocationHelper

class StorageLocationDialog : DialogFragment() {
    interface Listener {
        fun launchStorageDirectoryPicker()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        isCancelable = false
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.storage_location_title)
            .setMessage(R.string.storage_location_description)
            .setPositiveButton(R.string.storage_internal) { _, _ ->
                StorageLocationHelper.setStorageConfigured(requireContext(), null)
                DirectoryInitialization.start(requireContext())
            }
            .setNegativeButton(R.string.storage_custom_folder) { _, _ ->
                val listener = requireActivity() as Listener
                listener.launchStorageDirectoryPicker()
            }
            .setCancelable(false)
            .create()
    }

    companion object {
        const val TAG = "StorageLocationDialog"
    }
}
