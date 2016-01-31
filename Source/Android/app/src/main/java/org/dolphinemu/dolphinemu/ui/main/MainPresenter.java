package org.dolphinemu.dolphinemu.ui.main;


import android.database.Cursor;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import javax.inject.Inject;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

@ActivityScoped
public final class MainPresenter
{
	public static final int REQUEST_ADD_DIRECTORY = 1;
	public static final int REQUEST_EMULATE_GAME = 2;

	private final MainView mView;
	private final GameDatabase mGameDatabase;

	@Inject
	public MainPresenter(MainView view, GameDatabase gameDatabase)
	{
		mView = view;
		mGameDatabase = gameDatabase;
	}

	public void onCreate()
	{
		// TODO Rather than calling into native code, this should use the commented line below.
		// String versionName = BuildConfig.VERSION_NAME;
		String versionName = NativeLibrary.GetVersionString();
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

			case R.id.menu_refresh:
				mView.refresh();
				return true;

			case R.id.button_add_directory:
				mView.launchFileListActivity();
				return true;
		}

		return false;
	}

	public void handleActivityResult(int requestCode, int resultCode)
	{
		switch (requestCode)
		{
			case REQUEST_ADD_DIRECTORY:
				// If the user picked a file, as opposed to just backing out.
				if (resultCode == MainActivity.RESULT_OK)
				{
					mView.refresh();
				}
				break;

			case REQUEST_EMULATE_GAME:
				mView.refreshFragmentScreenshot(resultCode);
				break;
		}
	}

	public void loadGames(final int platformIndex)
	{
		mGameDatabase.getGamesForPlatform(platformIndex)
				.subscribeOn(Schedulers.io())
				.observeOn(AndroidSchedulers.mainThread())
				.subscribe(new Action1<Cursor>()
						   {
							   @Override
							   public void call(Cursor games)
							   {
								   mView.showGames(platformIndex, games);
							   }
						   }
				);
	}
}
