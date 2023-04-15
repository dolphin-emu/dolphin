// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.app.Activity
import android.content.DialogInterface
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import java.util.function.Supplier

object ThreadUtil {
    @JvmStatic
    @JvmOverloads
    fun runOnThreadAndShowResult(
        activity: Activity,
        progressTitle: Int,
        progressMessage: Int,
        f: Supplier<String?>,
        onResultDismiss: DialogInterface.OnDismissListener? = null
    ) {
        val resources = activity.resources
        val progressDialog = MaterialAlertDialogBuilder(activity)
            .setTitle(progressTitle)
            .setView(R.layout.dialog_indeterminate_progress)
            .setCancelable(false)
            .create()
        if (progressMessage != 0) progressDialog.setMessage(resources.getString(progressMessage))
        progressDialog.show()
        Thread({
            val result = f.get()
            activity.runOnUiThread {
                progressDialog.dismiss()
                if (result != null) {
                    MaterialAlertDialogBuilder(activity)
                        .setMessage(result)
                        .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
                        .setOnDismissListener(onResultDismiss)
                        .show()
                }
            }
        }, resources.getString(progressTitle)).start()
    }
}
