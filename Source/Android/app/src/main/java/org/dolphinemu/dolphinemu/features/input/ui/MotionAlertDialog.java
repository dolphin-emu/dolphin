// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.app.Activity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;

import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface;
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon;
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting;

/**
 * {@link AlertDialog} derivative that listens for
 * motion events from controllers and joysticks.
 */
public final class MotionAlertDialog extends AlertDialog
{
  private final Activity mActivity;
  private final InputMappingControlSetting mSetting;
  private final boolean mAllDevices;
  private boolean mRunning = false;

  /**
   * Constructor
   *
   * @param activity   The current {@link Activity}.
   * @param setting    The setting to show this dialog for.
   * @param allDevices Whether to detect inputs from devices other than the configured one.
   */
  public MotionAlertDialog(Activity activity, InputMappingControlSetting setting,
          boolean allDevices)
  {
    super(activity);

    mActivity = activity;
    mSetting = setting;
    mAllDevices = allDevices;
  }

  @Override
  protected void onStart()
  {
    super.onStart();

    mRunning = true;
    new Thread(() ->
    {
      String result = MappingCommon.detectInput(mSetting.getController(), mAllDevices);
      mActivity.runOnUiThread(() ->
      {
        if (mRunning)
        {
          mSetting.setValue(result);
          dismiss();
        }
      });
    }).start();
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mRunning = false;
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event)
  {
    ControllerInterface.dispatchKeyEvent(event);

    if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && event.isLongPress())
    {
      // Special case: Let the user cancel by long-pressing Back (intended for non-touch devices)
      mSetting.clearValue();
      dismiss();
    }

    return true;
  }

  @Override
  public boolean dispatchGenericMotionEvent(@NonNull MotionEvent event)
  {
    if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0)
    {
      // Special case: Let the user cancel by touching an on-screen button
      return super.dispatchGenericMotionEvent(event);
    }

    ControllerInterface.dispatchGenericMotionEvent(event);
    return true;
  }
}
