// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model.view

import org.dolphinemu.dolphinemu.features.settings.model.AdHocBooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class LogSwitchSetting(
    var key: String,
    title: CharSequence?,
    description: CharSequence?
) : SwitchSetting(
    AdHocBooleanSetting(
        Settings.FILE_LOGGER,
        Settings.SECTION_LOGGER_LOGS,
        key,
        false
    ),
    title,
    description
)
