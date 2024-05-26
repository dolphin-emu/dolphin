// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;

import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;

import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;

public class PermissionsHandler
{
  public static final int REQUEST_CODE_WRITE_PERMISSION = 500;
  public static final int REQUEST_CODE_RECORD_AUDIO = 501;
  private static boolean sWritePermissionDenied = false;

  public static void requestWritePermission(final FragmentActivity activity)
  {
    if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return;

    activity.requestPermissions(new String[]{WRITE_EXTERNAL_STORAGE},
            REQUEST_CODE_WRITE_PERMISSION);
  }

  public static boolean hasWriteAccess(Context context)
  {
    if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return true;

    if (!isExternalStorageLegacy())
      return false;

    int hasWritePermission = ContextCompat.checkSelfPermission(context, WRITE_EXTERNAL_STORAGE);
    return hasWritePermission == PackageManager.PERMISSION_GRANTED;
  }

  public static boolean isExternalStorageLegacy()
  {
    return Build.VERSION.SDK_INT < Build.VERSION_CODES.Q || Environment.isExternalStorageLegacy();
  }

  public static void setWritePermissionDenied()
  {
    sWritePermissionDenied = true;
  }

  public static boolean isWritePermissionDenied()
  {
    return sWritePermissionDenied;
  }
}
