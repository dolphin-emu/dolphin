package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.os.Build;

import androidx.appcompat.app.AlertDialog;

import com.android.volley.Request;
import com.android.volley.toolbox.StringRequest;

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
    new AfterDirectoryInitializationRunner().run(context, false, () ->
    {
      Settings settings = new Settings();
      settings.loadSettings(null);
      if (!BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.getBoolean(settings))
      {
        showMessage(context, settings);
      }
      else
      {
        settings.close();
      }
    });
  }

  private static void showMessage(Context context, Settings settings)
  {
    new AlertDialog.Builder(context, R.style.DolphinDialogBase)
            .setTitle(context.getString(R.string.analytics))
            .setMessage(context.getString(R.string.analytics_desc))
            .setPositiveButton(R.string.yes, (dialogInterface, i) ->
            {
              firstAnalyticsAdd(settings, true);
            })
            .setNegativeButton(R.string.no, (dialogInterface, i) ->
            {
              firstAnalyticsAdd(settings, false);
            })
            .show();
  }

  private static void firstAnalyticsAdd(Settings settings, boolean enabled)
  {
    BooleanSetting.MAIN_ANALYTICS_ENABLED.setBoolean(settings, enabled);
    BooleanSetting.MAIN_ANALYTICS_PERMISSION_ASKED.setBoolean(settings, true);

    // Context is set to null to avoid toasts
    settings.saveSettings(null, null);

    settings.close();
  }

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
