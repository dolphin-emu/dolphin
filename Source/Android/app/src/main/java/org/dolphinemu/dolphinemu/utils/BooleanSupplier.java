// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import androidx.annotation.Keep;

public interface BooleanSupplier
{
  @Keep
  boolean get();
}
