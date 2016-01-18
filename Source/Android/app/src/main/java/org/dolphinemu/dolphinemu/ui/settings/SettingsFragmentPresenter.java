package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.model.settings.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SliderSetting;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

public class SettingsFragmentPresenter
{
	private SettingsFragmentView mView;

	private String mMenuTag;

	private HashMap<String, SettingSection> mSettings;
	private ArrayList<SettingsItem> mSettingsList;

	public SettingsFragmentPresenter(SettingsFragmentView view)
	{
		mView = view;
	}

	public void onCreate(String menuTag)
	{
		mMenuTag = menuTag;
	}

	public void onViewCreated(HashMap<String, SettingSection> settings)
	{
		setSettings(settings);
	}

	/**
	 * If the screen is rotated, the Activity will forget the settings map. This fragment
	 * won't, though; so rather than have the Activity reload from disk, have the fragment pass
	 * the settings map back to the Activity.
	 */
	public void onAttach()
	{
		if (mSettings != null)
		{
			mView.passOptionsToActivity(mSettings);
		}
	}

	public void setSettings(HashMap<String, SettingSection> settings)
	{
		if (mSettingsList == null)
		{
			if (settings != null)
			{
				mSettings = settings;
			}

			if (mSettings != null)
			{
				loadSettingsList();
			}
		}
		else
		{
			mView.showSettingsList(mSettingsList);
		}
	}

	private void loadSettingsList()
	{
		ArrayList<SettingsItem> sl = new ArrayList<>();

		switch (mMenuTag)
		{
			case SettingsFile.FILE_NAME_DOLPHIN:
				addCoreSettings(sl);

				break;
		}

		mSettingsList = sl;
		mView.showSettingsList(mSettingsList);
	}

	private void addCoreSettings(ArrayList<SettingsItem> sl)
	{
		Setting cpuCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_CPU_CORE);
		sl.add(new SingleChoiceSetting(cpuCore.getKey(), cpuCore, R.string.cpu_core, 0, R.array.string_emu_cores, R.array.int_emu_cores));

		Setting dualCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_DUAL_CORE);
		sl.add(new CheckBoxSetting(dualCore.getKey(), dualCore, R.string.dual_core, R.string.dual_core_descrip));

		Setting overclockEnable = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_ENABLE);
		sl.add(new CheckBoxSetting(overclockEnable.getKey(), overclockEnable, R.string.overclock_enable, R.string.overclock_enable_description));

		Setting overclock = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_PERCENT);
		sl.add(new SliderSetting(overclock.getKey(), overclock, R.string.overclock_title, 0, 400));
	}
}
