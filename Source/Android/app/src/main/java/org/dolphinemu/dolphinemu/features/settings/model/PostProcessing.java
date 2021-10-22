// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model;

import androidx.annotation.NonNull;

public class PostProcessing
{
  @NonNull
  public static native String[] getShaderList();

  @NonNull
  public static native String[] getAnaglyphShaderList();

  @NonNull
  public static native String[] getPassiveShaderList();
}
