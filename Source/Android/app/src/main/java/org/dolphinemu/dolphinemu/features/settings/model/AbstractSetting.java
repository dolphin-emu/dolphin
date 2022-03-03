// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

public interface AbstractSetting
{
  boolean isOverridden(Settings settings);

  boolean isRuntimeEditable();

  boolean delete(Settings settings);
}
