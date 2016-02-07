package org.dolphinemu.dolphinemu.ui.platform;


import android.database.Cursor;

import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.utils.Log;

import javax.inject.Inject;

import rx.android.schedulers.AndroidSchedulers;
import rx.functions.Action1;
import rx.schedulers.Schedulers;

@FragmentScoped
public final class PlatformGamesPresenter
{
	private final PlatformGamesView mView;
	private final GameDatabase mGameDatabase;

	private int mPlatform;

	@Inject
	public PlatformGamesPresenter(PlatformGamesView view, GameDatabase gameDatabase)
	{
		mView = view;
		mGameDatabase = gameDatabase;
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

		mGameDatabase.getGamesForPlatform(mPlatform)
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
						   }
				);
	}
}
