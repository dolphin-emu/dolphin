/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import android.app.AlertDialog;
import android.app.Fragment;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.view.*;

import java.util.ArrayList;
import java.util.List;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;

/**
 * The {@link Fragment} responsible for implementing the functionality
 * within the input control mapping config.
 */
public final class InputConfigFragment extends PreferenceFragment
{
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
			"InputL", "InputR",
		};

		Preference pref;
		for (String key : keys)
		{
			String binding = NativeLibrary.GetConfig("Dolphin.ini", "Android", key, "None");
			pref = findPreference(key);
			pref.setSummary(binding);
		}
	}

	@Override
	public boolean onPreferenceTreeClick(PreferenceScreen screen, Preference pref)
	{
		// If the user is on the preference screen to set Gamecube input bindings.
		if (screen.getTitle().equals(getString(R.string.gamecube_bindings)))
		{
			// Begin the creation of the input alert.
			final MotionAlertDialog dialog = new MotionAlertDialog(getActivity(), pref);

			// Set the cancel button.
			dialog.setButton(AlertDialog.BUTTON_NEGATIVE, getString(R.string.cancel), new AlertDialog.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					// Do nothing. Just makes the cancel button show up.
				}
			});

			// Set the title and description message.
			dialog.setTitle(R.string.input_binding);
			dialog.setMessage(String.format(getString(R.string.input_binding_descrip), pref.getTitle()));

			// Don't allow the dialog to close when a user taps
			// outside of it. They must press cancel or provide an input.
			dialog.setCanceledOnTouchOutside(false);

			// Everything is set, show the dialog.
			dialog.show();
		}

		return true;
	}

	/**
	 * {@link AlertDialog} derivative that listens for
	 * motion events from controllers and joysticks.
	 */
	private static final class MotionAlertDialog extends AlertDialog
	{
		// The selected input preference
		private final Preference inputPref;

		private boolean firstEvent = true;
		private final ArrayList<Float> m_values = new ArrayList<Float>();

		/**
		 * Constructor
		 * 
		 * @param ctx       The current {@link Context}.
		 * @param inputPref The Preference to show this dialog for.
		 */
		public MotionAlertDialog(Context ctx, Preference inputPref)
		{
			super(ctx);

			this.inputPref = inputPref;
		}

		@Override
		public boolean onKeyDown(int keyCode, KeyEvent event)
		{
			Log.d("InputConfigFragment", "Received key event: " + event.getAction());
			switch (event.getAction())
			{
				case KeyEvent.ACTION_DOWN:
				case KeyEvent.ACTION_UP:
					InputDevice input = event.getDevice();
					String bindStr = "Device '" + getInputDesc(input) + "'-Button " + event.getKeyCode();
					NativeLibrary.SetConfig("Dolphin.ini", "Android", inputPref.getKey(), bindStr);
					inputPref.setSummary(bindStr);
					dismiss();
					return true;

				default:
					break;
			}

			return false;
		}
		

		// Method that will be called within dispatchGenericMotionEvent
		// that handles joystick/controller movements.
		private boolean onMotionEvent(MotionEvent event)
		{
			if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)
				return false;

			Log.d("InputConfigFragment", "Received motion event: " + event.getAction());

			InputDevice input = event.getDevice();
			List<InputDevice.MotionRange> motions = input.getMotionRanges();
			if (firstEvent)
			{
				m_values.clear();

				for (InputDevice.MotionRange range : motions)
				{
					m_values.add(event.getAxisValue(range.getAxis()));
				}

				firstEvent = false;
			}
			else
			{
				for (int a = 0; a < motions.size(); ++a)
				{
					InputDevice.MotionRange range = motions.get(a);

					if (m_values.get(a) > (event.getAxisValue(range.getAxis()) + 0.5f))
					{
						String bindStr = "Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "-";
						NativeLibrary.SetConfig("Dolphin.ini", "Android", inputPref.getKey(), bindStr);
						inputPref.setSummary(bindStr);
						dismiss();
					}
					else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
					{
						String bindStr = "Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "+";
						NativeLibrary.SetConfig("Dolphin.ini", "Android", inputPref.getKey(), bindStr);
						inputPref.setSummary(bindStr);
						dismiss();
					}
				}
			}

			return true;
		}

		@Override
		public boolean dispatchKeyEvent(KeyEvent event)
		{
			if (onKeyDown(event.getKeyCode(), event))
				return true;

			return super.dispatchKeyEvent(event);
		}
		
		@Override
		public boolean dispatchGenericMotionEvent(MotionEvent event)
		{
			if (onMotionEvent(event))
				return true;

			return super.dispatchGenericMotionEvent(event);
		}
	}
}
