// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import android.content.Context
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.settings.model.AbstractSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Most of them correspond to a single line in an INI file, but there are a few with multiple
 * analogues and a few with none (Headers, for example, do not correspond to anything on disk.)
 */
abstract class SettingsItem {
    val name: CharSequence
    val description: CharSequence

    /**
     * Base constructor.
     *
     * @param name        A text string to be displayed as this Setting's name.
     * @param description A text string to be displayed as this Setting's description.
     */
    constructor(name: CharSequence, description: CharSequence) {
        this.name = name
        this.description = description
    }

    /**
     * @param nameId        Resource ID for a text string to be displayed as this Setting's name.
     * @param descriptionId Resource ID for a text string to be displayed as this Setting's description.
     */
    constructor(context: Context, nameId: Int, descriptionId: Int) {
        name = if (nameId == 0) "" else context.getText(nameId)
        description = if (descriptionId == 0) "" else context.getText(descriptionId)
    }

    /**
     * Used by [SettingsAdapter]'s onCreateViewHolder()
     * method to determine which type of ViewHolder should be created.
     *
     * @return An integer (ideally, one of the constants defined in this file)
     */
    abstract val type: Int

    abstract val setting: AbstractSetting?

    val isOverridden: Boolean
        get() {
            val setting = setting
            return setting != null && setting.isOverridden
        }

    open val isEditable: Boolean
        get() {
            if (!NativeLibrary.IsRunning()) return true
            val setting = setting
            return setting != null && setting.isRuntimeEditable
        }

    private fun hasSetting(): Boolean {
        return setting != null
    }

    open fun canClear(): Boolean {
        return hasSetting()
    }

    open fun clear(settings: Settings) {
        setting!!.delete(settings)
    }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_INPUT_MAPPING_CONTROL = 5
        const val TYPE_STRING_SINGLE_CHOICE = 6
        const val TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS = 8
        const val TYPE_FILE_PICKER = 9
        const val TYPE_RUN_RUNNABLE = 10
        const val TYPE_STRING = 11
        const val TYPE_HYPERLINK_HEADER = 12
        const val TYPE_DATETIME_CHOICE = 13
    }
}
