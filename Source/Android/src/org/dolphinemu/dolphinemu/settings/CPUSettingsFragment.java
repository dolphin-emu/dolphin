/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import org.dolphinemu.dolphinemu.R;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;

/**
 * Responsible for the loading of the CPU preferences.
 */
public final class CPUSettingsFragment extends PreferenceFragment
{
    private Activity m_activity;
    
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.cpu_prefs);
        
        final ListPreference cpuCores = (ListPreference) findPreference("cpuCorePref");

        //
        // Set valid emulation cores depending on the CPU architecture
        // that the Android device is running on.
        //
        if (Build.CPU_ABI.contains("x86"))
        {
            cpuCores.setEntries(R.array.emuCoreEntriesX86);
            cpuCores.setEntryValues(R.array.emuCoreValuesX86);
        }
        else if (Build.CPU_ABI.contains("arm"))
        {
            cpuCores.setEntries(R.array.emuCoreEntriesARM);
            cpuCores.setEntryValues(R.array.emuCoreValuesARM);
        }
        else
        {
           cpuCores.setEntries(R.array.emuCoreEntriesOther);
           cpuCores.setEntryValues(R.array.emuCoreValuesOther);
        }
    }
    
    @Override
    public void onAttach(Activity activity)
    {
        super.onAttach(activity);

        // This makes sure that the container activity has implemented
        // the callback interface. If not, it throws an exception
        try
        {
            m_activity = activity;
        }
        catch (ClassCastException e)
        {
            throw new ClassCastException(activity.toString());
        }
    }
    
    @Override
    public void onDestroy()
    {
        super.onDestroy();
        
        // When this fragment is destroyed, force the settings to be saved to the ini file.
        UserPreferences.SaveConfigToDolphinIni(m_activity);
    }
}
