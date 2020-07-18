package org.dolphinemu.dolphinemu.features.settings.model;

public interface IntSetting extends AbstractSetting
{
  int getInt(Settings settings, int defaultValue);

  void setInt(Settings settings, int newValue);
}
