// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.view.Surface;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;

public class MotionListener implements SensorEventListener
{
  private final Activity mActivity;
  private final SensorManager mSensorManager;
  private final Sensor mAccelSensor;
  private final Sensor mGyroSensor;

  private boolean mEnabled = false;

  // The same sampling period as for Wii Remotes
  private static final int SAMPLING_PERIOD_US = 1000000 / 200;

  public MotionListener(Activity activity)
  {
    mActivity = activity;
    mSensorManager = (SensorManager) activity.getSystemService(Context.SENSOR_SERVICE);
    mAccelSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
    mGyroSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
  }

  @Override
  public void onSensorChanged(SensorEvent sensorEvent)
  {
    float x, y;
    float z = sensorEvent.values[2];
    int orientation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
    switch (orientation)
    {
      default:
      case Surface.ROTATION_0:
        x = -sensorEvent.values[0];
        y = -sensorEvent.values[1];
        break;
      case Surface.ROTATION_90:
        x = sensorEvent.values[1];
        y = -sensorEvent.values[0];
        break;
      case Surface.ROTATION_180:
        x = sensorEvent.values[0];
        y = sensorEvent.values[1];
        break;
      case Surface.ROTATION_270:
        x = -sensorEvent.values[1];
        y = sensorEvent.values[0];
        break;
    }

    if (sensorEvent.sensor == mAccelSensor)
    {
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_LEFT, x);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_RIGHT, x);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_FORWARD, y);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_BACKWARD, y);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_UP, z);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_ACCEL_DOWN, z);
    }

    if (sensorEvent.sensor == mGyroSensor)
    {
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_PITCH_UP, x);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_PITCH_DOWN, x);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_ROLL_LEFT, y);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_ROLL_RIGHT, y);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_YAW_LEFT, z);
      NativeLibrary.onGamePadMoveEvent(NativeLibrary.TouchScreenDevice,
              ButtonType.WIIMOTE_GYRO_YAW_RIGHT, z);
    }
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int i)
  {
    // We don't care about this
  }

  public void enable()
  {
    if (mEnabled)
      return;

    if (mAccelSensor != null)
      mSensorManager.registerListener(this, mAccelSensor, SAMPLING_PERIOD_US);
    if (mGyroSensor != null)
      mSensorManager.registerListener(this, mGyroSensor, SAMPLING_PERIOD_US);

    NativeLibrary.SetMotionSensorsEnabled(mAccelSensor != null, mGyroSensor != null);

    mEnabled = true;
  }

  public void disable()
  {
    if (!mEnabled)
      return;

    mSensorManager.unregisterListener(this);

    NativeLibrary.SetMotionSensorsEnabled(false, false);

    mEnabled = false;
  }
}
