package org.dolphinemu.dolphinemu.ui.settings;

import android.content.IntentFilter;
import android.os.Bundle;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService.DirectoryInitializationState;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

import rx.functions.Action1;

public final class SettingsActivityPresenter
{
	private static final String KEY_SHOULD_SAVE = "should_save";

	private SettingsActivityView mView;

	private ArrayList<HashMap<String, SettingSection>> mSettings = new ArrayList<>();

	private int mStackCount;

	private boolean mShouldSave;

	private DirectoryStateReceiver directoryStateReceiver;

	private MenuTag menuTag;

	public SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, MenuTag menuTag)
	{
		if (savedInstanceState == null)
		{
			this.menuTag = menuTag;
		}
		else
		{
			mShouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
		}
	}

	public void onStart()
	{
		prepareDolphinDirectoriesIfNeeded();
	}

	void loadSettingsUI()
	{
		if (mSettings.isEmpty())
		{
			mSettings.add(SettingsFile.SETTINGS_DOLPHIN, SettingsFile.readFile(SettingsFile.FILE_NAME_DOLPHIN, mView));
			mSettings.add(SettingsFile.SETTINGS_GFX, SettingsFile.readFile(SettingsFile.FILE_NAME_GFX, mView));
			mSettings.add(SettingsFile.SETTINGS_WIIMOTE, SettingsFile.readFile(SettingsFile.FILE_NAME_WIIMOTE, mView));
		}

		mView.showSettingsFragment(menuTag, null, false);
		mView.onSettingsFileLoaded(mSettings);
	}

	private void prepareDolphinDirectoriesIfNeeded()
	{
		if (DirectoryInitializationService.areDolphinDirectoriesReady()) {
			loadSettingsUI();
		} else {
			mView.showLoading();
			IntentFilter statusIntentFilter = new IntentFilter(
					DirectoryInitializationService.BROADCAST_ACTION);

			directoryStateReceiver =
					new DirectoryStateReceiver(directoryInitializationState ->
					{
						if (directoryInitializationState == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
						{
							mView.hideLoading();
							loadSettingsUI();
						}
						else if (directoryInitializationState == DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED)
						{
							mView.showPermissionNeededHint();
							mView.hideLoading();
						}
						else if (directoryInitializationState == DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE)
						{
							mView.showExternalStorageNotMountedHint();
							mView.hideLoading();
						}
					});

			mView.startDirectoryInitializationService(directoryStateReceiver, statusIntentFilter);
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
		if (directoryStateReceiver != null)
		{
			mView.stopListeningToDirectoryInitializationService(directoryStateReceiver);
			directoryStateReceiver = null;
		}

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
		outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
	}

	public void onGcPadSettingChanged(MenuTag key, int value)
	{
		if (value != 0) // Not disabled
		{
			Bundle bundle = new Bundle();
			bundle.putInt(SettingsFragmentPresenter.ARG_CONTROLLER_TYPE, value/6);
			mView.showSettingsFragment(key, bundle, true);
		}
	}

	public void onWiimoteSettingChanged(MenuTag menuTag, int value)
	{
		switch (value)
		{
			case 1:
				mView.showSettingsFragment(menuTag, null, true);
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
			mView.showSettingsFragment(menuTag, bundle, true);
		}
	}
}
