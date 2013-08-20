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
        
        // Current video backend being used. Falls back to software rendering upon error
        String currentVideoBackend = prefs.getString("gpuPref", "Software Rendering");
        
        
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUCore", currentEmuCore);
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUThread", isUsingDualCore ? "True" : "False");
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "GFXBackend", currentVideoBackend);
    }
}
