package org.dolphinemu.dolphinemu.features.settings.model;

public interface StringSetting extends AbstractSetting
{
  String getString(Settings settings, String defaultValue);

  void setString(Settings settings, String newValue);
}
