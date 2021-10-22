// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

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

  private AfterDirectoryInitializationRunner mAfterDirectoryInitializationRunner;

  private MenuTag mMenuTag;
  private String mGameId;
  private int mRevision;
  private boolean mIsWii;
  private Context mContext;

  SettingsActivityPresenter(SettingsActivityView view, Settings settings)
  {
    mView = view;
    mSettings = settings;
  }

  public void onCreate(Bundle savedInstanceState, MenuTag menuTag, String gameId, int revision,
          boolean isWii, Context context)
  {
    this.mMenuTag = menuTag;
    this.mGameId = gameId;
    this.mRevision = revision;
    this.mIsWii = isWii;
    this.mContext = context;

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
      if (!TextUtils.isEmpty(mGameId))
      {
        mSettings.loadSettings(mView, mGameId, mRevision, mIsWii);

        if (mSettings.gameIniContainsJunk())
        {
          mView.showGameIniJunkDeletionQuestion();
        }
      }
      else
      {
        mSettings.loadSettings(mView, mIsWii);
      }
    }

    mView.showSettingsFragment(mMenuTag, null, false, mGameId);
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
      mAfterDirectoryInitializationRunner.run(mContext, true, this::loadSettingsUI);
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
      mSettings.saveSettings(mView, mContext);
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
      mView.showSettingsFragment(key, bundle, true, mGameId);
    }
  }

  public void onWiimoteSettingChanged(MenuTag menuTag, int value)
  {
    switch (value)
    {
      case 1:
        mView.showSettingsFragment(menuTag, null, true, mGameId);
        break;

      case 2:
        mView.showToastMessage(mContext.getString(R.string.make_sure_continuous_scan_enabled));
        break;
    }
  }

  public void onExtensionSettingChanged(MenuTag menuTag, int value)
  {
    if (value != 0) // None
    {
      Bundle bundle = new Bundle();
      bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value);
      mView.showSettingsFragment(menuTag, bundle, true, mGameId);
    }
  }
}
