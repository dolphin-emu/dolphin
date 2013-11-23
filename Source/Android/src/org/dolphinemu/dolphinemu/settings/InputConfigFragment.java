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
{
	private Activity m_activity;
	private boolean firstEvent = true;
	private static final ArrayList<Float> m_values = new ArrayList<Float>();

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
		dialog.setOnKeyListener(new AlertDialog.OnKeyListener()
		{
			public boolean onKey(DialogInterface dlg, int keyCode, KeyEvent event)
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
				if (event == null || (event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)
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
							NativeLibrary.SetConfig("Dolphin.ini", "Android", pref.getKey(), bindStr);
							pref.setSummary(bindStr);
							dialog.dismiss();
						}
						else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
						{
							String bindStr = "Device '" + InputConfigFragment.getInputDesc(input) + "'-Axis " + range.getAxis() + "+";
							NativeLibrary.SetConfig("Dolphin.ini", "Android", pref.getKey(), bindStr);
							pref.setSummary(bindStr);
							dialog.dismiss();
						}
					}
				}

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
		dialog.setMessage(String.format(getString(R.string.input_binding_descrip), pref.getTitle()));

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
	private static final class MotionAlertDialog extends AlertDialog
	{
		private OnMotionEventListener motionListener;

		/**
		 * Constructor
		 * 
		 * @param ctx context to use this dialog in.
		 */
		public MotionAlertDialog(Context ctx)
		{
			super(ctx);
		}

		/**
		 * Interface which defines a callback method for general
		 * motion events. This allows motion event code to be set
		 * in the event anonymous classes of this dialog are used.
		 */
		public interface OnMotionEventListener
		{
			/**
			 * Denotes the behavior that should happen when a motion event occurs.
			 * 
			 * @param event Reference to the {@link MotionEvent} that occurred.
			 * 
			 * @return true if the {@link MotionEvent} is consumed in this call; false otherwise.
			 */
			boolean onMotion(MotionEvent event);
		}

		/**
		 * Sets the motion listener.
		 * 
		 * @param listener The motion listener to set.
		 */
		public void setOnMotionEventListener(OnMotionEventListener listener)
		{
			this.motionListener = listener;
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
			if (motionListener.onMotion(event))
				return true;

			return super.dispatchGenericMotionEvent(event);
		}
	}
}
