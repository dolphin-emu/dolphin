package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.NativeLibrary.ButtonType;

public class MotionListener implements SensorEventListener
{
  private final SensorManager mSensorManager;
  private final Sensor mAccelSensor;
  private final Sensor mGyroSensor;

  // The same sampling period as for Wii Remotes
  private static final int SAMPLING_PERIOD_US = 1000000 / 200;

  public MotionListener(Context context)
  {
    mSensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
    mAccelSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
    mGyroSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
  }

  @Override
  public void onSensorChanged(SensorEvent sensorEvent)
  {
    if (sensorEvent.sensor == mAccelSensor)
    {
      float x = -sensorEvent.values[0];
      float y = -sensorEvent.values[1];
      float z = sensorEvent.values[2];
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
      float x = -sensorEvent.values[0];
      float y = -sensorEvent.values[1];
      float z = sensorEvent.values[2];
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
    if (mAccelSensor != null)
      mSensorManager.registerListener(this, mAccelSensor, SAMPLING_PERIOD_US);
    if (mGyroSensor != null)
      mSensorManager.registerListener(this, mGyroSensor, SAMPLING_PERIOD_US);
  }

  public void disable()
  {
    mSensorManager.unregisterListener(this);
  }
}
