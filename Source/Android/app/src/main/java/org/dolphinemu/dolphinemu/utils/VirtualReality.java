// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;

public class VirtualReality
{
  private static final String VR_PACKAGE = "org.dolphinemu.dolphinemu.vr";

  public static boolean isActive(Context context)
  {
    return context.getPackageName().equals(VR_PACKAGE);
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
