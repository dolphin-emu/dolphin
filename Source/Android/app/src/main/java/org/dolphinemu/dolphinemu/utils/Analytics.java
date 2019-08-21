package org.dolphinemu.dolphinemu.utils;

import android.app.AlertDialog;
import android.content.Context;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;
import android.support.v4.content.LocalBroadcastManager;

import com.android.volley.Request;
import com.android.volley.toolbox.StringRequest;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;

public class Analytics
{
  private static final String analyticsAsked =
          Settings.SECTION_ANALYTICS + "_" + SettingsFile.KEY_ANALYTICS_PERMISSION_ASKED;
  private static final String analyticsEnabled =
          Settings.SECTION_ANALYTICS + "_" + SettingsFile.KEY_ANALYTICS_ENABLED;

  private static final String DEVICE_MANUFACTURER = "DEVICE_MANUFACTURER";
  private static final String DEVICE_OS = "DEVICE_OS";
  private static final String DEVICE_MODEL = "DEVICE_MODEL";
  private static final String DEVICE_TYPE = "DEVICE_TYPE";

  public static void checkAnalyticsInit(Context context)
  {
    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
    if (!preferences.getBoolean(analyticsAsked, false))
    {
      new AfterDirectoryInitializationRunner().run(context,
              () -> showMessage(context, preferences));
    }
  }

  private static void showMessage(Context context, SharedPreferences preferences)
  {
    // We asked, set to true regardless of answer
    SharedPreferences.Editor sPrefsEditor = preferences.edit();
    sPrefsEditor.putBoolean(analyticsAsked, true);
    sPrefsEditor.apply();

    new AlertDialog.Builder(context)
            .setTitle(context.getString(R.string.analytics))
            .setMessage(context.getString(R.string.analytics_desc))
            .setPositiveButton(R.string.yes, (dialogInterface, i) ->
            {
              sPrefsEditor.putBoolean(analyticsEnabled, true);
              sPrefsEditor.apply();
              SettingsFile.firstAnalyticsAdd(true);
            })
            .setNegativeButton(R.string.no, (dialogInterface, i) ->
            {
              sPrefsEditor.putBoolean(analyticsEnabled, false);
              sPrefsEditor.apply();
              SettingsFile.firstAnalyticsAdd(false);
            })
            .create()
            .show();
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
