// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.TaskViewModel

class TaskDialog : DialogFragment() {
    private lateinit var viewModel: TaskViewModel

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        viewModel = ViewModelProvider(requireActivity())[TaskViewModel::class.java]

        val dialogBuilder = MaterialAlertDialogBuilder(requireContext())
            .setTitle(requireArguments().getInt(KEY_TITLE))
            .setView(R.layout.dialog_indeterminate_progress)
        if (requireArguments().getBoolean(KEY_CANCELLABLE)) {
            dialogBuilder.setCancelable(true)
                .setNegativeButton(android.R.string.cancel) { dialog: DialogInterface, _: Int ->
                    viewModel.cancelled = true
                    dialog.dismiss()
                }
        }

        val dialog = dialogBuilder.create()
        dialog.setCanceledOnTouchOutside(false)

        val progressMessage = requireArguments().getInt(KEY_MESSAGE)
        if (progressMessage != 0) dialog.setMessage(resources.getString(progressMessage))

        viewModel.isComplete.observe(this) { complete: Boolean ->
            if (complete && viewModel.result.value != null) {
                dialog.dismiss()
                val notificationArguments = Bundle()
                notificationArguments.putInt(
                    TaskCompleteDialog.KEY_MESSAGE,
                    viewModel.result.value!!
                )

                val taskCompleteDialog = TaskCompleteDialog()
                taskCompleteDialog.arguments = notificationArguments
                taskCompleteDialog.show(
                    requireActivity().supportFragmentManager,
                    TaskCompleteDialog.TAG
                )
            }
        }

        viewModel.runTask()
        return dialog
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        viewModel.cancelled = true
    }

    companion object {
        const val TAG = "TaskDialog"
        const val KEY_TITLE = "title"
        const val KEY_MESSAGE = "message"
        const val KEY_CANCELLABLE = "cancellable"
    }
}
