// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

public interface AbstractBooleanSetting extends AbstractSetting
{
  boolean getBoolean();

  void setBoolean(@NonNull Settings settings, boolean newValue);
}
