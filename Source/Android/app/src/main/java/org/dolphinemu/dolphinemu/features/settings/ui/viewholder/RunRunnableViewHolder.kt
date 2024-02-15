// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder

import android.content.Context
import android.content.DialogInterface
import android.view.View
import android.widget.Toast
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding
import org.dolphinemu.dolphinemu.features.settings.model.view.RunRunnable
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter

class RunRunnableViewHolder(
    private val mBinding: ListItemSettingBinding,
    adapter: SettingsAdapter,
    private val mContext: Context
) : SettingViewHolder(mBinding.getRoot(), adapter) {
    private lateinit var setting: RunRunnable

    override val item: SettingsItem
        get() = setting

    override fun bind(item: SettingsItem) {
        setting = item as RunRunnable

        mBinding.textSettingName.text = setting.name
        mBinding.textSettingDescription.text = setting.description

        setStyle(mBinding.textSettingName, setting)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            showNotRuntimeEditableError()
            return
        }

        val alertTextID = setting.alertText

        if (alertTextID > 0) {
            MaterialAlertDialogBuilder(mContext)
                .setTitle(setting.name)
                .setMessage(alertTextID)
                .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int ->
                    runRunnable()
                    dialog.dismiss()
                }
                .setNegativeButton(R.string.cancel) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
                .show()
        } else {
            runRunnable()
        }
    }

    private fun runRunnable() {
        setting.runnable.run()

        if (setting.toastTextAfterRun > 0) {
            Toast.makeText(
                mContext,
                mContext.getString(setting.toastTextAfterRun),
                Toast.LENGTH_SHORT
            ).show()
        }
    }
}
