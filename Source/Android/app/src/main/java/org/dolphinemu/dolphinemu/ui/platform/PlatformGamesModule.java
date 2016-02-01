package org.dolphinemu.dolphinemu.ui.platform;

import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;
import org.dolphinemu.dolphinemu.utils.Log;

import dagger.Module;
import dagger.Provides;

@Module
public class PlatformGamesModule
{
	private PlatformGamesView view;

	public PlatformGamesModule(PlatformGamesView view)
	{
		this.view = view;
	}

	@Provides
	@FragmentScoped
	public PlatformGamesView provideView()
	{
		Log.verbose("[PlatformGamesModule] Providing view...");
		return view;
	}
}
