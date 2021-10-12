// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;

import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;

public class PermissionsHandler
{
  public static final int REQUEST_CODE_WRITE_PERMISSION = 500;

  @TargetApi(Build.VERSION_CODES.M)
  public static boolean checkWritePermission(final FragmentActivity activity)
  {
    if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
    {
      return true;
    }

    int hasWritePermission = ContextCompat.checkSelfPermission(activity, WRITE_EXTERNAL_STORAGE);

    if (hasWritePermission != PackageManager.PERMISSION_GRANTED)
    {
      // We only care about displaying the "Don't ask again" check and can ignore the result.
      // Previous toasts already explained the rationale.
      activity.shouldShowRequestPermissionRationale(WRITE_EXTERNAL_STORAGE);
      activity.requestPermissions(new String[]{WRITE_EXTERNAL_STORAGE},
              REQUEST_CODE_WRITE_PERMISSION);
      return false;
    }

    return true;
  }

  public static boolean hasWriteAccess(Context context)
  {
    if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
    {
      int hasWritePermission = ContextCompat.checkSelfPermission(context, WRITE_EXTERNAL_STORAGE);
      return hasWritePermission == PackageManager.PERMISSION_GRANTED;
    }

    return true;
  }
}
