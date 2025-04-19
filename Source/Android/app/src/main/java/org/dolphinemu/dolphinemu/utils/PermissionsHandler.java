// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;

import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;

import org.dolphinemu.dolphinemu.features.camera.Camera;
import org.jetbrains.annotations.NotNull;

import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;
import static android.Manifest.permission.CAMERA;

public class PermissionsHandler
{
  public static final int REQUEST_CODE_WRITE_PERMISSION = 500;
  public static final int REQUEST_CODE_CAMERA_PERMISSION = 502;
  private static boolean sWritePermissionDenied = false;

  public static void requestWritePermission(final FragmentActivity activity)
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return;

    activity.requestPermissions(new String[]{WRITE_EXTERNAL_STORAGE},
            REQUEST_CODE_WRITE_PERMISSION);
  }

  public static boolean hasWriteAccess(Context context)
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
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

  public static boolean hasCameraAccess(Context context) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return true;

    int hasCameraPermission = ContextCompat.checkSelfPermission(context, CAMERA);
    return hasCameraPermission == PackageManager.PERMISSION_GRANTED;
  }

  public static void requestCameraPermission(Activity activity) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return;

    activity.requestPermissions(new String[]{CAMERA}, REQUEST_CODE_CAMERA_PERMISSION);
  }

  public static void onRequestPermissionsResult(int requestCode, @NotNull String[] permissions, @NotNull int[] grantResults) {
    if (requestCode == PermissionsHandler.REQUEST_CODE_CAMERA_PERMISSION) {
      if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
        Camera.Companion.resumeCamera();
      }
    }
  }
}
