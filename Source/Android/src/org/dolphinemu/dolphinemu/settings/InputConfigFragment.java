/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import android.app.Activity;
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
		//implements PrefsActivity.OnGameConfigListener
{
	private Activity m_activity;
	private boolean firstEvent = true;
	private static ArrayList<Float> m_values = new ArrayList<Float>();

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
	public boolean onPreferenceTreeClick(final PreferenceScreen screen, final Preference pref)
	{
		// Begin the creation of the input alert.
		final MotionAlertDialog dialog = new MotionAlertDialog(m_activity);

		// Set the key listener
		dialog.setOnKeyEventListener(new MotionAlertDialog.OnKeyEventListener()
		{
			public boolean onKey(KeyEvent event)
			{
				Log.d("InputConfigFragment", "Received key event: " + event.getAction());
				switch (event.getAction())
				{
					case KeyEvent.ACTION_DOWN:
					case KeyEvent.ACTION_UP:
						InputDevice input = event.getDevice();
						String bindStr = "Device '" + getInputDesc(input) + "'-Button " + event.getKeyCode();
						NativeLibrary.SetConfig("Dolphin.ini", "Android", pref.getKey(), bindStr);
						pref.setSummary(bindStr);
						dialog.dismiss();
						return true;

					default:
						break;
				}

				return false;
			}
		});

		// Set the motion event listener.
		dialog.setOnMotionEventListener(new MotionAlertDialog.OnMotionEventListener()
		{
			public boolean onMotion(MotionEvent event)
			{
				Log.d("InputConfigFragment", "Received motion event: " + event.getAction());
				if (event == null || (event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)
					return false;

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
							NativeLibrary.SetConfig("Dolphin.ini", "Android", pref.getKey(), bindStr);
							pref.setSummary(bindStr);
						}
						else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
						{
							String bindStr = "Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "+";
							NativeLibrary.SetConfig("Dolphin.ini", "Android", pref.getKey(), bindStr);
							pref.setSummary(bindStr);
						}
					}
				}

				dialog.dismiss();
				return true;
			}
		});

		// Set the cancel button.
		dialog.setButton(AlertDialog.BUTTON_NEGATIVE, getString(R.string.cancel), new AlertDialog.OnClickListener()
		{
			public void onClick(DialogInterface dialog, int which)
			{
				// Do nothing. This just makes the cancel button appear.
			}
		});

		// Set the title and description message.
		dialog.setTitle(R.string.input_binding);
		dialog.setMessage(getString(R.string.input_binding_descrip));

		// Don't allow the dialog to close when a user taps
		// outside of it. They must press cancel or provide an input.
		dialog.setCanceledOnTouchOutside(false);

		// Everything is set, show the dialog.
		dialog.show();
		return true;
	}

	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// Cache the activity instance.
		m_activity = activity;
	}


	/**
	 * {@link AlertDialog} class derivative that allows the motion listener
	 * to be set anonymously, so the creation of an explicit class for
	 * providing functionality is not necessary.
	 */
	protected static final class MotionAlertDialog extends AlertDialog
	{
		private OnKeyEventListener keyListener;
		private OnMotionEventListener motionListener;

		public MotionAlertDialog(Context ctx)
		{
			super(ctx);
		}

		public interface OnKeyEventListener
		{
			boolean onKey(KeyEvent event);
		}

		public interface OnMotionEventListener
		{
			boolean onMotion(MotionEvent event);
		}

		public void setOnKeyEventListener(OnKeyEventListener listener)
		{
			this.keyListener = listener;
		}

		public void setOnMotionEventListener(OnMotionEventListener listener)
		{
			this.motionListener = listener;
		}
		
		@Override
		public boolean dispatchKeyEvent(KeyEvent event)
		{
			if (keyListener.onKey(event))
				return true;

			return super.dispatchKeyEvent(event);
		}
		
		@Override
		public boolean dispatchGenericMotionEvent(MotionEvent event)
		{
			if (motionListener.onMotion(event))
				return true;
			
			return super.dispatchGenericMotionEvent(event);
		}
	}
}
