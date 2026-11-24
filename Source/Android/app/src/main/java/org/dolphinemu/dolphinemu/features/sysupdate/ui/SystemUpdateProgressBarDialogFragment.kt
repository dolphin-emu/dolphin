// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui

import android.app.Dialog
import android.os.Bundle
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.ui.progressbardialog.ProgressBarDialogFragment

class SystemUpdateProgressBarDialogFragment : ProgressBarDialogFragment<SystemUpdateViewModel>() {
    override fun showResult() {
        val progressBarFragment = SystemUpdateResultFragment()
        progressBarFragment.show(parentFragmentManager, SystemUpdateResultFragment.TAG)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel = ViewModelProvider(requireActivity())[SystemUpdateViewModel::class.java]

        titleData.value = getString(R.string.updating)

        viewModel.titleIdData.observe(this) { titleId: Long ->
            messageData.value = getString(R.string.updating_message, titleId)
        }

        return super.onCreateDialog(savedInstanceState)
    }

    companion object {
        const val TAG = "SystemUpdateProgressBarDialogFragment"
    }
}
