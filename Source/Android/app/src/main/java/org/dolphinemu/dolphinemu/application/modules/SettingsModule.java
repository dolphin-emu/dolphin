package org.dolphinemu.dolphinemu.application.modules;

import org.dolphinemu.dolphinemu.application.qualifiers.Named;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.HashMap;

import javax.inject.Singleton;

import dagger.Module;
import dagger.Provides;

@Module
public class SettingsModule
{
	@Provides
	@Singleton
	@Named(SettingsFile.FILE_NAME_DOLPHIN)
	HashMap<String, SettingSection> provideDolphinSettings()
	{
		Log.verbose("[SettingsModule] Providing Dolphin Settings...");

		// TODO Actually load from the file
		return new HashMap<>();
	}

	@Provides
	@Singleton
	@Named(SettingsFile.FILE_NAME_GFX)
	HashMap<String, SettingSection> provideGfxSettings()
	{
		Log.verbose("[SettingsModule] Providing Graphics Settings...");

		// TODO Actually load from the file
		return new HashMap<>();
	}
}
