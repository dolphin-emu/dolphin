// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractStringSetting extends AbstractSetting
{
  String getString(Settings settings);

  void setString(Settings settings, String newValue);
}
