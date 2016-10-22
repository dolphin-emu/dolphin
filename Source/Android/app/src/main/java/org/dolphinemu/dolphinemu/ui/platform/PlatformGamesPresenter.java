package org.dolphinemu.dolphinemu.ui.platform;


import android.database.Cursor;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.utils.Log;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

public final class PlatformGamesPresenter
{
	private final PlatformGamesView mView;

	private int mPlatform;

	public PlatformGamesPresenter(PlatformGamesView view)
	{
		mView = view;
	}

	public void onCreate(int platform)
	{
		mPlatform = platform;
	}

	public void onCreateView()
	{
		loadGames();
	}

	public void refresh()
	{
		Log.debug("[PlatformGamesPresenter] " + mPlatform + ": Refreshing...");
		loadGames();
	}

	private void loadGames()
	{
		Log.debug("[PlatformGamesPresenter] " + mPlatform + ": Loading games...");

		GameDatabase databaseHelper = DolphinApplication.databaseHelper;

		databaseHelper.getGamesForPlatform(mPlatform)
				.subscribeOn(Schedulers.io())
				.observeOn(AndroidSchedulers.mainThread())
				.subscribe(new Action1<Cursor>()
				{
					@Override
					public void call(Cursor games)
					{
						Log.debug("[PlatformGamesPresenter] " + mPlatform + ": Load finished, swapping cursor...");

						mView.showGames(games);
					}
				});
	}
}
