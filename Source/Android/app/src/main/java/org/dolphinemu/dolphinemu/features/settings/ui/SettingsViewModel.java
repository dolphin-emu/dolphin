package org.dolphinemu.dolphinemu.features.settings.ui;

import org.dolphinemu.dolphinemu.features.settings.model.Settings;

import androidx.lifecycle.ViewModel;

public class SettingsViewModel extends ViewModel
{
  private final Settings mSettings = new Settings();

  public Settings getSettings()
  {
    return mSettings;
  }
}
