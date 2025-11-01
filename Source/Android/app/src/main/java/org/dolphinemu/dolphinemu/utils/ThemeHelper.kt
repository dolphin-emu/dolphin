package org.dolphinemu.dolphinemu.utils

import androidx.appcompat.app.AppCompatActivity
import org.dolphinemu.dolphinemu.R
import android.os.Build
import androidx.core.content.ContextCompat
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.WindowCompat
import org.dolphinemu.dolphinemu.ui.main.ThemeProvider
import android.content.res.Configuration
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.elevation.ElevationOverlayProvider
import com.google.android.material.color.MaterialColors
import androidx.annotation.ColorInt
import androidx.preference.PreferenceManager

object ThemeHelper {

    const val CURRENT_THEME = "current_theme"
    const val CURRENT_THEME_MODE = "current_theme_mode"
    const val USE_BLACK_BACKGROUNDS = "use_black_backgrounds"

    const val DEFAULT = 0
    const val MONET = 1
    const val MATERIAL_DEFAULT = 2
    const val GREEN = 3
    const val PINK = 4

    @JvmStatic
    fun setTheme(activity: AppCompatActivity) {
        // We have to use shared preferences in addition to Dolphin's settings to guarantee that the
        // requested theme id is ready before the onCreate method of any given Activity.
        val preferences = PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
        setThemeMode(activity)
        when (preferences.getInt(CURRENT_THEME, DEFAULT)) {
            DEFAULT -> activity.setTheme(R.style.Theme_Dolphin_Main)
            MONET -> activity.setTheme(R.style.Theme_Dolphin_Main_MaterialYou)
            MATERIAL_DEFAULT -> activity.setTheme(R.style.Theme_Dolphin_Main_Material)
            GREEN -> activity.setTheme(R.style.Theme_Dolphin_Main_Green)
            PINK -> activity.setTheme(R.style.Theme_Dolphin_Main_Pink)
        }
        if (preferences.getBoolean(USE_BLACK_BACKGROUNDS, false)) {
            activity.setTheme(R.style.ThemeOverlay_Dolphin_Dark)
        }

        // Since the top app bar matches the color of the status bar, devices below API 23 have to get a
        // black status bar since their icons do not adapt based on background color
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            activity.window.statusBarColor =
                ContextCompat.getColor(activity.applicationContext, android.R.color.black)
        }
    }

    private fun setThemeMode(activity: AppCompatActivity) {
        val themeMode = PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .getInt(CURRENT_THEME_MODE, AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
        activity.delegate.localNightMode = themeMode
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        val systemReportedThemeMode =
            activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK
        when (themeMode) {
            AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM -> when (systemReportedThemeMode) {
                Configuration.UI_MODE_NIGHT_NO -> setLightModeSystemBars(windowController)
                Configuration.UI_MODE_NIGHT_YES -> setDarkModeSystemBars(windowController)
            }
            AppCompatDelegate.MODE_NIGHT_NO -> setLightModeSystemBars(windowController)
            AppCompatDelegate.MODE_NIGHT_YES -> setDarkModeSystemBars(windowController)
        }
    }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true

        // Fix for an API 26 specific bug where the navigation bar buttons would appear invisible
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.O) {
            windowController.isAppearanceLightNavigationBars = true
        }
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    @JvmStatic
    fun saveTheme(activity: AppCompatActivity, themeValue: Int) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .putInt(CURRENT_THEME, themeValue)
            .apply()
        activity.recreate()
    }

    @JvmStatic
    fun deleteThemeKey(activity: AppCompatActivity) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .remove(CURRENT_THEME)
            .apply()
        activity.recreate()
    }

    @JvmStatic
    fun saveThemeMode(activity: AppCompatActivity, themeModeValue: Int) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .putInt(CURRENT_THEME_MODE, themeModeValue)
            .apply()
        setThemeMode(activity)
    }

    @JvmStatic
    fun deleteThemeModeKey(activity: AppCompatActivity) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .remove(CURRENT_THEME_MODE)
            .apply()
        setThemeMode(activity)
    }

    @JvmStatic
    fun saveBackgroundSetting(activity: AppCompatActivity, backgroundValue: Boolean) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .putBoolean(USE_BLACK_BACKGROUNDS, backgroundValue)
            .apply()
        activity.recreate()
    }

    @JvmStatic
    fun deleteBackgroundSetting(activity: AppCompatActivity) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .remove(USE_BLACK_BACKGROUNDS)
            .apply()
        activity.recreate()
    }

    @JvmStatic
    fun resetThemePreferences(activity: AppCompatActivity, applyImmediately: Boolean = false) {
        PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .edit()
            .remove(CURRENT_THEME)
            .remove(CURRENT_THEME_MODE)
            .remove(USE_BLACK_BACKGROUNDS)
            .apply()
        activity.delegate.localNightMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
        activity.delegate.applyDayNight()
        if (applyImmediately) {
            activity.recreate()
        }
    }

    @JvmStatic
    fun setCorrectTheme(activity: AppCompatActivity) {
        val provider = activity as? ThemeProvider ?: return
        val currentTheme = provider.themeId
        setTheme(activity)
        if (currentTheme != provider.themeId) {
            activity.recreate()
        }
    }

    @JvmStatic
    fun setStatusBarColor(activity: AppCompatActivity, @ColorInt color: Int) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            activity.window.statusBarColor =
                ContextCompat.getColor(activity.applicationContext, android.R.color.black)
        } else {
            activity.window.statusBarColor = color
        }
    }

    @JvmStatic
    fun enableScrollTint(
        activity: AppCompatActivity, toolbar: MaterialToolbar, appBarLayout: AppBarLayout
    ) {
        appBarLayout.addOnOffsetChangedListener { layout: AppBarLayout, verticalOffset: Int ->
            if (-verticalOffset >= layout.totalScrollRange / 2) {
                @ColorInt val color =
                    ElevationOverlayProvider(appBarLayout.context).compositeOverlay(
                        MaterialColors.getColor(appBarLayout, R.attr.colorSurface),
                        activity.resources.getDimensionPixelSize(R.dimen.elevated_app_bar).toFloat()
                    )
                toolbar.setBackgroundColor(color)
                setStatusBarColor(activity, color)
            } else {
                @ColorInt val statusBarColor = ContextCompat.getColor(
                    activity.applicationContext,
                    android.R.color.transparent
                )
                @ColorInt val appBarColor = MaterialColors.getColor(toolbar, R.attr.colorSurface)
                toolbar.setBackgroundColor(appBarColor)
                setStatusBarColor(activity, statusBarColor)
            }
        }
    }

    @JvmStatic
    fun enableStatusBarScrollTint(activity: AppCompatActivity, appBarLayout: AppBarLayout) {
        appBarLayout.addOnOffsetChangedListener { _: AppBarLayout, verticalOffset: Int ->
            if (-verticalOffset > 0) {
                @ColorInt val color = MaterialColors.getColor(appBarLayout, R.attr.colorSurface)
                setStatusBarColor(activity, color)
            } else {
                @ColorInt val statusBarColor = ContextCompat.getColor(
                    activity.applicationContext,
                    android.R.color.transparent
                )
                setStatusBarColor(activity, statusBarColor)
            }
        }
    }
}
