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
    // The cached shared preferences.
    private final SharedPreferences mPrefs;
    
    // Whether or not the user is using dual core.
    private final boolean isUsingDualCore;
    
    // The current CPU core being used.
    private final String currentEmuCore;
    
    // The current video back-end being used.
    private final String currentVideoBackend;
    
    /**
     * Constructor
     * 
     * @param ctx The context to use an instance of this class in.
     *            This allows the class to retrieve the SharedPreferences
     *            instance from the given context, which, in turn allows
     *            this class to function for its intended purpose.
     */
    public UserPreferences(Context ctx)
    {
        // Get an instance of all of our stored preferences.
        this.mPrefs = PreferenceManager.getDefaultSharedPreferences(ctx);
        
        //-- Assign to the variables to cache the settings. --//
        
        this.isUsingDualCore = mPrefs.getBoolean("dualCorePref", true);
        
        // Fall back to interpreter if it somehow can't find a CPU core.
        this.currentEmuCore = mPrefs.getString("cpuCorePref", "0");
        
        // Fall back to using software rendering if another valid backend can't be found.
        this.currentVideoBackend = mPrefs.getString("gpuPref", "Software Renderer");
    }
    
    /** Writes the config to the Dolphin ini file. */
    public void SaveConfigToDolphinIni()
    {
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUCore", currentEmuCore);
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "CPUThread", isUsingDualCore ? "True" : "False");
        NativeLibrary.SetConfig("Dolphin.ini", "Core", "GFXBackend", currentVideoBackend);
    }
}
