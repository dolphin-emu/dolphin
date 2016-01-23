package org.dolphinemu.dolphinemu.ui.settings;


import android.os.Bundle;

import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.HashMap;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

public final class SettingsActivityPresenter
{
	private SettingsActivityView mView;

	private String mFileName;
	private HashMap<String, SettingSection> mSettingsBySection;

	private int mStackCount;

	public SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, final String filename)
	{
		mFileName = filename;

		if (savedInstanceState == null)
		{
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
									mView.onSettingsFileLoaded(null);
								}
							});

			mView.showSettingsFragment(mFileName, false);
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
		if (mSettingsBySection != null && finishing)
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
}
