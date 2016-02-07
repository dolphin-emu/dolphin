package org.dolphinemu.dolphinemu.ui.files;

import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;
import org.dolphinemu.dolphinemu.utils.Log;

import dagger.Module;
import dagger.Provides;

@Module
public class AddDirectoryModule
{
	private AddDirectoryView view;

	public AddDirectoryModule(AddDirectoryView view)
	{
		this.view = view;
	}

	@Provides
	@ActivityScoped
	public AddDirectoryView provideView()
	{
		Log.verbose("[AddDirectoryModule] Providing view...");
		return view;
	}
}
