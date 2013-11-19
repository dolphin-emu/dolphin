/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input;

import java.util.List;

import android.app.Fragment;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.view.InputDevice;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;

/**
 * The {@link Fragment} responsible for implementing the functionality
 * within the input control mapping config.
 */
public final class InputConfigFragment extends PreferenceFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Expand the preferences from the XML.
		addPreferencesFromResource(R.xml.input_prefs);

		// Set the summary messages of the preferences to whatever binding
		// is currently set within the Dolphin config.
		final String[] keys = 
		{
			"InputA", "InputB", "InputX", "InputY", "InputZ", "InputStart",
			"DPadUp", "DPadDown", "DPadLeft", "DPadRight",
			"MainUp", "MainDown", "MainLeft", "MainRight",
			"CStickUp", "CStickDown", "CStickLeft", "CStickRight",
			"InputL", "InputR"
		};

		// Loop through the keys for all 4 GameCube controllers.
		for (int i = 1; i <= 4; i++)
		{
			for (String key : keys)
			{
				final String binding = NativeLibrary.GetConfig("Dolphin.ini", "Android", key+"_"+i, "None");
				final Preference pref = findPreference(key+"_"+i);
				pref.setSummary(binding);
			}
		}
	}

	/**
	 * Gets the descriptor for the given {@link InputDevice}.
	 * 
	 * @param input The {@link InputDevice} to get the descriptor of.
	 * 
	 * @return the descriptor for the given {@link InputDevice}.
	 */
	public static String getInputDesc(InputDevice input)
	{
		if (input == null)
			return "null"; // Happens when the InputDevice is from an unknown source

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN)
		{
			return input.getDescriptor();
		}
		else
		{
			List<InputDevice.MotionRange> motions = input.getMotionRanges();
			String fakeid = "";

			for (InputDevice.MotionRange range : motions)
				fakeid += range.getAxis();

			return fakeid;
		}
	}

	@Override
	public boolean onPreferenceTreeClick(PreferenceScreen screen, Preference pref)
	{
		// If the user has clicked the option to configure the input overlay.
		if (pref.getTitle().equals(getString(R.string.input_overlay_layout)))
		{
			Intent inputOverlayConfig = new Intent(getActivity(), InputOverlayConfigActivity.class);
			startActivity(inputOverlayConfig);
			return true;
		}

		return false;
	}
}
