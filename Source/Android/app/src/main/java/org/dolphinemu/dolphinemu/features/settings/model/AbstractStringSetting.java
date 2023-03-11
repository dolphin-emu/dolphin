// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

public interface AbstractStringSetting extends AbstractSetting
{
  @NonNull
  String getString();

  void setString(@NonNull Settings settings, @NonNull String newValue);
}
