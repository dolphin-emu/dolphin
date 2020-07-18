package org.dolphinemu.dolphinemu.features.settings.model;

public interface BooleanSetting extends AbstractSetting
{
  boolean getBoolean(Settings settings, boolean defaultValue);

  void setBoolean(Settings settings, boolean newValue);
}
