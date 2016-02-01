package org.dolphinemu.dolphinemu.ui.main;

import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;
import org.dolphinemu.dolphinemu.utils.Log;

import dagger.Module;
import dagger.Provides;

@Module
public class MainModule
{
	private MainView view;

	public MainModule(MainView view)
	{
		this.view = view;
	}

	@Provides
	@ActivityScoped
	public MainView provideView()
	{
		Log.verbose("[MainModule] Providing view...");
		return view;
	}
}
