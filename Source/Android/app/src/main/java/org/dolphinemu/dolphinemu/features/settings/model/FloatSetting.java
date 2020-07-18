package org.dolphinemu.dolphinemu.features.settings.model;

public interface FloatSetting extends AbstractSetting
{
  float getFloat(Settings settings, float defaultValue);

  void setFloat(Settings settings, float newValue);
}
