package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link AlertDialog} derivative that listens for
 * motion events from controllers and joysticks.
 */
public final class MotionAlertDialog extends AlertDialog
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
		Log.debug("[MotionAlertDialog] Received key event: " + event.getAction());
		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
			case KeyEvent.ACTION_UP:

				InputDevice input = event.getDevice();
				saveInput(input, event, null, false);

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

		Log.debug("[MotionAlertDialog] Received motion event: " + event.getAction());

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
					saveInput(input, null, range, false);
				}
				else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
				{
					saveInput(input, null, range, true);
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

	/**
	 * Saves the provided input setting both to the INI file (so native code can use it) and as an
	 * Android preference (so it persists correctly, and is human-readable.)
	 *
	 * @param device       Required; the InputDevice from which the input event originated.
	 * @param keyEvent     If the event was a button push, this KeyEvent represents it and is required.
	 * @param motionRange  If the event was an axis movement, this MotionRange represents it and is required.
	 * @param axisPositive If the event was an axis movement, this boolean indicates the direction and is required.
	 */
	private void saveInput(InputDevice device, KeyEvent keyEvent, InputDevice.MotionRange motionRange, boolean axisPositive)
	{
		String bindStr = null;
		String uiString = null;

		if (keyEvent != null)
		{
			bindStr = "Device '" + device.getDescriptor() + "'-Button " + keyEvent.getKeyCode();
			uiString = device.getName() + ": Button " + keyEvent.getKeyCode();
		}

		if (motionRange != null)
		{
			if (axisPositive)
			{
				bindStr = "Device '" + device.getDescriptor() + "'-Axis " + motionRange.getAxis() + "+";
				uiString = device.getName() + ": Axis " + motionRange.getAxis() + "+";
			}
			else
			{
				bindStr = "Device '" + device.getDescriptor() + "'-Axis " + motionRange.getAxis() + "-";
				uiString = device.getName() + ": Axis " + motionRange.getAxis() + "-";
			}
		}

		if (bindStr != null)
		{
			NativeLibrary.SetConfig("Dolphin.ini", "Android", inputPref.getKey(), bindStr);
		}
		else
		{
			Log.error("[MotionAlertDialog] Failed to save input to INI.");
		}


		if (uiString != null)
		{
			SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
			SharedPreferences.Editor editor = preferences.edit();

			editor.putString(inputPref.getKey(), uiString);
			editor.apply();

			inputPref.setSummary(uiString);
		}
		else
		{
			Log.error("[MotionAlertDialog] Failed to save input to preference.");
		}

		dismiss();
	}
}