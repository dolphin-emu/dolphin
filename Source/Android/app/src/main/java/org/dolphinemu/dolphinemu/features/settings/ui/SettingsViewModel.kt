// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import androidx.lifecycle.ViewModel
import org.dolphinemu.dolphinemu.features.settings.model.Settings

class SettingsViewModel : ViewModel() {
    val settings = Settings()
}
