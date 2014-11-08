/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import org.dolphinemu.dolphinemu.NativeLibrary;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

/**
 * A class that retrieves all of the set user preferences in Android, in a safe way.
 * <p>
 * If any preferences are added to this emulator, an accessor for that preference
 * should be added here. This way lengthy calls to getters from SharedPreferences
 * aren't made necessary.
 */
public final class UserPreferences
{
	/**
	 * Loads the settings stored in the Dolphin ini config files to the shared preferences of this front-end.
	 * 
	 * @param ctx The context used to retrieve the SharedPreferences instance.
	 */
	public static void LoadIniToPrefs(Context ctx)
	{
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);

		// Get an editor.
		SharedPreferences.Editor editor = prefs.edit();

		// Add the settings.
		editor.putString("cpuCorePref",   getConfig("Dolphin.ini", "Core", "CPUCore", "3"));
		editor.putBoolean("dualCorePref", getConfig("Dolphin.ini", "Core", "CPUThread", "False").equals("True"));
		editor.putBoolean("fastmemPref", getConfig("Dolphin.ini", "Core", "Fastmem", "False").equals("True"));

		editor.putString("gpuPref",               getConfig("Dolphin.ini", "Core", "GFXBackend", "Software Renderer"));
		editor.putBoolean("showFPS",              getConfig("gfx_opengl.ini", "Settings", "ShowFPS", "False").equals("True"));
		editor.putBoolean("drawOnscreenControls", getConfig("Dolphin.ini", "Android", "ScreenControls", "True").equals("True"));

		editor.putString("internalResolution",     getConfig("gfx_opengl.ini", "Settings", "EFBScale", "2") );
		editor.putString("FSAA",                   getConfig("gfx_opengl.ini", "Settings", "MSAA", "0"));
		editor.putString("anisotropicFiltering",   getConfig("gfx_opengl.ini", "Enhancements", "MaxAnisotropy", "0"));
		editor.putString("postProcessingShader",   getConfig("gfx_opengl.ini", "Enhancements", "PostProcessingShader", ""));
		editor.putBoolean("scaledEFBCopy",         getConfig("gfx_opengl.ini", "Hacks", "EFBScaledCopy", "True").equals("True"));
		editor.putBoolean("perPixelLighting",      getConfig("gfx_opengl.ini", "Settings", "EnablePixelLighting", "False").equals("True"));
		editor.putBoolean("forceTextureFiltering", getConfig("gfx_opengl.ini", "Enhancements", "ForceFiltering", "False").equals("True"));
		editor.putBoolean("disableFog",            getConfig("gfx_opengl.ini", "Settings", "DisableFog", "False").equals("True"));
		editor.putBoolean("skipEFBAccess",         getConfig("gfx_opengl.ini", "Hacks", "EFBAccessEnable", "False").equals("True"));
		editor.putBoolean("ignoreFormatChanges",   getConfig("gfx_opengl.ini", "Hacks", "EFBEmulateFormatChanges", "False").equals("False"));

		String efbCopyOn     = getConfig("gfx_opengl.ini", "Hacks", "EFBCopyEnable", "True");
		String efbToTexture  = getConfig("gfx_opengl.ini", "Hacks", "EFBToTextureEnable", "True");
		String efbCopyCache  = getConfig("gfx_opengl.ini", "Hacks", "EFBCopyCacheEnable", "False");

		if (efbCopyOn.equals("False"))
		{
			editor.putString("efbCopyMethod", "Off");
		}
		else if (efbCopyOn.equals("True") && efbToTexture.equals("True"))
		{
			editor.putString("efbCopyMethod", "Texture");
		}
		else if(efbCopyOn.equals("True") && efbToTexture.equals("False") && efbCopyCache.equals("False"))
		{
			editor.putString("efbCopyMethod", "RAM (uncached)");
		}
		else if(efbCopyOn.equals("True") && efbToTexture.equals("False") && efbCopyCache.equals("True"))
		{
			editor.putString("efbCopyMethod", "RAM (cached)");
		}

		editor.putString("textureCacheAccuracy", getConfig("gfx_opengl.ini", "Settings", "SafeTextureCacheColorSamples", "128"));

		String usingXFB = getConfig("gfx_opengl.ini", "Settings", "UseXFB", "False");
		String usingRealXFB = getConfig("gfx_opengl.ini", "Settings", "UseRealXFB", "False");

		if (usingXFB.equals("False"))
		{
			editor.putString("externalFrameBuffer", "Disabled");
		}
		else if (usingXFB.equals("True") && usingRealXFB.equals("False"))
		{
			editor.putString("externalFrameBuffer", "Virtual");
		}
		else if (usingXFB.equals("True") && usingRealXFB.equals("True"))
		{
			editor.putString("externalFrameBuffer", "Real");
		}

		editor.putBoolean("disableDestinationAlpha", getConfig("gfx_opengl.ini", "Settings", "DstAlphaPass", "False").equals("True"));
		editor.putBoolean("fastDepthCalculation",    getConfig("gfx_opengl.ini", "Settings", "FastDepthCalc", "True").equals("True"));

		// Apply the changes.
		editor.apply();
	}

	// Small utility method that shortens calls to NativeLibrary.GetConfig.
	private static String getConfig(String ini, String section, String key, String defaultValue)
	{
		return NativeLibrary.GetConfig(ini, section, key, defaultValue);
	}

	/** 
	 * Writes the preferences set in the front-end to the Dolphin ini files.
	 * 
	 * @param ctx The context used to retrieve the user settings.
	 * */
	public static void SavePrefsToIni(Context ctx)
	{
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);

		// Whether or not the user is using dual core.
		boolean isUsingDualCore = prefs.getBoolean("dualCorePref", true);

		// Current CPU core being used. Falls back to interpreter upon error.
		String currentEmuCore   = prefs.getString("cpuCorePref", "0");

		// Fastmem JIT core usage
		boolean isUsingFastmem = prefs.getBoolean("fastmemPref", false);

		// Current video backend being used. Falls back to software rendering upon error.
		String currentVideoBackend = prefs.getString("gpuPref", "Software Rendering");

		// Whether or not FPS will be displayed on-screen.
		boolean showingFPS = prefs.getBoolean("showFPS", false);

		// Whether or not to draw on-screen controls.
		boolean drawingOnscreenControls = prefs.getBoolean("drawOnscreenControls", true);

		// Whether or not to ignore all EFB access requests from the CPU.
		boolean skipEFBAccess = prefs.getBoolean("skipEFBAccess", false);

		// Whether or not to ignore changes to the EFB format.
		boolean ignoreFormatChanges = prefs.getBoolean("ignoreFormatChanges", false);

		// EFB copy method to use.
		String efbCopyMethod = prefs.getString("efbCopyMethod", "Texture");

		// Texture cache accuracy. Falls back to "Fast" up error.
		String textureCacheAccuracy = prefs.getString("textureCacheAccuracy", "128");

		// External frame buffer emulation. Falls back to disabled upon error.
		String externalFrameBuffer = prefs.getString("externalFrameBuffer", "Disabled");

		// Whether or not to disable destination alpha.
		boolean disableDstAlphaPass = prefs.getBoolean("disableDestinationAlpha", false);

		// Whether or not to use fast depth calculation.
		boolean useFastDepthCalc = prefs.getBoolean("fastDepthCalculation", true);

		// Internal resolution. Falls back to 1x Native upon error.
		String internalResolution = prefs.getString("internalResolution", "2");

		// FSAA Level. Falls back to 1x upon error.
		String FSAALevel = prefs.getString("FSAA", "0");

		// Anisotropic Filtering Level. Falls back to 1x upon error.
		String anisotropicFiltLevel = prefs.getString("anisotropicFiltering", "0");

		// Post processing shader setting
		String postProcessing = prefs.getString("postProcessingShader", "");

		// Whether or not Scaled EFB copies are used.
		boolean usingScaledEFBCopy = prefs.getBoolean("scaledEFBCopy", true);

		// Whether or not per-pixel lighting is used.
		boolean usingPerPixelLighting = prefs.getBoolean("perPixelLighting", false);

		// Whether or not texture filtering is being forced.
		boolean isForcingTextureFiltering = prefs.getBoolean("forceTextureFiltering", false);

		// Whether or not fog is disabled.
		boolean fogIsDisabled = prefs.getBoolean("disableFog", false);


		// CPU related Settings
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUCore", currentEmuCore);
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUThread", isUsingDualCore ? "True" : "False");
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "Fastmem", isUsingFastmem ? "True" : "False");

		// General Video Settings
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "GFXBackend", currentVideoBackend);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "ShowFPS", showingFPS ? "True" : "False");
		NativeLibrary.SetConfig("Dolphin.ini", "Android", "ScreenControls", drawingOnscreenControls ? "True" : "False");

		// Video Hack Settings
		NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBAccessEnable", skipEFBAccess ? "False" : "True");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBEmulateFormatChanges", ignoreFormatChanges ? "True" : "False");

		// Set EFB Copy Method 
		if (efbCopyMethod.equals("Off"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyEnable", "False");
		}
		else if (efbCopyMethod.equals("Texture"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyEnable", "True");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBToTextureEnable", "True");
		}
		else if (efbCopyMethod.equals("RAM (uncached)"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyEnable", "True");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBToTextureEnable", "False");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyCacheEnable", "False");
		}
		else if (efbCopyMethod.equals("RAM (cached)"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyEnable", "True");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBToTextureEnable", "False");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBCopyCacheEnable", "True");
		}

		// Set texture cache accuracy
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "SafeTextureCacheColorSamples", textureCacheAccuracy);

		// Set external frame buffer.
		if (externalFrameBuffer.equals("Disabled"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "UseXFB", "False");
		}
		else if (externalFrameBuffer.equals("Virtual"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "UseXFB", "True");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "UseRealXFB", "False");
		}
		else if (externalFrameBuffer.equals("Real"))
		{
			NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "UseXFB", "True");
			NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "UseRealXFB", "True");
		}

		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "DstAlphaPass", disableDstAlphaPass ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "FastDepthCalc", useFastDepthCalc ? "True" : "False");

		//-- Enhancement Settings --//
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "EFBScale", internalResolution);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "MSAA", FSAALevel);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Enhancements", "MaxAnisotropy", anisotropicFiltLevel);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Enhancements", "PostProcessingShader", postProcessing);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBScaledCopy", usingScaledEFBCopy ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "EnablePixelLighting", usingPerPixelLighting ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Enhancements", "ForceFiltering", isForcingTextureFiltering ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "DisableFog", fogIsDisabled ? "True" : "False");
	}
}
