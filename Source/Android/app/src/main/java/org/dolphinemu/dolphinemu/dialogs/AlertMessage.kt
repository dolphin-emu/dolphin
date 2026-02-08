// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig

class AlertMessage : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val emulationActivity: EmulationActivity = NativeLibrary.getEmulationActivity()
            ?: throw IllegalStateException("EmulationActivity missing while showing alert")
        val args = requireArguments()
        val title = args.getString(ARG_TITLE).orEmpty()
        val message = args.getString(ARG_MESSAGE).orEmpty()
        val yesNo = args.getBoolean(ARG_YES_NO)
        val isWarning = args.getBoolean(ARG_IS_WARNING)
        isCancelable = false

        return MaterialAlertDialogBuilder(emulationActivity).setTitle(title).setMessage(message)
            .apply { configureButtons(yesNo, isWarning) }.create()
    }

    private fun MaterialAlertDialogBuilder.configureButtons(yesNo: Boolean, isWarning: Boolean) {
        if (yesNo) {
            setPositiveButton(android.R.string.yes) { dialog, _ ->
                dialog.releaseLock(result = true)
            }
            setNegativeButton(android.R.string.no) { dialog, _ ->
                dialog.releaseLock(result = false)
            }
        } else {
            setPositiveButton(android.R.string.ok) { dialog, _ ->
                dialog.releaseLock()
            }
        }

        if (isWarning) {
            setNeutralButton(R.string.ignore_warning_alert_messages) { dialog, _ ->
                BooleanSetting.MAIN_USE_PANIC_HANDLERS.setBoolean(
                    NativeConfig.LAYER_CURRENT, false
                )
                dialog.releaseLock()
            }
        }
    }

    private fun DialogInterface.releaseLock(result: Boolean? = null) {
        result?.let { alertResult = it }
        dismiss()
        NativeLibrary.NotifyAlertMessageLock()
    }

    companion object {
        private var alertResult = false
        private const val ARG_TITLE = "title"
        private const val ARG_MESSAGE = "message"
        private const val ARG_YES_NO = "yesNo"
        private const val ARG_IS_WARNING = "isWarning"

        fun newInstance(
            title: String, message: String, yesNo: Boolean, isWarning: Boolean
        ): AlertMessage {
            return AlertMessage().apply {
                arguments = Bundle().apply {
                    putString(ARG_TITLE, title)
                    putString(ARG_MESSAGE, message)
                    putBoolean(ARG_YES_NO, yesNo)
                    putBoolean(ARG_IS_WARNING, isWarning)
                }
            }
        }

        fun getAlertResult(): Boolean = alertResult
    }
}
