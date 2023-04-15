// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R

class SystemMenuNotInstalledDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.system_menu_not_installed_title)
            .setMessage(R.string.system_menu_not_installed_message)
            .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int ->
                OnlineUpdateRegionSelectDialogFragment().show(
                    parentFragmentManager,
                    OnlineUpdateRegionSelectDialogFragment.TAG
                )
                dismiss()
            }
            .setNegativeButton(R.string.no) { _: DialogInterface?, _: Int -> dismiss() }
            .create()
    }

    companion object {
        const val TAG = "SystemMenuNotInstalledDialogFragment"
    }
}
