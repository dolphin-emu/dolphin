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

import java.util.ArrayList;
import java.util.List;

/**
 * {@link AlertDialog} derivative that listens for
 * motion events from controllers and joysticks.
 */
public final class MotionAlertDialog extends AlertDialog
{
	// The selected input preference
	private final InputBindingSetting setting;

	private boolean firstEvent = true;
	private final ArrayList<Float> m_values = new ArrayList<>();

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

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
		Log.debug("[MotionAlertDialog] Received key event: " + event.getAction());
		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
			case KeyEvent.ACTION_UP:
				saveKeyInput(event);

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
					saveMotionInput(input, range, '-');
				}
				else if (m_values.get(a) < (event.getAxisValue(range.getAxis()) - 0.5f))
				{
					saveMotionInput(input, range, '+');
				}
			}
		}

		return true;
	}

	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		return onKeyDown(event.getKeyCode(), event) || super.dispatchKeyEvent(event);
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		return onMotionEvent(event) || super.dispatchGenericMotionEvent(event);
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
