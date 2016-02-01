package org.dolphinemu.dolphinemu.application;


import org.dolphinemu.dolphinemu.application.modules.AppModule;
import org.dolphinemu.dolphinemu.application.modules.DatabaseModule;
import org.dolphinemu.dolphinemu.application.modules.SettingsModule;
import org.dolphinemu.dolphinemu.ui.files.AddDirectoryComponent;
import org.dolphinemu.dolphinemu.ui.files.AddDirectoryModule;
import org.dolphinemu.dolphinemu.ui.files.FileListComponent;
import org.dolphinemu.dolphinemu.ui.files.FileListModule;
import org.dolphinemu.dolphinemu.ui.main.MainComponent;
import org.dolphinemu.dolphinemu.ui.main.MainModule;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesComponent;
import org.dolphinemu.dolphinemu.ui.platform.PlatformGamesModule;

import javax.inject.Singleton;

import dagger.Component;

@Singleton
@Component(modules =
		{
				AppModule.class,
				DatabaseModule.class,
				SettingsModule.class
		}
)
public interface AppComponent
{
	/**
	 * Since MainComponent is an {@link dagger.Subcomponent}, Dagger's generated
	 * implementation of this class ({@link DaggerAppComponent} will allow us to
	 * get a generated implementation of MainComponent (DaggerMainComponent) with
	 * access to all of AppComponent's dependency modules.
	 */
	MainComponent plus(MainModule module);

	PlatformGamesComponent plus(PlatformGamesModule module);

	AddDirectoryComponent plus(AddDirectoryModule module);

	FileListComponent plus(FileListModule module);
}

