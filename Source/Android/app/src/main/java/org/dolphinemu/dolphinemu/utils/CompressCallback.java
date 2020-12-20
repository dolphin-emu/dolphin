package org.dolphinemu.dolphinemu.utils;

import androidx.annotation.Keep;

public interface CompressCallback
{
  @Keep
  boolean run(String text, float completion);
}
