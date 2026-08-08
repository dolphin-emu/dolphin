// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.AbstractStringSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

open class CameraChoiceSetting(
    private val context: Context,
    setting: AbstractStringSetting
) : StringSingleChoiceSetting(
    context,
    setting,
    R.string.camera_device,
    0,
    emptyArray(),
    emptyArray()
) {
    init {
        refreshChoicesAndValues()
    }

    override fun refreshChoicesAndValues() {
        val cameras = NativeLibrary.getCameras()

        val choiceList = mutableListOf<String>()
        val valueList = mutableListOf<String>()

        choiceList.add(context.getString(R.string.camera_automatic))
        valueList.add("")

        for (camera in cameras) {
            choiceList.add(camera)
            valueList.add(camera)
        }

        choices = choiceList.toTypedArray()
        values = valueList.toTypedArray()
    }
}
