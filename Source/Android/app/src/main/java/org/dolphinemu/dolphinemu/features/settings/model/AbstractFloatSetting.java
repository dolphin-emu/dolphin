package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractFloatSetting extends AbstractSetting
{
  float getFloat(Settings settings);

  void setFloat(Settings settings, float newValue);
}
