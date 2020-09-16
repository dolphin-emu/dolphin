package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractIntSetting extends AbstractSetting
{
  int getInt(Settings settings);

  void setInt(Settings settings, int newValue);
}
