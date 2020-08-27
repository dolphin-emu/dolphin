package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.content.IntentFilter;
import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.HashSet;
import java.util.Set;

public final class SettingsActivityPresenter
{
  private static final String KEY_SHOULD_SAVE = "should_save";

  private SettingsActivityView mView;

  private Settings mSettings;

  private boolean mShouldSave;

  private DirectoryStateReceiver directoryStateReceiver;

  private MenuTag menuTag;
  private String gameId;
  private Context context;

  private final Set<String> modifiedSettings = new HashSet<>();

  SettingsActivityPresenter(SettingsActivityView view, Settings settings)
  {
    mView = view;
    mSettings = settings;
  }

  public void onCreate(Bundle savedInstanceState, MenuTag menuTag, String gameId, Context context)
  {
    this.menuTag = menuTag;
    this.gameId = gameId;
    this.context = context;

    mShouldSave = savedInstanceState != null && savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
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
        mSettings.loadSettings(gameId, mView);

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
      IntentFilter statusIntentFilter = new IntentFilter(
              DirectoryInitialization.BROADCAST_ACTION);

      directoryStateReceiver =
              new DirectoryStateReceiver(directoryInitializationState ->
              {
                if (directoryInitializationState ==
                        DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
                {
                  mView.hideLoading();
                  loadSettingsUI();
                }
                else if (directoryInitializationState ==
                        DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED)
                {
                  mView.showPermissionNeededHint();
                  mView.hideLoading();
                }
                else if (directoryInitializationState ==
                        DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE)
                {
                  mView.showExternalStorageNotMountedHint();
                  mView.hideLoading();
                }
              });

      mView.startDirectoryInitializationService(directoryStateReceiver, statusIntentFilter);
    }
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void clearSettings()
  {
    mSettings.clearSettings();
    onSettingChanged(null);
  }

  public void onStop(boolean finishing)
  {
    if (directoryStateReceiver != null)
    {
      mView.stopListeningToDirectoryInitializationService(directoryStateReceiver);
      directoryStateReceiver = null;
    }

    if (mSettings != null && finishing && mShouldSave)
    {
      Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
      mSettings.saveSettings(mView, context, modifiedSettings);
    }
  }

  public boolean handleOptionsItem(int itemId)
  {
    switch (itemId)
    {
      case R.id.menu_save_exit:
        mView.finish();
        return true;
    }

    return false;
  }

  public void onSettingChanged(String key)
  {
    if (key != null)
    {
      modifiedSettings.add(key);
    }

    mShouldSave = true;
  }

  public void saveState(Bundle outState)
  {
    outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
  }

  public boolean shouldSave()
  {
    return mShouldSave;
  }

  public void onGcPadSettingChanged(MenuTag key, int value)
  {
    if (value != 0) // Not disabled
    {
      Bundle bundle = new Bundle();
      bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value / 6);
      mView.showSettingsFragment(key, bundle, true, gameId);
    }
  }

  public void onWiimoteSettingChanged(MenuTag menuTag, int value)
  {
    switch (value)
    {
      case 1:
        mView.showSettingsFragment(menuTag, null, true, gameId);
        break;

      case 2:
        mView.showToastMessage("Please make sure Continuous Scanning is enabled in Core Settings.");
        break;
    }
  }

  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    if (value != 0) // None
    {
      Bundle bundle = new Bundle();
      bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value);
      mView.showSettingsFragment(menuTag, bundle, true, gameId);
    }
  }
}
