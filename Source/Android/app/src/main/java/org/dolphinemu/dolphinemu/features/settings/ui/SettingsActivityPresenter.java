// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.core.app.ComponentActivity;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.Log;

public final class SettingsActivityPresenter
{
  private static final String KEY_SHOULD_SAVE = "should_save";

  private SettingsActivityView mView;

  private Settings mSettings;

  private boolean mShouldSave;

  private MenuTag mMenuTag;
  private String mGameId;
  private int mRevision;
  private boolean mIsWii;
  private ComponentActivity mActivity;

  SettingsActivityPresenter(SettingsActivityView view, Settings settings)
  {
    mView = view;
    mSettings = settings;
  }

  public void onCreate(Bundle savedInstanceState, MenuTag menuTag, String gameId, int revision,
          boolean isWii, ComponentActivity activity)
  {
    this.mMenuTag = menuTag;
    this.mGameId = gameId;
    this.mRevision = revision;
    this.mIsWii = isWii;
    this.mActivity = activity;

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
    mView.hideLoading();

    if (!mSettings.areSettingsLoaded())
    {
      if (!TextUtils.isEmpty(mGameId))
      {
        mSettings.loadSettings(mGameId, mRevision, mIsWii);

        if (mSettings.gameIniContainsJunk())
        {
          mView.showGameIniJunkDeletionQuestion();
        }
      }
      else
      {
        mSettings.loadSettings(mIsWii);
      }
    }

    mView.showSettingsFragment(mMenuTag, null, false, mGameId);
    mView.onSettingsFileLoaded(mSettings);
  }

  private void prepareDolphinDirectoriesIfNeeded()
  {
    mView.showLoading();

    new AfterDirectoryInitializationRunner().runWithLifecycle(mActivity, this::loadSettingsUI);
  }

  public Settings getSettings()
  {
    return mSettings;
  }

  public void clearGameSettings()
  {
    mSettings.clearGameSettings();
    onSettingChanged();
  }

  public void onStop(boolean finishing)
  {
    if (mSettings != null && finishing && mShouldSave)
    {
      Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
      mSettings.saveSettings(mActivity);
    }
  }

  public void onSettingChanged()
  {
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

  public void onMenuTagAction(@NonNull MenuTag menuTag, int value)
  {
    if (menuTag.isSerialPort1Menu())
    {
      if (value != 0 && value != 255) // Not disabled or dummy
      {
        Bundle bundle = new Bundle();
        bundle.putInt(SettingsFragmentPresenter.ARG_SERIALPORT1_TYPE, value);
        mView.showSettingsFragment(menuTag, bundle, true, mGameId);
      }
    }

    if (menuTag.isGCPadMenu())
    {
      if (value != 0) // Not disabled
      {
        Bundle bundle = new Bundle();
        bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value);
        mView.showSettingsFragment(menuTag, bundle, true, mGameId);
      }
    }

    if (menuTag.isWiimoteMenu())
    {
      if (value == 1) // Emulated Wii Remote
      {
        mView.showSettingsFragment(menuTag, null, true, mGameId);
      }
    }

    if (menuTag.isWiimoteExtensionMenu())
    {
      if (value != 0) // Not disabled
      {
        Bundle bundle = new Bundle();
        bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value);
        mView.showSettingsFragment(menuTag, bundle, true, mGameId);
      }
    }
  }

  public boolean hasMenuTagActionForValue(@NonNull MenuTag menuTag, int value)
  {
    if (menuTag.isSerialPort1Menu())
    {
      return (value != 0 && value != 255); // Not disabled or dummy
    }

    if (menuTag.isGCPadMenu())
    {
      return (value != 0); // Not disabled
    }

    if (menuTag.isWiimoteMenu())
    {
      return (value == 1); // Emulated Wii Remote
    }

    if (menuTag.isWiimoteExtensionMenu())
    {
      return (value != 0); // Not disabled
    }

    return false;
  }
}
