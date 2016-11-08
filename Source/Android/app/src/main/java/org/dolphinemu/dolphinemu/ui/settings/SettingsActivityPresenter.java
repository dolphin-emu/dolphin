package org.dolphinemu.dolphinemu.ui.settings;

import android.os.Bundle;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.HashMap;

public final class SettingsActivityPresenter
{
	private static final String SHOULD_SAVE = BuildConfig.APPLICATION_ID + ".should_save";

	private SettingsActivityView mView;

	private HashMap<String, SettingSection> mDolphinSettings;
	private HashMap<String, SettingSection> mGfxSettings;
	private HashMap<String, SettingSection> mWiimoteSettings;

	private int mStackCount;

	private boolean mShouldSave;

	public SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, String menuTag)
	{
		if (savedInstanceState == null)
		{
			mView.showSettingsFragment(menuTag, false);

			mDolphinSettings = SettingsFile.readFile(SettingsFile.FILE_NAME_DOLPHIN, mView);
			mGfxSettings = SettingsFile.readFile(SettingsFile.FILE_NAME_GFX, mView);
			mWiimoteSettings = SettingsFile.readFile(SettingsFile.FILE_NAME_WIIMOTE, mView);
			mView.onSettingsFileLoaded(mDolphinSettings, mGfxSettings, mWiimoteSettings);
		}
		else
		{
			mShouldSave = savedInstanceState.getBoolean(SHOULD_SAVE);
		}
	}

	public void setSettings(HashMap<String, SettingSection> dolphinSettings, HashMap<String, SettingSection> gfxSettings,
				HashMap<String, SettingSection> wiimoteSettings)
	{
		mDolphinSettings = dolphinSettings;
		mGfxSettings = gfxSettings;
		mWiimoteSettings = wiimoteSettings;
	}

	public HashMap<String, SettingSection> getSettings(String fileName)
	{
		switch (fileName)
		{
			case SettingsFile.FILE_NAME_DOLPHIN:
				return mDolphinSettings;
			case SettingsFile.FILE_NAME_GFX:
				return mGfxSettings;
			case SettingsFile.FILE_NAME_WIIMOTE:
				return mWiimoteSettings;
		}
		return null;
	}

	public void onStop(boolean finishing)
	{
		if (mDolphinSettings != null && finishing && mShouldSave)
		{
			Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
			SettingsFile.saveFile(SettingsFile.FILE_NAME_DOLPHIN, mDolphinSettings, mView);
			SettingsFile.saveFile(SettingsFile.FILE_NAME_GFX, mGfxSettings, mView);
			SettingsFile.saveFile(SettingsFile.FILE_NAME_WIIMOTE, mWiimoteSettings, mView);
			mView.showToastMessage("Saved settings to INI files");
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
			case R.id.menu_exit_no_save:
				mShouldSave = false;
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
		outState.putBoolean(SHOULD_SAVE, mShouldSave);
	}

	public void onGcPadSettingChanged(String key, int value)
	{
		switch (value)
		{
			case 6:
				mView.showToastMessage("Configuration coming soon. Settings from old versions will still work.");
				break;

			case 12:
				mView.showSettingsFragment(key, true);
				break;
		}
	}

	public void onWiimoteSettingChanged(String section, int value)
	{
		switch (value)
		{
			case 1:
				mView.showToastMessage("Configuration coming soon. Settings from old versions will still work.");
				break;

			case 2:
				mView.showToastMessage("Please make sure Continuous Scanning is enabled in Core Settings.");
				break;
		}
	}
}
