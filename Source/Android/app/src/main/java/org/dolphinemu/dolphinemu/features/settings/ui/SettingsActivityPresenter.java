package org.dolphinemu.dolphinemu.features.settings.ui;

import android.os.Bundle;
import android.text.TextUtils;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;

public final class SettingsActivityPresenter
{
	private static final String KEY_SHOULD_SAVE = "should_save";
	private static final String KEY_MENU_TAG = "menu_tag";
	private static final String KEY_GAME_ID = "game_id";

	private SettingsActivityView mView;

	private Settings mSettings = new Settings();

	private int mStackCount;

	private boolean mShouldSave;

	private MenuTag menuTag;
	private String gameId;

	SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, MenuTag menuTag, String gameId)
	{
		if (savedInstanceState == null)
		{
			this.menuTag = menuTag;
			this.gameId = gameId;
		}
		else
		{
			String menuTagStr = savedInstanceState.getString(KEY_MENU_TAG);
			this.mShouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
			this.gameId = savedInstanceState.getString(KEY_GAME_ID);
			this.menuTag = MenuTag.getMenuTag(menuTagStr);
		}
	}

	public void onStart()
	{
		loadSettingsUI();
	}

	private void loadSettingsUI()
	{
		if (mSettings.isEmpty())
		{
			if (!TextUtils.isEmpty(gameId))
			{
				mSettings.loadSettings(gameId, mView);
			}
			else
			{
				mSettings.loadSettings(mView);
			}
		}

		mView.showSettingsFragment(menuTag, null, false, gameId);
		mView.onSettingsFileLoaded(mSettings);
	}

	public void setSettings(Settings settings)
	{
		mSettings = settings;
	}

	public Settings getSettings()
	{
		return mSettings;
	}

	public void onStop(boolean finishing)
	{
		if (mSettings != null && finishing && mShouldSave)
		{
			mSettings.saveSettings(mView);
		}
	}

	public void addToStack()
	{
		mStackCount++;
	}

	public void onBackPressed()
	{
		if (mStackCount > 0)
		{
			mView.popBackStack();
			mStackCount--;
		}
		else
		{
			mView.finish();
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

	public void onSettingChanged()
	{
		mShouldSave = true;
	}

	public void saveState(Bundle outState)
	{
		outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
		outState.putString(KEY_MENU_TAG, menuTag.toString());
		outState.putString(KEY_GAME_ID, gameId);
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
