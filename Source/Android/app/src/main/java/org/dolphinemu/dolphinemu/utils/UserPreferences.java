/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.NativeLibrary;

/**
 * A class that retrieves all of the set user preferences in Android, in a safe way.
 * <p>
 * If any preferences are added to this emulator, an accessor for that preference
 * should be added here. This way lengthy calls to getters from SharedPreferences
 * aren't made necessary.
 */
public final class UserPreferences
{
	private UserPreferences()
	{
		// Disallows instantiation.
	}

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
		if (Build.CPU_ABI.contains("arm64"))
			editor.putString("cpuCorePref",   getConfig("Dolphin.ini", "Core", "CPUCore", "4"));
		else
			editor.putString("cpuCorePref",   getConfig("Dolphin.ini", "Core", "CPUCore", "3"));

		editor.putBoolean("dualCorePref", getConfig("Dolphin.ini", "Core", "CPUThread", "True").equals("True"));
		editor.putBoolean("OverclockEnable", getConfig("Dolphin.ini", "Core", "OverclockEnable", "False").equals("True"));
		editor.putString("Overclock", getConfig("Dolphin.ini", "Core", "Overclock", "100"));

		// Load analog ranges from GCPadNew.ini and WiimoteNew.ini
		editor.putString("mainRadius0", getConfig("GCPadNew.ini", "GCPad1", "Main Stick/Radius", "100,000000"));
		editor.putString("cStickRadius0", getConfig("GCPadNew.ini", "GCPad1", "C-Stick/Radius", "100,000000"));
		editor.putString("inputThres0", getConfig("GCPadNew.ini", "GCPad1", "Triggers/Threshold", "90,000000"));
		editor.putString("mainRadius1", getConfig("GCPadNew.ini", "GCPad2", "Main Stick/Radius", "100,000000"));
		editor.putString("cStickRadius1", getConfig("GCPadNew.ini", "GCPad2", "C-Stick/Radius", "100,000000"));
		editor.putString("inputThres1", getConfig("GCPadNew.ini", "GCPad2", "Triggers/Threshold", "90,000000"));
		editor.putString("mainRadius2", getConfig("GCPadNew.ini", "GCPad3", "Main Stick/Radius", "100,000000"));
		editor.putString("cStickRadius2", getConfig("GCPadNew.ini", "GCPad3", "C-Stick/Radius", "100,000000"));
		editor.putString("inputThres2", getConfig("GCPadNew.ini", "GCPad3", "Triggers/Threshold", "90,000000"));
		editor.putString("mainRadius3", getConfig("GCPadNew.ini", "GCPad4", "Main Stick/Radius", "100,000000"));
		editor.putString("cStickRadius3", getConfig("GCPadNew.ini", "GCPad4", "C-Stick/Radius", "100,000000"));
		editor.putString("inputThres3", getConfig("GCPadNew.ini", "GCPad4", "Triggers/Threshold", "90,000000"));

		editor.putString("tiltRange4", getConfig("WiimoteNew.ini", "Wiimote1", "Tilt/Modifier/Range", "50,00000"));
		editor.putString("tiltRange5", getConfig("WiimoteNew.ini", "Wiimote2", "Tilt/Modifier/Range", "50,00000"));
		editor.putString("tiltRange6", getConfig("WiimoteNew.ini", "Wiimote3", "Tilt/Modifier/Range", "50,00000"));
		editor.putString("tiltRange7", getConfig("WiimoteNew.ini", "Wiimote4", "Tilt/Modifier/Range", "50,00000"));

		editor.putString("nunchukRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Nunchuk/Stick/Radius", "100,000000"));
		editor.putString("nunchukRange4", getConfig("WiimoteNew.ini", "Wiimote1", "Nunchuk/Tilt/Modifier/Range", "50,00000"));
		editor.putString("nunchukRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Nunchuk/Stick/Radius", "100,000000"));
		editor.putString("nunchukRange5", getConfig("WiimoteNew.ini", "Wiimote2", "Nunchuk/Tilt/Modifier/Range", "50,00000"));
		editor.putString("nunchukRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Nunchuk/Stick/Radius", "100,000000"));
		editor.putString("nunchukRange6", getConfig("WiimoteNew.ini", "Wiimote3", "Nunchuk/Tilt/Modifier/Range", "50,00000"));
		editor.putString("nunchukRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Nunchuk/Stick/Radius", "100,000000"));
		editor.putString("nunchukRange7", getConfig("WiimoteNew.ini", "Wiimote4", "Nunchuk/Tilt/Modifier/Range", "50,00000"));

		editor.putString("classicLRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Classic/Left Stick/Radius", "100,000000"));
		editor.putString("classicRRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Classic/Right Stick/Radius", "100,000000"));
		editor.putString("classicThres4", getConfig("WiimoteNew.ini", "Wiimote1", "Classic/Triggers/Threshold", "90,000000"));
		editor.putString("classicLRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Classic/Left Stick/Radius", "100,000000"));
		editor.putString("classicRRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Classic/Right Stick/Radius", "100,000000"));
		editor.putString("classicThres5", getConfig("WiimoteNew.ini", "Wiimote2", "Classic/Triggers/Threshold", "90,000000"));
		editor.putString("classicLRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Classic/Left Stick/Radius", "100,000000"));
		editor.putString("classicRRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Classic/Right Stick/Radius", "100,000000"));
		editor.putString("classicThres6", getConfig("WiimoteNew.ini", "Wiimote3", "Classic/Triggers/Threshold", "90,000000"));
		editor.putString("classicLRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Classic/Left Stick/Radius", "100,000000"));
		editor.putString("classicRRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Classic/Right Stick/Radius", "100,000000"));
		editor.putString("classicThres7", getConfig("WiimoteNew.ini", "Wiimote4", "Classic/Triggers/Threshold", "90,000000"));

		editor.putString("guitarRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Guitar/Stick/Radius", "100,000000"));
		editor.putString("guitarRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Guitar/Stick/Radius", "100,000000"));
		editor.putString("guitarRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Guitar/Stick/Radius", "100,000000"));
		editor.putString("guitarRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Guitar/Stick/Radius", "100,000000"));

		editor.putString("drumsRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Drums/Stick/Radius", "100,000000"));
		editor.putString("drumsRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Drums/Stick/Radius", "100,000000"));
		editor.putString("drumsRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Drums/Stick/Radius", "100,000000"));
		editor.putString("drumsRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Drums/Stick/Radius", "100,000000"));

		editor.putString("turntableRadius4", getConfig("WiimoteNew.ini", "Wiimote1", "Turntable/Stick/Radius", "100,000000"));
		editor.putString("turntableRadius5", getConfig("WiimoteNew.ini", "Wiimote2", "Turntable/Stick/Radius", "100,000000"));
		editor.putString("turntableRadius6", getConfig("WiimoteNew.ini", "Wiimote3", "Turntable/Stick/Radius", "100,000000"));
		editor.putString("turntableRadius7", getConfig("WiimoteNew.ini", "Wiimote4", "Turntable/Stick/Radius", "100,000000"));

		// Load Wiimote Extension settings from WiimoteNew.ini
		editor.putString("wiimoteExtension4", getConfig("WiimoteNew.ini", "Wiimote1", "Extension", "None"));
		editor.putString("wiimoteExtension5", getConfig("WiimoteNew.ini", "Wiimote2", "Extension", "None"));
		editor.putString("wiimoteExtension6", getConfig("WiimoteNew.ini", "Wiimote3", "Extension", "None"));
		editor.putString("wiimoteExtension7", getConfig("WiimoteNew.ini", "Wiimote4", "Extension", "None"));

		editor.putString("gpuPref",               getConfig("Dolphin.ini", "Core", "GFXBackend", "OGL"));
		editor.putBoolean("showFPS",              getConfig("gfx_opengl.ini", "Settings", "ShowFPS", "False").equals("True"));
		editor.putBoolean("drawOnscreenControls", getConfig("Dolphin.ini", "Android", "ScreenControls", "True").equals("True"));

		editor.putString("internalResolution",     getConfig("gfx_opengl.ini", "Settings", "EFBScale", "2"));
		editor.putString("FSAA",                   getConfig("gfx_opengl.ini", "Settings", "MSAA", "0"));
		editor.putString("anisotropicFiltering",   getConfig("gfx_opengl.ini", "Enhancements", "MaxAnisotropy", "0"));
		editor.putString("postProcessingShader",   getConfig("gfx_opengl.ini", "Enhancements", "PostProcessingShader", ""));
		editor.putBoolean("scaledEFBCopy",         getConfig("gfx_opengl.ini", "Hacks", "EFBScaledCopy", "True").equals("True"));
		editor.putBoolean("perPixelLighting",      getConfig("gfx_opengl.ini", "Settings", "EnablePixelLighting", "False").equals("True"));
		editor.putBoolean("forceTextureFiltering", getConfig("gfx_opengl.ini", "Enhancements", "ForceFiltering", "False").equals("True"));
		editor.putBoolean("disableFog",            getConfig("gfx_opengl.ini", "Settings", "DisableFog", "False").equals("True"));
		editor.putBoolean("skipEFBAccess",         getConfig("gfx_opengl.ini", "Hacks", "EFBAccessEnable", "False").equals("True"));
		editor.putBoolean("ignoreFormatChanges",   getConfig("gfx_opengl.ini", "Hacks", "EFBEmulateFormatChanges", "False").equals("True"));
		editor.putString("stereoscopyMode",        getConfig("gfx_opengl.ini", "Stereoscopy", "StereoMode", "0"));
		editor.putBoolean("stereoSwapEyes",        getConfig("gfx_opengl.ini", "Stereoscopy", "StereoSwapEyes", "False").equals("True"));
		editor.putString("stereoDepth",            getConfig("gfx_opengl.ini", "Stereoscopy", "StereoDepth", "20"));
		editor.putString("stereoConvergencePercentage", getConfig("gfx_opengl.ini", "Stereoscopy", "StereoConvergencePercentage", "100"));
		editor.putBoolean("enableController1",     getConfig("Dolphin.ini", "Settings", "SIDevice0", "6") == "6");
		editor.putBoolean("enableController2",     getConfig("Dolphin.ini", "Settings", "SIDevice1", "0") == "6");
		editor.putBoolean("enableController3",     getConfig("Dolphin.ini", "Settings", "SIDevice2", "0") == "6");
		editor.putBoolean("enableController4",     getConfig("Dolphin.ini", "Settings", "SIDevice3", "0") == "6");

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

		editor.putBoolean("fastDepthCalculation",    getConfig("gfx_opengl.ini", "Settings", "FastDepthCalc", "True").equals("True"));
		editor.putString("aspectRatio", getConfig("gfx_opengl.ini", "Settings", "AspectRatio", "0"));

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
		String currentEmuCore = prefs.getString("cpuCorePref", "0");

		boolean overclockEnabled = prefs.getBoolean("OverclockEnable", false);
		String overclockSetting =  prefs.getString("Overclock", "100");

		// Current GC analog range setup. Falls back to default upon error.
		String currentMainRadius0 = prefs.getString("mainRadius0", "100,000000");
		String currentCStickRadius0 = prefs.getString("cStickRadius0", "100,000000");
		String currentInputThres0 = prefs.getString("inputThres0", "90,000000");
		String currentMainRadius1 = prefs.getString("mainRadius1", "100,000000");
		String currentCStickRadius1 = prefs.getString("cStickRadius1", "100,000000");
		String currentInputThres1 = prefs.getString("inputThres1", "90,000000");
		String currentMainRadius2 = prefs.getString("mainRadius2", "100,000000");
		String currentCStickRadius2 = prefs.getString("cStickRadius2", "100,000000");
		String currentInputThres2 = prefs.getString("inputThres2", "90,000000");
		String currentMainRadius3 = prefs.getString("mainRadius3", "100,000000");
		String currentCStickRadius3 = prefs.getString("cStickRadius3", "100,000000");
		String currentInputThres3 = prefs.getString("inputThres3", "90,000000");

		// Current Wii analog range setup. Falls back to default on error.
		String currentTiltRange4 = prefs.getString("tiltRange4", "50,000000");
		String currentTiltRange5 = prefs.getString("tiltRange5", "50,000000");
		String currentTiltRange6 = prefs.getString("tiltRange6", "50,000000");
		String currentTiltRange7 = prefs.getString("tiltRange7", "50,000000");

		// Current Nunchuk analog range setup. Falls back to default upon error.
		String currentNunchukRadius4 = prefs.getString("nunchukRadius4", "100,000000");
		String currentNunchukRange4 = prefs.getString("nunchukRange4", "50,000000");
		String currentNunchukRadius5 = prefs.getString("nunchukRadius5", "100,000000");
		String currentNunchukRange5 = prefs.getString("nunchukRange5", "50,000000");
		String currentNunchukRadius6 = prefs.getString("nunchukRadius6", "100,000000");
		String currentNunchukRange6 = prefs.getString("nunchukRange6", "50,000000");
		String currentNunchukRadius7 = prefs.getString("nunchukRadius7", "100,000000");
		String currentNunchukRange7 = prefs.getString("nunchukRange7", "50,000000");

		// Current Classic analog range setup. Falls back to 100,000000 upon error.
		String currentClassicLRadius4 = prefs.getString("classicLRadius4", "100,000000");
		String currentClassicRRadius4 = prefs.getString("classicRRadius4", "100,000000");
		String currentClassicThres4 = prefs.getString("classicThres4", "90,000000");
		String currentClassicLRadius5 = prefs.getString("classicLRadius5", "100,000000");
		String currentClassicRRadius5 = prefs.getString("classicRRadius5", "100,000000");
		String currentClassicThres5 = prefs.getString("classicThres5", "90,000000");
		String currentClassicLRadius6 = prefs.getString("classicLRadius6", "100,000000");
		String currentClassicRRadius6 = prefs.getString("classicRRadius6", "100,000000");
		String currentClassicThres6 = prefs.getString("classicThres6", "90,000000");
		String currentClassicLRadius7 = prefs.getString("classicLRadius7", "100,000000");
		String currentClassicRRadius7 = prefs.getString("classicRRadius7", "100,000000");
		String currentClassicThres7 = prefs.getString("classicThres7", "90,000000");

		// Current Guitar analog range setup. Falls back to default upon error.
		String currentGuitarRadius4 = prefs.getString("guitarRadius4", "100,000000");
		String currentGuitarRadius5 = prefs.getString("guitarRadius5", "100,000000");
		String currentGuitarRadius6 = prefs.getString("guitarRadius6", "100,000000");
		String currentGuitarRadius7 = prefs.getString("guitarRadius7", "100,000000");

		// Current Drums modifier Radius setup. Falls back to default upon error.
		String currentDrumsRadius4 = prefs.getString("drumsRadius4", "100,000000");
		String currentDrumsRadius5 = prefs.getString("drumsRadius5", "100,000000");
		String currentDrumsRadius6 = prefs.getString("drumsRadius6", "100,000000");
		String currentDrumsRadius7 = prefs.getString("drumsRadius7", "100,000000");

		// Current Turntable analog range setup. Falls back to default upon error.
		String currentTurntableRadius4 = prefs.getString("turntableRadius4", "100,000000");
		String currentTurntableRadius5 = prefs.getString("turntableRadius5", "100,000000");
		String currentTurntableRadius6 = prefs.getString("turntableRadius6", "100,000000");
		String currentTurntableRadius7 = prefs.getString("turntableRadius7", "100,000000");

		// Current wiimote extension setup. Falls back to no extension upon error.
		String currentWiimoteExtension4 = prefs.getString("wiimoteExtension4", "None");
		String currentWiimoteExtension5 = prefs.getString("wiimoteExtension5", "None");
		String currentWiimoteExtension6 = prefs.getString("wiimoteExtension6", "None");
		String currentWiimoteExtension7 = prefs.getString("wiimoteExtension7", "None");

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

		// Whether or not to use fast depth calculation.
		boolean useFastDepthCalc = prefs.getBoolean("fastDepthCalculation", true);

		// Aspect ratio selection
		String aspectRatio = prefs.getString("aspectRatio", "0");

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

		// Stereoscopy setting
		String stereoscopyMode = prefs.getString("stereoscopyMode", "0");

		// Stereoscopy swap eyes
		boolean stereoscopyEyeSwap = prefs.getBoolean("stereoSwapEyes", false);

		// Stereoscopy separation
		String stereoscopySeparation = prefs.getString("stereoDepth", "20");

		// Stereoscopy convergence
		String stereoConvergencePercentage = prefs.getString("stereoConvergencePercentage", "100");

		// Controllers
		// Controller 1 never gets disconnected due to touch screen
		//boolean enableController1 = prefs.getBoolean("enableController1", true);
		boolean enableController2 = prefs.getBoolean("enableController2", false);
		boolean enableController3 = prefs.getBoolean("enableController3", false);
		boolean enableController4 = prefs.getBoolean("enableController4", false);

		// CPU related Settings
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUCore", currentEmuCore);
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUThread", isUsingDualCore ? "True" : "False");

		NativeLibrary.SetConfig("Dolphin.ini", "Core", "OverclockEnable", overclockEnabled ? "True" : "False");
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "Overclock", overclockSetting);

		// GameCube analog ranges Setup
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad1", "Main Stick/Radius", currentMainRadius0);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad1", "C-Stick/Radius", currentCStickRadius0);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad1", "Triggers/Threshold", currentInputThres0);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad2", "Main Stick/Radius", currentMainRadius1);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad2", "C-Stick/Radius", currentCStickRadius1);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad2", "Triggers/Threshold", currentInputThres1);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad3", "Main Stick/Radius", currentMainRadius2);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad3", "C-Stick/Radius", currentCStickRadius2);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad3", "Triggers/Threshold", currentInputThres2);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad4", "Main Stick/Radius", currentMainRadius3);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad4", "C-Stick/Radius", currentCStickRadius3);
		NativeLibrary.SetConfig("GCPadNew.ini", "GCPad4", "Triggers/Threshold", currentInputThres3);

		// Wiimote analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Tilt/Modifier/Range", currentTiltRange4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Tilt/Modifier/Range", currentTiltRange5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Tilt/Modifier/Range", currentTiltRange6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Tilt/Modifier/Range", currentTiltRange7);

		// Nunchuk analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Nunchuk/Stick/Radius", currentNunchukRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Nunchuk/Stick/Radius", currentNunchukRange4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Nunchuk/Stick/Radius", currentNunchukRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Nunchuk/Stick/Radius", currentNunchukRange5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Nunchuk/Stick/Radius", currentNunchukRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Nunchuk/Stick/Radius", currentNunchukRange6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Nunchuk/Stick/Radius", currentNunchukRadius7);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Nunchuk/Stick/Radius", currentNunchukRange7);

		// Classic analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Classic/Left Stick/Radius", currentClassicLRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Classic/Right Stick/Radius", currentClassicRRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Classic/Triggers/Threshold", currentClassicThres4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Classic/Left Stick/Radius", currentClassicLRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Classic/Right Stick/Radius", currentClassicRRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Classic/Triggers/Threshold", currentClassicThres5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Classic/Left Stick/Radius", currentClassicLRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Classic/Right Stick/Radius", currentClassicRRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Classic/Triggers/Threshold", currentClassicThres6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Classic/Left Stick/Radius", currentClassicLRadius7);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Classic/Right Stick/Radius", currentClassicRRadius7);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Classic/Triggers/Threshold", currentClassicThres7);

		// Guitar analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Guitar/Stick/Radius", currentGuitarRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Guitar/Stick/Radius", currentGuitarRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Guitar/Stick/Radius", currentGuitarRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Guitar/Stick/Radius", currentGuitarRadius7);

		// Drums analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Drums/Stick/Radius", currentDrumsRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Drums/Stick/Radius", currentDrumsRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Drums/Stick/Radius", currentDrumsRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Drums/Stick/Radius", currentDrumsRadius7);

		// Turntable analog ranges Setup
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Turntable/Stick/Radius", currentTurntableRadius4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Turntable/Stick/Radius", currentTurntableRadius5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Turntable/Stick/Radius", currentTurntableRadius6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Turntable/Stick/Radius", currentTurntableRadius7);

		// Wiimote Extension Settings
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Extension", currentWiimoteExtension4);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote2", "Extension", currentWiimoteExtension5);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote3", "Extension", currentWiimoteExtension6);
		NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote4", "Extension", currentWiimoteExtension7);

		// General Video Settings
		NativeLibrary.SetConfig("Dolphin.ini", "Core", "GFXBackend", currentVideoBackend);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "ShowFPS", showingFPS ? "True" : "False");
		NativeLibrary.SetConfig("Dolphin.ini", "Android", "ScreenControls", drawingOnscreenControls ? "True" : "False");

		// Video Hack Settings
		NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBAccessEnable", skipEFBAccess ? "False" : "True");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "EFBEmulateFormatChanges", ignoreFormatChanges ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "AspectRatio", aspectRatio);

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
		NativeLibrary.SetConfig("gfx_opengl.ini", "Stereoscopy", "StereoMode", stereoscopyMode);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Stereoscopy", "StereoSwapEyes", stereoscopyEyeSwap ? "True" : "False");
		NativeLibrary.SetConfig("gfx_opengl.ini", "Stereoscopy", "StereoDepth", stereoscopySeparation);
		NativeLibrary.SetConfig("gfx_opengl.ini", "Stereoscopy", "StereoConvergence", stereoConvergencePercentage);
		NativeLibrary.SetConfig("Dolphin.ini", "Settings", "SIDevice0", "6");
		NativeLibrary.SetConfig("Dolphin.ini", "Settings", "SIDevice1", enableController2 ? "6" : "0");
		NativeLibrary.SetConfig("Dolphin.ini", "Settings", "SIDevice2", enableController3 ? "6" : "0");
		NativeLibrary.SetConfig("Dolphin.ini", "Settings", "SIDevice3", enableController4 ? "6" : "0");
	}
}
