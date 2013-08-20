package org.dolphinemu.dolphinemu;

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
     * Writes the config to the Dolphin ini file. 
     * 
     * @param ctx The context used to retrieve the user settings.
     * */
    public static void SaveConfigToDolphinIni(Context ctx)
    {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
        
        // Whether or not the user is using dual core.
        boolean isUsingDualCore = prefs.getBoolean("dualCorePref", true);
        
        // Current CPU core being used. Falls back to interpreter upon error.
        String currentEmuCore   = prefs.getString("cpuCorePref", "0");
        
        // Current video backend being used. Falls back to software rendering upon error.
        String currentVideoBackend = prefs.getString("gpuPref", "Software Rendering");
        
        // Whether or not to ignore all EFB access requests from the CPU.
        boolean skipEFBAccess = prefs.getBoolean("skipEFBAccess", false);
        
        // Whether or not to ignore changes to the EFB format.
        boolean ignoreFormatChanges = prefs.getBoolean("ignoreFormatChanges", false);
        
        // EFB copy method to use.
        String efbCopyMethod = prefs.getString("efbCopyMethod", "Off");
        
        // Texture cache accuracy. Falls back to "Fast" up error.
        String textureCacheAccuracy = prefs.getString("textureCacheAccuracy", "128");
        
        // External frame buffer emulation. Falls back to disabled upon error.
        String externalFrameBuffer = prefs.getString("externalFrameBuffer", "Disabled");
        
        // Whether or not display list caching is enabled.
        boolean dlistCachingEnabled = prefs.getBoolean("cacheDisplayLists", false);
        
        // Whether or not to disable destination alpha.
        boolean disableDstAlphaPass = prefs.getBoolean("disableDestinationAlpha", false);
        
        // Whether or not to use fast depth calculation.
        boolean useFastDepthCalc = prefs.getBoolean("fastDepthCalculation", true);
        
        // Internal resolution. Falls back to 1x Native upon error.
        String internalResolution = prefs.getString("internalResolution", "2");
        
        // Anisotropic Filtering Level. Falls back to 1x upon error.
        String anisotropicFiltLevel = prefs.getString("anisotropicFiltering", "0");
        
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
        
        // General Video Settings
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "GFXBackend", currentVideoBackend);
        
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
        
        NativeLibrary.SetConfig("gfx_opengl.ini", "Hacks", "DlistCachingEnable", dlistCachingEnabled ? "True" : "False");
        NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "DstAlphaPass", disableDstAlphaPass ? "True" : "False");
        NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "FastDepthCalc", useFastDepthCalc ? "True" : "False");
        
        //-- Enhancement Settings --//
        NativeLibrary.SetConfig("gfx_opengl.ini", "Settings", "EFBScale", internalResolution);
        NativeLibrary.SetConfig("gfx_opengl.ini", "Enhancements", "MaxAnisotropy", anisotropicFiltLevel);
        NativeLibrary.SetConfig("gfx.opengl.ini", "Hacks", "EFBScaledCopy", usingScaledEFBCopy ? "True" : "False");
        NativeLibrary.SetConfig("gfx.opengl.ini", "Settings", "EnablePixelLighting", usingPerPixelLighting ? "True" : "False");
        NativeLibrary.SetConfig("gfx.opengl.ini", "Enhancements", "ForceFiltering", isForcingTextureFiltering ? "True" : "False");
        NativeLibrary.SetConfig("gfx.opengl.ini", "Settings", "DisableFog", fogIsDisabled ? "True" : "False");
    }
}
