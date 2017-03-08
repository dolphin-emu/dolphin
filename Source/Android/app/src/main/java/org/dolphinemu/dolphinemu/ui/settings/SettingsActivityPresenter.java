package org.dolphinemu.dolphinemu.ui.settings;

import android.os.Bundle;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsActivityPresenter
{
	private static final String SHOULD_SAVE = BuildConfig.APPLICATION_ID + ".should_save";

	private SettingsActivityView mView;

	private ArrayList<HashMap<String, SettingSection>> mSettings = new ArrayList<>();

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

			mSettings.add(SettingsFile.SETTINGS_DOLPHIN, SettingsFile.readFile(SettingsFile.FILE_NAME_DOLPHIN, mView));
			mSettings.add(SettingsFile.SETTINGS_GFX, SettingsFile.readFile(SettingsFile.FILE_NAME_GFX, mView));
			mSettings.add(SettingsFile.SETTINGS_WIIMOTE, SettingsFile.readFile(SettingsFile.FILE_NAME_WIIMOTE, mView));
			mView.onSettingsFileLoaded(mSettings);
		}
		else
		{
			mShouldSave = savedInstanceState.getBoolean(SHOULD_SAVE);
		}
	}

	public void setSettings(ArrayList<HashMap<String, SettingSection>> settings)
	{
		mSettings = settings;
	}

	public HashMap<String, SettingSection> getSettings(int file)
	{
		return mSettings.get(file);
	}

	public void onStop(boolean finishing)
	{
		if (mSettings != null && finishing && mShouldSave)
		{
			Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
			SettingsFile.saveFile(SettingsFile.FILE_NAME_DOLPHIN, mSettings.get(SettingsFile.SETTINGS_DOLPHIN), mView);
			SettingsFile.saveFile(SettingsFile.FILE_NAME_GFX, mSettings.get(SettingsFile.SETTINGS_GFX), mView);
			SettingsFile.saveFile(SettingsFile.FILE_NAME_WIIMOTE, mSettings.get(SettingsFile.SETTINGS_WIIMOTE), mView);
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
		outState.putBoolean(SHOULD_SAVE, mShouldSave);
	}

	public void onGcPadSettingChanged(String key, int value)
	{
		if (value != 0) // Not disabled
		{
			mView.showSettingsFragment(key + (value / 6), true);
		}
	}

	public void onWiimoteSettingChanged(String section, int value)
	{
		switch (value)
		{
			case 1:
				mView.showSettingsFragment(section, true);
				break;

			case 2:
				mView.showToastMessage("Please make sure Continuous Scanning is enabled in Core Settings.");
				break;
		}
	}

	public void onExtensionSettingChanged(String key, int value)
	{
		if (value != 0) // None
		{
			mView.showSettingsFragment(key + value, true);
		}
	}
}
