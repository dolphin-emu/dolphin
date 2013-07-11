package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class PrefsActivity extends PreferenceActivity {
	private PrefsActivity m_activity;

    public class PrefsFragment extends PreferenceFragment {

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // Load the preferences from an XML resource
            addPreferencesFromResource(R.layout.prefs);

	        final ListPreference etp = new ListPreference(m_activity);
	        CharSequence[] _entries;
	        CharSequence[] _entryvalues;

	        if (Build.CPU_ABI.contains("x86"))
	        {
		        _entries = new CharSequence[] {
			        "Interpreter",
			        "JIT64 Recompiler",
			        "JITIL Recompiler",
		        };
		        _entryvalues = new CharSequence[] {"0", "1", "2"};
	        }
	        else if (Build.CPU_ABI.contains("arm"))
	        {
		        _entries = new CharSequence[] {
				        "Interpreter",
				        "JIT ARM Recompiler",
		        };
		        _entryvalues = new CharSequence[] {"0", "3"};
	        }
	        else
	        {
		        _entries = new CharSequence[] {
				        "Interpreter",
		        };
		        _entryvalues = new CharSequence[] {"0"};
	        }

	        etp.setEntries(_entries);
	        etp.setEntryValues(_entryvalues);
	        etp.setKey("cpupref");
	        etp.setTitle("CPU Core");
	        etp.setSummary("Emulation core to use");

	        PreferenceCategory mCategory = (PreferenceCategory) findPreference("cpuprefcat");
	        mCategory.addPreference(etp);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

	    m_activity = this;

        getFragmentManager().beginTransaction().replace(android.R.id.content,
                new PrefsFragment()).commit();
    }
    @Override
    public void onBackPressed() {
        Intent intent = new Intent();
        setResult(Activity.RESULT_OK, intent);
        this.finish();
        super.onBackPressed();
    }
}
