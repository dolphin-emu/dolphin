// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

public interface AbstractFloatSetting extends AbstractSetting
{
  float getFloat();

  void setFloat(@NonNull Settings settings, float newValue);
}
