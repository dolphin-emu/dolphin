package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.model.settings.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.List;

/**
 * {@link AlertDialog} derivative that listens for
 * motion events from controllers and joysticks.
 */
public final class MotionAlertDialog extends AlertDialog
{
	// The selected input preference
	private final InputBindingSetting setting;
	private boolean mWaitingForEvent = true;

	/**
	 * Constructor
	 *
	 * @param context The current {@link Context}.
	 * @param setting The Preference to show this dialog for.
	 */
	public MotionAlertDialog(Context context, InputBindingSetting setting)
	{
		super(context);

		this.setting = setting;
	}

	public boolean onKeyEvent(int keyCode, KeyEvent event)
	{
		Log.debug("[MotionAlertDialog] Received key event: " + event.getAction());
		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
				saveKeyInput(event);

				return true;

			default:
				return false;
		}
	}

	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		// Handle this key if we care about it, otherwise pass it down the framework
		return onKeyEvent(event.getKeyCode(), event) || super.dispatchKeyEvent(event);
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		// Handle this event if we care about it, otherwise pass it down the framework
		return onMotionEvent(event) || super.dispatchGenericMotionEvent(event);
	}

	private boolean onMotionEvent(MotionEvent event)
	{
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)
			return false;

		Log.debug("[MotionAlertDialog] Received motion event: " + event.getAction());

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motionRanges = input.getMotionRanges();

		int numMovedAxis = 0;
		InputDevice.MotionRange lastMovedRange = null;
		char lastMovedDir = '?';
		if (mWaitingForEvent)
		{
			// Get only the axis that seem to have moved (more than .5)
			for (InputDevice.MotionRange range : motionRanges)
			{
				int axis = range.getAxis();
				float value = event.getAxisValue(axis);
				if (Math.abs(value) > 0.5f)
				{
					numMovedAxis++;
					lastMovedRange = range;
					lastMovedDir = value < 0.0f ? '-' : '+';
				}
			}

			// If only one axis moved, that's the winner.
			if (numMovedAxis == 1)
			{
				mWaitingForEvent = false;
				saveMotionInput(input, lastMovedRange, lastMovedDir);
			}
		}

		return true;
	}

	/**
	 * Saves the provided key input setting both to the INI file (so native code can use it) and as
	 * an Android preference (so it persists correctly and is human-readable.)
	 *
	 * @param keyEvent     KeyEvent of this key press.
	 */
	private void saveKeyInput(KeyEvent keyEvent)
	{
		InputDevice device = keyEvent.getDevice();
		String bindStr = "Device '" + device.getDescriptor() + "'-Button " + keyEvent.getKeyCode();
		String uiString = device.getName() + ": Button " + keyEvent.getKeyCode();

		saveInput(bindStr, uiString);
	}

	/**
	 * Saves the provided motion input setting both to the INI file (so native code can use it) and as
	 * an Android preference (so it persists correctly and is human-readable.)
	 *
	 * @param device       InputDevice from which the input event originated.
	 * @param motionRange  MotionRange of the movement
	 * @param axisDir      Either '-' or '+'
	 */
	private void saveMotionInput(InputDevice device, InputDevice.MotionRange motionRange, char axisDir)
	{
		String bindStr = "Device '" + device.getDescriptor() + "'-Axis " + motionRange.getAxis() + axisDir;
		String uiString = device.getName() + ": Axis " + motionRange.getAxis() + axisDir;

		saveInput(bindStr, uiString);
	}

	/** Save the input string to settings and SharedPreferences, then dismiss this Dialog. */
	private void saveInput(String bind, String ui)
	{
		setting.setValue(bind);

		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
		SharedPreferences.Editor editor = preferences.edit();

		editor.putString(setting.getKey(), ui);
		editor.apply();

		dismiss();
	}
}
