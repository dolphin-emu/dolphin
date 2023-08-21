// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

public interface AbstractSetting
{
  boolean isOverridden(@NonNull Settings settings);

  boolean isRuntimeEditable();

  boolean delete(@NonNull Settings settings);
}
