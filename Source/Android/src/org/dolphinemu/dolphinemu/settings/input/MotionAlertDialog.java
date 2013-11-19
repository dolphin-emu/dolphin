package org.dolphinemu.dolphinemu.settings.input;

import java.util.ArrayList;
import java.util.List;

import org.dolphinemu.dolphinemu.NativeLibrary;

import android.app.AlertDialog;
import android.content.Context;
import android.preference.Preference;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * {@link AlertDialog} derivative that listens for
 * motion events from controllers and joysticks.
 */
final class MotionAlertDialog extends AlertDialog
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
				String bindStr = "Device '" + InputConfigFragment.getInputDesc(input) + "'-Button " + event.getKeyCode();
				NativeLibrary.SetConfig("Dolphin.ini", "Android", inputPref.getKey(), bindStr);
				inputPref.setSummary(bindStr);
				dismiss();
				return true;

			default:
				return false;
		}
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