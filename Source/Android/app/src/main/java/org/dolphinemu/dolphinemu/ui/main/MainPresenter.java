package org.dolphinemu.dolphinemu.ui.main;


import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AddDirectoryHelper;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import rx.android.schedulers.AndroidSchedulers;
import rx.schedulers.Schedulers;

public final class MainPresenter
{
	public static final int REQUEST_ADD_DIRECTORY = 1;
	public static final int REQUEST_EMULATE_GAME = 2;

	private final MainView mView;
	private String mDirToAdd;

	public MainPresenter(MainView view)
	{
		mView = view;
	}

	public void onCreate()
	{
		String versionName = BuildConfig.VERSION_NAME;
		mView.setVersionString(versionName);
	}

	public void onFabClick()
	{
		mView.launchFileListActivity();
	}

	public boolean handleOptionSelection(int itemId)
	{
		switch (itemId)
		{
			case R.id.menu_settings_core:
				mView.launchSettingsActivity(SettingsFile.FILE_NAME_DOLPHIN);
				return true;

			case R.id.menu_settings_video:
				mView.launchSettingsActivity(SettingsFile.FILE_NAME_GFX);
				return true;

			case R.id.menu_settings_gcpad:
				mView.launchSettingsActivity(SettingsFile.FILE_NAME_GCPAD);
				return true;

			case R.id.menu_settings_wiimote:
				mView.launchSettingsActivity(SettingsFile.FILE_NAME_WIIMOTE);
				return true;

			case R.id.menu_refresh:
				GameDatabase databaseHelper = DolphinApplication.databaseHelper;
				databaseHelper.scanLibrary(databaseHelper.getWritableDatabase());
				mView.refresh();
				return true;

			case R.id.button_add_directory:
				mView.launchFileListActivity();
				return true;
		}

		return false;
	}

	public void addDirIfNeeded(AddDirectoryHelper helper)
	{
		if (mDirToAdd != null)
		{
			helper.addDirectory(mDirToAdd, mView::refresh);

			mDirToAdd = null;
		}
	}

	public void onDirectorySelected(String dir)
	{
		mDirToAdd = dir;
	}

	public void refreshFragmentScreenshot(int resultCode)
	{
		mView.refreshFragmentScreenshot(resultCode);
	}


	public void loadGames(final Platform platform)
	{
		GameDatabase databaseHelper = DolphinApplication.databaseHelper;

		databaseHelper.getGamesForPlatform(platform)
				.subscribeOn(Schedulers.io())
				.observeOn(AndroidSchedulers.mainThread())
				.subscribe(games -> mView.showGames(platform, games));
	}
}
