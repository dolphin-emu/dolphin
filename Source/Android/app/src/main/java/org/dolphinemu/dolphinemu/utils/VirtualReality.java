// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;

import org.dolphinemu.dolphinemu.BuildConfig;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Locale;

public class VirtualReality
{
  private static final String VR_PACKAGE = "org.dolphinemu.dolphinemu.vr";

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

  public static void linkLoader()
  {
    if (isActive())
    {
      String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.ROOT);
      // rename oculus to meta as this will probably happen in the future anyway
      if (manufacturer.contains("oculus"))
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

  public static void openIntent(Context context, String[] filePaths)
  {
    Intent launcher = context.getPackageManager().getLaunchIntentForPackage(VR_PACKAGE);

    if (VirtualReality.isLegacyPath(filePaths[0]))
    {
      launcher.putExtra("AutoStartFiles", filePaths);
    }
    else
    {
      Uri uri;
      try
      {
        uri = ContentHandler.unmangle(filePaths[0]);
      }
      catch (FileNotFoundException e)
      {
        uri = Uri.parse(filePaths[0]);
      }
      launcher.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
      launcher.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
      launcher.setAction(Intent.ACTION_GET_CONTENT);
      launcher.setData(uri);
    }

    serializeConfigs(launcher);
    context.getApplicationContext().startActivity(launcher);
  }

  public static void restoreConfig(Bundle extras)
  {
    File root = new File(DirectoryInitialization.getUserDirectory() + "/Config/");
    for (File file : root.listFiles())
    {
      if (file.isDirectory())
        continue;

      try
      {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
          byte[] bytes = extras.getByteArray(file.getName());
          if (bytes != null)
            Files.write(file.toPath(), bytes);
        }
      }
      catch (IOException e)
      {
        e.printStackTrace();
      }
    }
  }

  private static void serializeConfigs(Intent intent)
  {
    File root = new File(DirectoryInitialization.getUserDirectory() + "/Config/");
    for (File file : root.listFiles())
    {
      if (file.isDirectory())
        continue;

      try
      {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
          intent.putExtra(file.getName(), Files.readAllBytes(file.toPath()));
        }
      }
      catch (IOException e)
      {
        e.printStackTrace();
      }
    }
  }
}
