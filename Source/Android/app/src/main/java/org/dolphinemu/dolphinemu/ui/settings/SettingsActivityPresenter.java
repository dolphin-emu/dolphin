package org.dolphinemu.dolphinemu.ui.settings;


import android.os.Bundle;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.HashMap;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

public final class SettingsActivityPresenter
{
	private static final String SHOULD_SAVE = BuildConfig.APPLICATION_ID + ".should_save";

	private SettingsActivityView mView;

	private String mFileName;
	private HashMap<String, SettingSection> mSettingsBySection;

	private int mStackCount;

	private boolean mShouldSave;

	public SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, final String filename)
	{
		mFileName = filename;

		if (savedInstanceState == null)
		{
			// TODO DI should be able to get rid of this hack
			if (filename.equals(SettingsFile.FILE_NAME_GCPAD))
			{
				// Psyche! Don't actually load that file (yet).
				mFileName = SettingsFile.FILE_NAME_DOLPHIN;

				// But do display its fragment, as if we had.
				mView.showSettingsFragment(SettingsFile.FILE_NAME_GCPAD, false);
			}
			else
			{
				mFileName = filename;
				mView.showSettingsFragment(mFileName, false);
			}

			SettingsFile.readFile(mFileName)
					.subscribeOn(Schedulers.io())
					.observeOn(AndroidSchedulers.mainThread())
					.subscribe(new Action1<HashMap<String, SettingSection>>()
							   {
								   @Override
								   public void call(HashMap<String, SettingSection> settingsBySection)
								   {
									   mSettingsBySection = settingsBySection;
									   mView.onSettingsFileLoaded(settingsBySection);
								   }
							   },
							new Action1<Throwable>()
							{
								@Override
								public void call(Throwable throwable)
								{
									Log.error("[SettingsActivityPresenter] Error reading file " + filename + ".ini: "+ throwable.getMessage());
									mView.onSettingsFileNotFound();
								}
							});
		}
		else
		{
			mShouldSave = savedInstanceState.getBoolean(SHOULD_SAVE);
		}
	}

	public void setSettings(HashMap<String, SettingSection> settings)
	{
		mSettingsBySection = settings;
	}

	public HashMap<String, SettingSection> getSettings()
	{
		return mSettingsBySection;
	}

	public void onStop(boolean finishing)
	{
		if (mSettingsBySection != null && finishing && mShouldSave)
		{
			Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
			SettingsFile.saveFile(mFileName, mSettingsBySection)
					.subscribeOn(Schedulers.io())
					.observeOn(AndroidSchedulers.mainThread())
					.subscribe(
							new Action1<Boolean>()
							{
								@Override
								public void call(Boolean aBoolean)
								{
									mView.showToastMessage("Saved successfully to " + mFileName + ".ini");
								}
							},
							new Action1<Throwable>()
							{
								@Override
								public void call(Throwable throwable)
								{
									mView.showToastMessage("Error saving " + mFileName + ".ini: " + throwable.getMessage());
								}
							});
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
}
