package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.content.Context;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.dolphinemu.dolphinemu.features.settings.model.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.utils.ControllerMappingHelper;
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
  private final ArrayList<Float> mPreviousValues = new ArrayList<>();
  private int mPrevDeviceId = 0;
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
      case KeyEvent.ACTION_UP:
        if (!ControllerMappingHelper.shouldKeyBeIgnored(event.getDevice(), keyCode))
        {
          setting.onKeyInput(event);
          dismiss();
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

    if (input.getId() != mPrevDeviceId)
    {
      mPreviousValues.clear();
    }
    mPrevDeviceId = input.getId();
    boolean firstEvent = mPreviousValues.isEmpty();

    int numMovedAxis = 0;
    float axisMoveValue = 0.0f;
    InputDevice.MotionRange lastMovedRange = null;
    char lastMovedDir = '?';
    if (mWaitingForEvent)
    {
      for (int i = 0; i < motionRanges.size(); i++)
      {
        InputDevice.MotionRange range = motionRanges.get(i);
        int axis = range.getAxis();
        float origValue = event.getAxisValue(axis);
        float value = ControllerMappingHelper.scaleAxis(input, axis, origValue);
        if (firstEvent)
        {
          mPreviousValues.add(value);
        }
        else
        {
          float previousValue = mPreviousValues.get(i);

          // Only handle the axes that are not neutral (more than 0.5)
          // but ignore any axis that has a constant value (e.g. always 1)
          if (Math.abs(value) > 0.5f && value != previousValue)
          {
            // It is common to have multiple axes with the same physical input. For example,
            // shoulder butters are provided as both AXIS_LTRIGGER and AXIS_BRAKE.
            // To handle this, we ignore an axis motion that's the exact same as a motion
            // we already saw. This way, we ignore axes with two names, but catch the case
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
          // Special case for d-pads (axis value jumps between 0 and 1 without any values
          // in between). Without this, the user would need to press the d-pad twice
          // due to the first press being caught by the "if (firstEvent)" case further up.
          else if (Math.abs(value) < 0.25f && Math.abs(previousValue) > 0.75f)
          {
            numMovedAxis++;
            lastMovedRange = range;
            lastMovedDir = previousValue < 0.0f ? '-' : '+';
          }
        }

        mPreviousValues.set(i, value);
      }

      // If only one axis moved, that's the winner.
      if (numMovedAxis == 1)
      {
        mWaitingForEvent = false;
        setting.onMotionInput(input, lastMovedRange, lastMovedDir);
        dismiss();
      }
    }
    return true;
  }
}
