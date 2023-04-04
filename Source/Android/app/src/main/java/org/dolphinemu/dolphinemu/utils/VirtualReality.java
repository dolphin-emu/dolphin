// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;

import org.dolphinemu.dolphinemu.BuildConfig;

import java.util.Locale;

public class VirtualReality
{
  private static final String VR_PACKAGE = "org.dolphinemu.dolphinemu.vr";

  static {
    if (isActive())
    {
      String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.ROOT);
      if (manufacturer.contains("oculus")) // rename oculus to meta as this will probably happen in the future anyway
        manufacturer = "meta";

      //Load manufacturer specific loader
      try
      {
        System.loadLibrary("openxr_" + manufacturer);
      }
      catch (UnsatisfiedLinkError e)
      {
        Log.error("Unsupported VR device: " + manufacturer);
        System.exit(0);
      }
    }
  }

  public static boolean isActive()
  {
    return BuildConfig.BUILD_TYPE.equals("vr");
  }

  public static boolean isInstalled(Context context)
  {
    PackageManager pm = context.getPackageManager();
    for (ApplicationInfo app : pm.getInstalledApplications(PackageManager.GET_META_DATA))
    {
      if (app.packageName.equals(VR_PACKAGE))
      {
        return true;
      }
    }
    return false;
  }

  public static boolean isLegacyPath(String filePath)
  {
    return filePath.startsWith("/");
  }

  public static void openIntent(Context context, String[] filePaths)
  {
    Intent launcher = context.getPackageManager().getLaunchIntentForPackage(VR_PACKAGE);

    if (VirtualReality.isLegacyPath(filePaths[0]))
    {
      launcher.putExtra("AutoStartFiles", filePaths);
    }
    else
    {
      Uri uri = Uri.parse(filePaths[0]);
      ContentResolver resolver = context.getContentResolver();
      resolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
      launcher.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
      launcher.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
      launcher.setAction(Intent.ACTION_GET_CONTENT);
      launcher.setData(uri);
    }

    context.startActivity(launcher);
  }
}
