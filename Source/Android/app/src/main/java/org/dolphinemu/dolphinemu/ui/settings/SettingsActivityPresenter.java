package org.dolphinemu.dolphinemu.ui.settings;

import android.content.IntentFilter;
import android.os.Bundle;
import android.text.TextUtils;

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

	private String menuTag;
	private String gameId;

	public SettingsActivityPresenter(SettingsActivityView view)
	{
		mView = view;
	}

	public void onCreate(Bundle savedInstanceState, String menuTag, String gameId)
	{
		if (savedInstanceState == null)
		{
			this.menuTag = menuTag;
			this.gameId = gameId;
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
			if (!TextUtils.isEmpty(gameId))
			{
				mSettings.add(SettingsFile.SETTINGS_DOLPHIN, SettingsFile.readFile("../GameSettings/" + gameId, mView));
				mSettings.add(SettingsFile.SETTINGS_GFX, SettingsFile.readFile("../GameSettings/" + gameId, mView));
				mSettings.add(SettingsFile.SETTINGS_WIIMOTE, SettingsFile.readFile("../GameSettings/" + gameId, mView));
			}
			else
			{
				mSettings.add(SettingsFile.SETTINGS_DOLPHIN, SettingsFile.readFile(SettingsFile.FILE_NAME_DOLPHIN, mView));
				mSettings.add(SettingsFile.SETTINGS_GFX, SettingsFile.readFile(SettingsFile.FILE_NAME_GFX, mView));
				mSettings.add(SettingsFile.SETTINGS_WIIMOTE, SettingsFile.readFile(SettingsFile.FILE_NAME_WIIMOTE, mView));
			}
		}

		mView.showSettingsFragment(menuTag, false, gameId);
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
				if (!TextUtils.isEmpty(gameId)) {
					Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
					// Needed workaround for now due to an odd bug in how it handles saving two different settings sections to the same file. It won't save GFX settings if it follows the normal saving pattern
					if (menuTag.equals("Dolphin"))
					{
						SettingsFile.saveFile("../GameSettings/" + gameId, mSettings.get(SettingsFile.SETTINGS_DOLPHIN), mView);
					}
					else if (menuTag.equals("GFX"))
					{
						SettingsFile.saveFile("../GameSettings/" + gameId, mSettings.get(SettingsFile.SETTINGS_GFX), mView);
					}
					mView.showToastMessage("Saved settings for " + gameId);
				} else {
					Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
					SettingsFile.saveFile(SettingsFile.FILE_NAME_DOLPHIN, mSettings.get(SettingsFile.SETTINGS_DOLPHIN), mView);
					SettingsFile.saveFile(SettingsFile.FILE_NAME_GFX, mSettings.get(SettingsFile.SETTINGS_GFX), mView);
					SettingsFile.saveFile(SettingsFile.FILE_NAME_WIIMOTE, mSettings.get(SettingsFile.SETTINGS_WIIMOTE), mView);
					mView.showToastMessage("Saved settings to INI files");
				}
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

	public void onGcPadSettingChanged(String key, int value)
	{
		if (value != 0) // Not disabled
		{
			mView.showSettingsFragment(key + (value / 6), true, gameId);
		}
	}

	public void onWiimoteSettingChanged(String section, int value)
	{
		switch (value)
		{
			case 1:
				mView.showSettingsFragment(section, true, gameId);
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
			mView.showSettingsFragment(key + value, true, gameId);
		}
	}
}
