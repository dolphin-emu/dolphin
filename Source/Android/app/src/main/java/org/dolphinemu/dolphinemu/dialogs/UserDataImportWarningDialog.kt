// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.core.net.toUri
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.UserDataActivity
import org.dolphinemu.dolphinemu.model.TaskViewModel
import kotlin.system.exitProcess

class UserDataImportWarningDialog : DialogFragment() {
    private lateinit var taskViewModel: TaskViewModel

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        taskViewModel = ViewModelProvider(requireActivity())[TaskViewModel::class.java]

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setMessage(R.string.user_data_import_warning)
            .setNegativeButton(R.string.no) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
            .setPositiveButton(R.string.yes) { dialog: DialogInterface, _: Int ->
                dialog.dismiss()

                val taskArguments = Bundle()
                taskArguments.putInt(TaskDialog.KEY_TITLE, R.string.import_in_progress)
                taskArguments.putInt(TaskDialog.KEY_MESSAGE, R.string.do_not_close_app)
                taskArguments.putBoolean(TaskDialog.KEY_CANCELLABLE, false)

                taskViewModel.task = {
                    taskViewModel.setResult(
                        (requireActivity() as UserDataActivity).importUserData(
                            requireArguments().getString(KEY_URI_RESULT)!!.toUri()
                        )
                    )
                }

                taskViewModel.onResultDismiss = {
                    if (taskViewModel.mustRestartApp) {
                        exitProcess(0)
                    }
                }

                val taskDialog = TaskDialog()
                taskDialog.arguments = taskArguments
                taskDialog.show(requireActivity().supportFragmentManager, TaskDialog.TAG)
            }
        return dialog.create()
    }

    companion object {
        const val TAG = "UserDataImportWarningDialog"
        const val KEY_URI_RESULT = "uri"
    }
}
