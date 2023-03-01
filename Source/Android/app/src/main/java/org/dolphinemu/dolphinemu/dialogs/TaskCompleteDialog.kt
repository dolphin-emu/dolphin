// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.model.TaskViewModel

class TaskCompleteDialog : DialogFragment() {
    private lateinit var viewModel: TaskViewModel

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel = ViewModelProvider(requireActivity())[TaskViewModel::class.java]

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setMessage(requireArguments().getInt(KEY_MESSAGE))
            .setPositiveButton(android.R.string.ok, null)
        return dialog.create()
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        if (viewModel.onResultDismiss != null)
            viewModel.onResultDismiss!!.invoke()

        viewModel.clear()
    }

    companion object {
        const val TAG = "TaskCompleteDialog"
        const val KEY_MESSAGE = "message"
    }
}
