package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractSetting
{
  boolean isOverridden(Settings settings);

  boolean delete(Settings settings);
}
