package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.model.settings.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.utils.ControllerMappingHelper;
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
	private final ControllerMappingHelper mControllerMappingHelper;
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
		this.mControllerMappingHelper = new ControllerMappingHelper();
	}

	public boolean onKeyEvent(int keyCode, KeyEvent event)
	{
		Log.debug("[MotionAlertDialog] Received key event: " + event.getAction());
		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
				if (!mControllerMappingHelper.shouldKeyBeIgnored(event.getDevice(), keyCode))
				{
					saveKeyInput(event);
				}
				// Even if we ignore the key, we still consume it. Thus return true regardless.
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
		if (event.getAction() != MotionEvent.ACTION_MOVE)
			return false;

		InputDevice input = event.getDevice();

		List<InputDevice.MotionRange> motionRanges = input.getMotionRanges();

		int numMovedAxis = 0;
		float axisMoveValue = 0.0f;
		InputDevice.MotionRange lastMovedRange = null;
		char lastMovedDir = '?';
		if (mWaitingForEvent)
		{
			// Get only the axis that seem to have moved (more than .5)
			for (InputDevice.MotionRange range : motionRanges)
			{
				int axis = range.getAxis();
				float origValue = event.getAxisValue(axis);
				float value = mControllerMappingHelper.scaleAxis(input, axis, origValue);
				if (Math.abs(value) > 0.5f)
				{
					// It is common to have multiple axis with the same physical input. For example,
					// shoulder butters are provided as both AXIS_LTRIGGER and AXIS_BRAKE.
					// To handle this, we ignore an axis motion that's the exact same as a motion
					// we already saw. This way, we ignore axis with two names, but catch the case
					// where a joystick is moved in two directions.
					// ref: bottom of https://developer.android.com/training/game-controllers/controller-input.html
					if (value != axisMoveValue)
					{
						axisMoveValue = value;
						numMovedAxis++;
						lastMovedRange = range;
						lastMovedDir = value < 0.0f ? '-' : '+';
					}
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
