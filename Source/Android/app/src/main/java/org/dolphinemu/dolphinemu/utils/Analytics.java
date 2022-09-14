// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.os.Build;

import androidx.annotation.Keep;

import com.android.volley.Request;
import com.android.volley.toolbox.StringRequest;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public class Analytics
{
  private static final String DEVICE_MANUFACTURER = "DEVICE_MANUFACTURER";
  private static final String DEVICE_OS = "DEVICE_OS";
  private static final String DEVICE_MODEL = "DEVICE_MODEL";
  private static final String DEVICE_TYPE = "DEVICE_TYPE";

  public static void checkAnalyticsInit(Context context)
  {
    new AfterDirectoryInitializationRunner().runWithoutLifecycle(() ->
    {
      if (!BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.getBooleanGlobal())
      {
        showMessage(context);
      }
    });
  }

  private static void showMessage(Context context)
  {
    new MaterialAlertDialogBuilder(context)
            .setTitle(context.getString(R.string.analytics))
            .setMessage(context.getString(R.string.analytics_desc))
            .setPositiveButton(R.string.yes, (dialogInterface, i) -> firstAnalyticsAdd(true))
            .setNegativeButton(R.string.no, (dialogInterface, i) -> firstAnalyticsAdd(false))
            .show();
  }

  private static void firstAnalyticsAdd(boolean enabled)
  {
    try (Settings settings = new Settings())
    {
      settings.loadSettings();

      BooleanSetting.MAIN_ANALYTICS_ENABLED.setBoolean(settings, enabled);
      BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.setBoolean(settings, true);

      // Context is set to null to avoid toasts
      settings.saveSettings(null, null);
    }
  }

  @Keep
  public static void sendReport(String endpoint, byte[] data)
  {
    StringRequest request = new StringRequest(Request.Method.POST, endpoint,
            null, error -> Log.debug("Failed to send report"))
    {
      @Override
      public byte[] getBody()
      {
        return data;
      }
    };

    VolleyUtil.getQueue().add(request);
  }

  @Keep
  public static String getValue(String key)
  {
    switch (key)
    {
      case DEVICE_MODEL:
        return Build.MODEL;
      case DEVICE_MANUFACTURER:
        return Build.MANUFACTURER;
      case DEVICE_OS:
        return String.valueOf(Build.VERSION.SDK_INT);
      case DEVICE_TYPE:
        return TvUtil.isLeanback(DolphinApplication.getAppContext()) ? "android-tv" :
                "android-mobile";
      default:
        return "";
    }
  }
}
