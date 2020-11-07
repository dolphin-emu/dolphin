package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.IniFile;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;

public final class SettingsActivityPresenter
{
  public static final int WIIMOTE_DISABLED = 0;
  public static final int WIIMOTE_EMULATED = 1;
  public static final int WIIMOTE_REAL = 2;

  public static final int WIIMOTE_EXT_NONE = 0;
  public static final int WIIMOTE_EXT_NUNCHUK = 1;
  public static final int WIIMOTE_EXT_CLASSIC = 2;
  public static final int WIIMOTE_EXT_GUITAR = 3;
  public static final int WIIMOTE_EXT_DRUMS = 4;
  public static final int WIIMOTE_EXT_TURNTABLE = 5;

  private static final String KEY_SHOULD_SAVE = "should_save";

  private final SettingsActivityView mView;

  private Settings mSettings;

  private boolean mShouldSave;

  private AfterDirectoryInitializationRunner mAfterDirectoryInitializationRunner;

  private MenuTag menuTag;
  private String gameId;
  private int revision;
  private Context context;

  SettingsActivityPresenter(SettingsActivityView view, Settings settings)
  {
    mView = view;
    mSettings = settings;
  }

  public void onCreate(Bundle savedInstanceState, MenuTag menuTag, String gameId, int revision,
          Context context)
  {
    this.menuTag = menuTag;
    this.gameId = gameId;
    this.revision = revision;
    this.context = context;

    mShouldSave = savedInstanceState != null && savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
  }

  public void onDestroy()
  {
    if (mSettings != null)
    {
      mSettings.close();
      mSettings = null;
    }
  }

  public void onStart()
  {
    prepareDolphinDirectoriesIfNeeded();
  }

  private void loadSettingsUI()
  {
    if (mSettings.isEmpty())
    {
      if (!TextUtils.isEmpty(gameId))
      {
        mSettings.loadSettings(gameId, revision, mView);

        if (mSettings.gameIniContainsJunk())
        {
          mView.showGameIniJunkDeletionQuestion();
        }
      }
      else
      {
        mSettings.loadSettings(mView);
      }
    }

    mView.showSettingsFragment(menuTag, null, false, gameId);
    mView.onSettingsFileLoaded(mSettings);
  }

  private void prepareDolphinDirectoriesIfNeeded()
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      loadSettingsUI();
    }
    else
    {
      mView.showLoading();

      mAfterDirectoryInitializationRunner = new AfterDirectoryInitializationRunner();
      mAfterDirectoryInitializationRunner.setFinishedCallback(mView::hideLoading);
      mAfterDirectoryInitializationRunner.run(context, true, this::loadSettingsUI);
    }
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void clearSettings()
  {
    mSettings.clearSettings();
    onSettingChanged();
  }

  public void onStop(boolean finishing)
  {
    if (mAfterDirectoryInitializationRunner != null)
    {
      mAfterDirectoryInitializationRunner.cancel();
      mAfterDirectoryInitializationRunner = null;
    }

    if (mSettings != null && finishing && mShouldSave)
    {
      Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
      mSettings.saveSettings(mView, context);
    }
  }

  public boolean handleOptionsItem(int itemId)
  {
    if (itemId == R.id.menu_save_exit)
    {
      mView.finish();
      return true;
    }

    return false;
  }

  public void onSettingChanged()
  {
    mShouldSave = true;
  }

  public void saveState(Bundle outState)
  {
    outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
  }

  public void onGcPadSettingChanged(MenuTag key, int value)
  {
    if (key == MenuTag.GCPAD_1) // Player 1
    {
      switch (value)
      {
        case InputOverlay.DISABLED_GAMECUBE_CONTROLLER:
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER.setInt(mSettings, InputOverlay.OVERLAY_NONE);
          break;

        case InputOverlay.EMULATED_GAMECUBE_CONTROLLER:
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER.setInt(mSettings, InputOverlay.OVERLAY_GAMECUBE);
          break;

        case InputOverlay.GAMECUBE_ADAPTER:
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                  .setInt(mSettings, InputOverlay.OVERLAY_GAMECUBE_ADAPTER);
          break;
      }
    }

    if (value != InputOverlay.DISABLED_GAMECUBE_CONTROLLER)
    {
      Bundle bundle = new Bundle();
      bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value / 6);
      mView.showSettingsFragment(key, bundle, true, gameId);
    }
  }

  public void onWiimoteSettingChanged(MenuTag menuTag, int value)
  {
    File wiimoteNewFile = SettingsFile.getSettingsFile(Settings.FILE_WIIMOTE);
    IniFile wiimoteNewIni = new IniFile(wiimoteNewFile);

    String extension = wiimoteNewIni
            .getString(SettingsFile.KEY_WIIMOTE_PLAYER_1, SettingsFile.KEY_WIIMOTE_EXTENSION,
                    "None");

    if (menuTag == MenuTag.WIIMOTE_1) // Player 1
    {
      switch (value)
      {
        case WIIMOTE_DISABLED:
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER.setInt(mSettings, InputOverlay.OVERLAY_NONE);
          break;

        case WIIMOTE_EMULATED:
          setWiiOverlayControllerByExtension(mSettings, extension);
          break;

        case WIIMOTE_REAL:
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                  .setInt(mSettings, InputOverlay.OVERLAY_WIIMOTE_REAL);
          break;
      }
    }

    switch (value)
    {
      case WIIMOTE_EMULATED:
        mView.showSettingsFragment(menuTag, null, true, gameId);
        break;

      case WIIMOTE_REAL:
        if (!BooleanSetting.MAIN_WIIMOTE_CONTINUOUS_SCANNING.getBooleanGlobal())
        {
          BooleanSetting.MAIN_WIIMOTE_CONTINUOUS_SCANNING.setBoolean(mSettings, true);
          mView.showToastMessage(context.getString(R.string.wiimote_scanning_enabled));
        }
        break;
    }
  }

  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    String extension =
            context.getResources().getStringArray(R.array.wiimoteExtensionsValues)[value];

    // Immediately set this setting in case onWiimoteSettingChanged is called before saving.
    if (TextUtils.isEmpty(gameId))
    {
      File wiimoteNewFile = SettingsFile.getSettingsFile(Settings.FILE_WIIMOTE);
      IniFile wiimoteNewIni = new IniFile(wiimoteNewFile);
      wiimoteNewIni.setString(SettingsFile.KEY_WIIMOTE_PLAYER_1, SettingsFile.KEY_WIIMOTE_EXTENSION,
              extension);
      wiimoteNewIni.save(wiimoteNewFile);
    }
    else
    {
      SettingsFile.saveCustomWiimoteSetting(gameId, SettingsFile.KEY_WIIMOTE_EXTENSION, extension,
              menuTag.getSubType() - 4);
    }

    setWiiOverlayControllerByExtension(mSettings, extension);

    Bundle bundle = new Bundle();
    bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value);
    mView.showSettingsFragment(menuTag, bundle, true, gameId);
  }

  public static void setWiiOverlayControllerByExtension(Settings settings, String extension)
  {
    File wiimoteNewFile = SettingsFile.getSettingsFile(Settings.FILE_WIIMOTE);
    IniFile wiimoteNewIni = new IniFile(wiimoteNewFile);

    switch (extension)
    {
      case "None":
        if (wiimoteNewIni
                .getBoolean(SettingsFile.KEY_WIIMOTE_PLAYER_1, SettingsFile.KEY_WIIMOTE_ORIENTATION,
                        false))
        {
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                  .setInt(settings, InputOverlay.OVERLAY_WIIMOTE_HORIZONTAL);
        }
        else
        {
          IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                  .setInt(settings, InputOverlay.OVERLAY_WIIMOTE_VERTICAL);
        }
        break;

      case "Nunchuk":
        IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                .setInt(settings, InputOverlay.OVERLAY_WIIMOTE_NUNCHUK);
        break;

      case "Classic":
        IntSetting.MAIN_WII_OVERLAY_CONTROLLER
                .setInt(settings, InputOverlay.OVERLAY_WIIMOTE_CLASSIC);
        break;
    }
  }
}
