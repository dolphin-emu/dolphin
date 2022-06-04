package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider;

public class ThemeHelper
{
  public static final String CURRENT_THEME = "current_theme";

  public static final int DEFAULT = 0;
  public static final int MONET = 1;
  public static final int MATERIAL_DEFAULT = 2;
  public static final int GREEN = 3;
  public static final int PINK = 4;

  public static void setTheme(Activity activity)
  {
    // We have to use shared preferences in addition to Dolphin's settings to guarantee that the
    // requested theme id is ready before the onCreate method of any given Activity.
    SharedPreferences preferences =
            PreferenceManager.getDefaultSharedPreferences(activity.getApplicationContext());
    switch (preferences.getInt(CURRENT_THEME, DEFAULT))
    {
      case DEFAULT:
        activity.setTheme(R.style.Theme_Dolphin_Main);
        activity.getWindow()
                .setStatusBarColor(activity.getResources().getColor(R.color.dolphin_surface));
        break;

      case MONET:
        activity.setTheme(R.style.Theme_Dolphin_Main_MaterialYou);
        int currentNightMode = activity.getResources().getConfiguration().uiMode &
                Configuration.UI_MODE_NIGHT_MASK;
        switch (currentNightMode)
        {
          case Configuration.UI_MODE_NIGHT_NO:
            activity.getWindow().setStatusBarColor(
                    activity.getResources().getColor(R.color.m3_sys_color_dynamic_light_surface));
            break;
          case Configuration.UI_MODE_NIGHT_YES:
            activity.getWindow().setStatusBarColor(
                    activity.getResources().getColor(R.color.m3_sys_color_dynamic_dark_surface));
            break;
        }
        break;

      case MATERIAL_DEFAULT:
        activity.setTheme(R.style.Theme_Dolphin_Main_Material);
        activity.getWindow()
                .setStatusBarColor(activity.getResources().getColor(R.color.dolphin_surface));
        break;

      case GREEN:
        activity.setTheme(R.style.Theme_Dolphin_Main_Green);
        activity.getWindow()
                .setStatusBarColor(activity.getResources().getColor(R.color.green_surface));
        break;

      case PINK:
        activity.setTheme(R.style.Theme_Dolphin_Main_Pink);
        activity.getWindow()
                .setStatusBarColor(activity.getResources().getColor(R.color.pink_surface));
        break;
    }

    // Since the top app bar matches the color of the status bar, devices below API 23 have to get a
    // black status bar since their icons do not adapt based on background color
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
    {
      activity.getWindow()
              .setStatusBarColor(activity.getResources().getColor(android.R.color.black));
    }
  }

  public static void saveTheme(Activity activity, int themeValue)
  {
    SharedPreferences preferences =
            PreferenceManager.getDefaultSharedPreferences(activity.getApplicationContext());
    preferences.edit().putInt(CURRENT_THEME, themeValue).apply();
  }

  public static void deleteThemeKey(Activity activity)
  {
    SharedPreferences preferences =
            PreferenceManager.getDefaultSharedPreferences(activity.getApplicationContext());
    preferences.edit().remove(CURRENT_THEME).apply();
    activity.setTheme(R.style.Theme_Dolphin_Main);
    activity.recreate();
  }

  public static void setCorrectTheme(Activity activity)
  {
    int currentTheme = ((ThemeProvider) activity).getThemeId();
    setTheme(activity);

    if (currentTheme != ((ThemeProvider) activity).getThemeId())
    {
      activity.recreate();
    }
  }
}
