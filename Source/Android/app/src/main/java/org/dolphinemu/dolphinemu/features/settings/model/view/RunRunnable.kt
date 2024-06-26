// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting

class RunRunnable(
    context: Context,
    titleId: Int,
    descriptionId: Int,
    val alertText: Int,
    val toastTextAfterRun: Int,
    private val worksDuringEmulation: Boolean,
    val runnable: Runnable
) : SettingsItem(context, titleId, descriptionId) {
    override val type: Int = TYPE_RUN_RUNNABLE

    override val setting: AbstractSetting? = null

    override val isEditable: Boolean
        get() = worksDuringEmulation || !NativeLibrary.IsRunning()
}
