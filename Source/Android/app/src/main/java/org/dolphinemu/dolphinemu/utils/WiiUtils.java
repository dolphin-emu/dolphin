// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

public final class WiiUtils
{
  public static final int RESULT_SUCCESS = 0;
  public static final int RESULT_ERROR = 1;
  public static final int RESULT_CANCELLED = 2;
  public static final int RESULT_CORRUPTED_SOURCE = 3;
  public static final int RESULT_TITLE_MISSING = 4;

  public static native boolean installWAD(String file);

  public static native int importWiiSave(String file, BooleanSupplier canOverwrite);

  public static native void importNANDBin(String file);
}
