// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R

class OnlineUpdateRegionSelectDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val items = arrayOf(
            getString(R.string.country_europe),
            getString(R.string.country_japan),
            getString(R.string.country_korea),
            getString(R.string.country_usa)
        )
        val checkedItem = -1
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.region_select_title)
            .setSingleChoiceItems(items, checkedItem) { _: DialogInterface?, which: Int ->
                val viewModel =
                    ViewModelProvider(requireActivity())[SystemUpdateViewModel::class.java]
                viewModel.region = which
                SystemUpdateProgressBarDialogFragment().show(
                    parentFragmentManager,
                    SystemUpdateProgressBarDialogFragment.TAG
                )
                dismiss()
            }
            .create()
    }

    companion object {
        const val TAG = "OnlineUpdateRegionSelectDialogFragment"
    }
}
