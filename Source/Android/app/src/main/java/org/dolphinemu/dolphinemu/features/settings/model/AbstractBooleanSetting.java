// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractBooleanSetting extends AbstractSetting
{
  boolean getBoolean(Settings settings);

  void setBoolean(Settings settings, boolean newValue);
}
