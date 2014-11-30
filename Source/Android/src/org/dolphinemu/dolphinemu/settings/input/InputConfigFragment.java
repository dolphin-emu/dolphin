/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input;

import android.app.Fragment;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.view.InputDevice;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;

import java.util.List;

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
		final String[] gamecubeKeys = 
		{
			"InputA", "InputB", "InputX", "InputY", "InputZ", "InputStart",
			"DPadUp", "DPadDown", "DPadLeft", "DPadRight",
			"MainUp", "MainDown", "MainLeft", "MainRight",
			"CStickUp", "CStickDown", "CStickLeft", "CStickRight",
			"InputL", "InputR"
		};

		final String[] wiimoteKeys = 
		{
			"WiimoteInputA", "WiimoteInputB", "WiimoteInputOne", "WiimoteInputTwo", "WiimoteInputPlus", "WiimoteInputMinus", "WiimoteInputHome",
			"WiimoteIRUp", "WiimoteIRDown", "WiimoteIRLeft", "WiimoteIRRight", "WiimoteIRForward", "WiimoteIRBackward",
			"WiimoteSwingUp", "WiimoteSwingDown", "WiimoteSwingLeft", "WiimoteSwingRight",
			"WiimoteTiltForward", "WiimoteTiltBackward", "WiimoteTiltLeft", "WiimoteTiltRight",
			"WiimoteShakeX", "WiimoteShakeY", "WiimoteShakeZ",
			"WiimoteDPadUp", "WiimoteDPadDown", "WiimoteDPadLeft", "WiimoteDPadRight"
		};

		for (int i = 0; i < 4; i++)
		{
			// Loop through the keys for all 4 GameCube controllers.
			for (String key : gamecubeKeys)
			{
				final String binding = NativeLibrary.GetConfig("Dolphin.ini", "Android", key+"_"+i, "None");
				final Preference pref = findPreference(key+"_"+i);
				pref.setSummary(binding);
			}

			// Loop through the keys for the Wiimote
			/*for (String key : wiimoteKeys)
			{
				final String binding = NativeLibrary.GetConfig("Dolphin.ini", "Android", key+"_"+i, "None");
				final Preference pref = findPreference(key+"_"+i);
				pref.setSummary(binding);
			}*/
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
			StringBuilder fakeid = new StringBuilder();

			for (InputDevice.MotionRange range : motions)
				fakeid.append(range.getAxis());

			return fakeid.toString();
		}
	}
}
