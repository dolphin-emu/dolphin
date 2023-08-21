// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import androidx.lifecycle.ViewModel;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class SettingsViewModel extends ViewModel
{
  private final Settings mSettings = new Settings();

  public Settings getSettings()
  {
    return mSettings;
  }
}
