// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

public final class WiiUtils
{
  public static final int RESULT_SUCCESS = 0;
  public static final int RESULT_ERROR = 1;
  public static final int RESULT_CANCELLED = 2;
  public static final int RESULT_CORRUPTED_SOURCE = 3;
  public static final int RESULT_TITLE_MISSING = 4;

  public static final int UPDATE_RESULT_SUCCESS = 0;
  public static final int UPDATE_RESULT_ALREADY_UP_TO_DATE = 1;
  public static final int UPDATE_RESULT_REGION_MISMATCH = 2;
  public static final int UPDATE_RESULT_MISSING_UPDATE_PARTITION = 3;
  public static final int UPDATE_RESULT_DISC_READ_FAILED = 4;
  public static final int UPDATE_RESULT_SERVER_FAILED = 5;
  public static final int UPDATE_RESULT_DOWNLOAD_FAILED = 6;
  public static final int UPDATE_RESULT_IMPORT_FAILED = 7;
  public static final int UPDATE_RESULT_CANCELLED = 8;

  public static native boolean installWAD(String file);

  public static native int importWiiSave(String file, BooleanSupplier canOverwrite);

  public static native void importNANDBin(String file);

  public static native int doOnlineUpdate(String region, WiiUpdateCallback callback);

  public static native int doDiscUpdate(String path, WiiUpdateCallback callback);

  public static native boolean isSystemMenuInstalled();

  public static native boolean isSystemMenuvWii();

  public static native String getSystemMenuVersion();

  public static native boolean syncSdFolderToSdImage();

  public static native boolean syncSdImageToSdFolder();
}
