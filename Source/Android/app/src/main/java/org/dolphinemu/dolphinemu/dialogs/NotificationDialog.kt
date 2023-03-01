// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder

class NotificationDialog : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setMessage(requireArguments().getInt(KEY_MESSAGE))
            .setPositiveButton(android.R.string.ok, null)
        return dialog.create()
    }

    companion object {
        const val TAG = "NotificationDialog"
        const val KEY_MESSAGE = "message"
    }
}
