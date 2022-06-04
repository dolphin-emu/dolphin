// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import androidx.annotation.Keep;

public interface WiiUpdateCallback
{
  @Keep
  boolean run(int processed, int total, long titleId);
}
