// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import android.content.Intent
import androidx.activity.result.ActivityResultLauncher
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting

class DirectoryPicker(
    context: Context,
    setting: AbstractStringSetting,
    titleId: Int,
    descriptionId: Int,
    launcher: ActivityResultLauncher<Intent>,
    defaultPathRelativeToUserDirectory: String?
) : FilePicker(context, setting, titleId, descriptionId, launcher, defaultPathRelativeToUserDirectory) {
    override val type: Int = TYPE_DIRECTORY_PICKER
}
