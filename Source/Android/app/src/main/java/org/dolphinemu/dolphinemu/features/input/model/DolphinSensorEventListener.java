package org.dolphinemu.dolphinemu.features.input.model;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.view.InputDevice;
import android.view.Surface;

import androidx.annotation.Keep;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class DolphinSensorEventListener implements SensorEventListener
{
  // Set of three axes. Creates a negative companion to each axis, and corrects for device rotation.
  private static final int AXIS_SET_TYPE_DEVICE_COORDINATES = 0;
  // Set of three axes. Creates a negative companion to each axis.
  private static final int AXIS_SET_TYPE_OTHER_COORDINATES = 1;

  private static class AxisSetDetails
  {
    public final int firstAxisOfSet;
    public final int axisSetType;

    public AxisSetDetails(int firstAxisOfSet, int axisSetType)
    {
      this.firstAxisOfSet = firstAxisOfSet;
      this.axisSetType = axisSetType;
    }
  }

  private static class SensorDetails
  {
    public final int sensorType;
    public final String[] axisNames;
    public final AxisSetDetails[] axisSetDetails;
    public boolean isSuspended = true;

    public SensorDetails(int sensorType, String[] axisNames, AxisSetDetails[] axisSetDetails)
    {
      this.sensorType = sensorType;
      this.axisNames = axisNames;
      this.axisSetDetails = axisSetDetails;
    }
  }

  private static int sDeviceRotation = Surface.ROTATION_0;

  private final SensorManager mSensorManager;

  private final HashMap<Sensor, SensorDetails> mSensorDetails = new HashMap<>();

  private final boolean mRotateCoordinatesForScreenOrientation;

  private String mDeviceQualifier = "";

  // The fastest sampling rate Android lets us use without declaring the HIGH_SAMPLING_RATE_SENSORS
  // permission is 200 Hz. This is also the sampling rate of a Wii Remote, so it fits us perfectly.
  private static final int SAMPLING_PERIOD_US = 1000000 / 200;

  @Keep
  public DolphinSensorEventListener()
  {
    mSensorManager = (SensorManager)
            DolphinApplication.getAppContext().getSystemService(Context.SENSOR_SERVICE);
    mRotateCoordinatesForScreenOrientation = true;

    addSensors();
  }

  @Keep
  public DolphinSensorEventListener(InputDevice inputDevice)
  {
    mRotateCoordinatesForScreenOrientation = false;

    if (Build.VERSION.SDK_INT >= 31)
    {
      mSensorManager = inputDevice.getSensorManager();

      // TODO: There is a bug where after suspending sensors, onSensorChanged can get called for
      // a sensor that we never registered as a listener for. The way our code is currently written,
      // this causes a NullPointerException, but if we checked for null we would instead have the
      // problem of being spammed with onSensorChanged calls even though the sensor shouldn't be
      // enabled. For now, let's comment out the ability to use InputDevice sensors.

      //addSensors();
    }
    else
    {
      mSensorManager = null;
    }
  }

  private void addSensors()
  {
    tryAddSensor(Sensor.TYPE_ACCELEROMETER, new String[]{"Accel Right", "Accel Left",
                    "Accel Forward", "Accel Backward", "Accel Up", "Accel Down"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_GYROSCOPE, new String[]{"Gyro Pitch Up", "Gyro Pitch Down",
                    "Gyro Roll Right", "Gyro Roll Left", "Gyro Yaw Left", "Gyro Yaw Right"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_LIGHT, "Light");

    tryAddSensor(Sensor.TYPE_PRESSURE, "Pressure");

    tryAddSensor(Sensor.TYPE_TEMPERATURE, "Device Temperature");

    tryAddSensor(Sensor.TYPE_PROXIMITY, "Proximity");

    tryAddSensor(Sensor.TYPE_GRAVITY, new String[]{"Gravity Right", "Gravity Left",
                    "Gravity Forward", "Gravity Backward", "Gravity Up", "Gravity Down"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_LINEAR_ACCELERATION,
            new String[]{"Linear Acceleration Right", "Linear Acceleration Left",
                    "Linear Acceleration Forward", "Linear Acceleration Backward",
                    "Linear Acceleration Up", "Linear Acceleration Down"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    // The values provided by this sensor can be interpreted as an Euler vector or a quaternion.
    // The directions of X and Y are flipped to match the Wii Remote coordinate system.
    tryAddSensor(Sensor.TYPE_ROTATION_VECTOR,
            new String[]{"Rotation Vector X-", "Rotation Vector X+", "Rotation Vector Y-",
                    "Rotation Vector Y+", "Rotation Vector Z+",
                    "Rotation Vector Z-", "Rotation Vector R", "Rotation Vector Heading Accuracy"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_RELATIVE_HUMIDITY, "Relative Humidity");

    tryAddSensor(Sensor.TYPE_AMBIENT_TEMPERATURE, "Ambient Temperature");

    // The values provided by this sensor can be interpreted as an Euler vector or a quaternion.
    // The directions of X and Y are flipped to match the Wii Remote coordinate system.
    tryAddSensor(Sensor.TYPE_GAME_ROTATION_VECTOR,
            new String[]{"Game Rotation Vector X-", "Game Rotation Vector X+",
                    "Game Rotation Vector Y-", "Game Rotation Vector Y+", "Game Rotation Vector Z+",
                    "Game Rotation Vector Z-", "Game Rotation Vector R"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED,
            new String[]{"Gyro Uncalibrated Pitch Up", "Gyro Uncalibrated Pitch Down",
                    "Gyro Uncalibrated Roll Right", "Gyro Uncalibrated Roll Left",
                    "Gyro Uncalibrated Yaw Left", "Gyro Uncalibrated Yaw Right",
                    "Gyro Drift Pitch Up", "Gyro Drift Pitch Down", "Gyro Drift Roll Right",
                    "Gyro Drift Roll Left", "Gyro Drift Yaw Left", "Gyro Drift Yaw Right"},
            new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES),
                    new AxisSetDetails(3, AXIS_SET_TYPE_DEVICE_COORDINATES)});

    tryAddSensor(Sensor.TYPE_HEART_RATE, "Heart Rate");

    if (Build.VERSION.SDK_INT >= 24)
    {
      tryAddSensor(Sensor.TYPE_HEART_BEAT, "Heart Beat");
    }

    if (Build.VERSION.SDK_INT >= 26)
    {
      tryAddSensor(Sensor.TYPE_ACCELEROMETER_UNCALIBRATED,
              new String[]{"Accel Uncalibrated Right", "Accel Uncalibrated Left",
                      "Accel Uncalibrated Forward", "Accel Uncalibrated Backward",
                      "Accel Uncalibrated Up", "Accel Uncalibrated Down",
                      "Accel Bias Right", "Accel Bias Left", "Accel Bias Forward",
                      "Accel Bias Backward", "Accel Bias Up", "Accel Bias Down"},
              new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_DEVICE_COORDINATES),
                      new AxisSetDetails(3, AXIS_SET_TYPE_DEVICE_COORDINATES)});
    }

    if (Build.VERSION.SDK_INT >= 30)
    {
      tryAddSensor(Sensor.TYPE_HINGE_ANGLE, "Hinge Angle");
    }

    if (Build.VERSION.SDK_INT >= 33)
    {
      // The values provided by this sensor can be interpreted as an Euler vector.
      // The directions of X and Y are flipped to match the Wii Remote coordinate system.
      tryAddSensor(Sensor.TYPE_HEAD_TRACKER,
              new String[]{"Head Rotation Vector X-", "Head Rotation Vector X+",
                      "Head Rotation Vector Y-", "Head Rotation Vector Y+",
                      "Head Rotation Vector Z+", "Head Rotation Vector Z-",
                      "Head Pitch Up", "Head Pitch Down", "Head Roll Right", "Head Roll Left",
                      "Head Yaw Left", "Head Yaw Right"},
              new AxisSetDetails[]{new AxisSetDetails(0, AXIS_SET_TYPE_OTHER_COORDINATES),
                      new AxisSetDetails(3, AXIS_SET_TYPE_OTHER_COORDINATES)});

      tryAddSensor(Sensor.TYPE_HEADING, new String[]{"Heading", "Heading Accuracy"},
              new AxisSetDetails[]{});
    }
  }

  private void tryAddSensor(int sensorType, String axisName)
  {
    tryAddSensor(sensorType, new String[]{axisName}, new AxisSetDetails[]{});
  }

  private void tryAddSensor(int sensorType, String[] axisNames, AxisSetDetails[] axisSetDetails)
  {
    Sensor sensor = mSensorManager.getDefaultSensor(sensorType);
    if (sensor != null)
    {
      mSensorDetails.put(sensor, new SensorDetails(sensorType, axisNames, axisSetDetails));
    }
  }

  @Override
  public void onSensorChanged(SensorEvent sensorEvent)
  {
    final SensorDetails sensorDetails = mSensorDetails.get(sensorEvent.sensor);

    final float[] values = sensorEvent.values;
    final String[] axisNames = sensorDetails.axisNames;
    final AxisSetDetails[] axisSetDetails = sensorDetails.axisSetDetails;

    int eventAxisIndex = 0;
    int detailsAxisIndex = 0;
    int detailsAxisSetIndex = 0;
    boolean keepSensorAlive = false;
    while (eventAxisIndex < values.length && detailsAxisIndex < axisNames.length)
    {
      if (detailsAxisSetIndex < axisSetDetails.length &&
              axisSetDetails[detailsAxisSetIndex].firstAxisOfSet == eventAxisIndex)
      {
        int rotation = Surface.ROTATION_0;
        if (mRotateCoordinatesForScreenOrientation &&
                axisSetDetails[detailsAxisSetIndex].axisSetType == AXIS_SET_TYPE_DEVICE_COORDINATES)
        {
          rotation = sDeviceRotation;
        }

        float x, y;
        switch (rotation)
        {
          default:
          case Surface.ROTATION_0:
            x = values[eventAxisIndex];
            y = values[eventAxisIndex + 1];
            break;
          case Surface.ROTATION_90:
            x = -values[eventAxisIndex + 1];
            y = values[eventAxisIndex];
            break;
          case Surface.ROTATION_180:
            x = -values[eventAxisIndex];
            y = -values[eventAxisIndex + 1];
            break;
          case Surface.ROTATION_270:
            x = values[eventAxisIndex + 1];
            y = -values[eventAxisIndex];
            break;
        }

        float z = values[eventAxisIndex + 2];

        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex], x);
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex + 1], x);
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex + 2], y);
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex + 3], y);
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex + 4], z);
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex + 5], z);

        eventAxisIndex += 3;
        detailsAxisIndex += 6;
        detailsAxisSetIndex++;
      }
      else
      {
        keepSensorAlive |= ControllerInterface.dispatchSensorEvent(mDeviceQualifier,
                axisNames[detailsAxisIndex], values[eventAxisIndex]);

        eventAxisIndex++;
        detailsAxisIndex++;
      }
    }

    if (!keepSensorAlive)
    {
      setSensorSuspended(sensorEvent.sensor, sensorDetails, true);
    }
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int i)
  {
    // We don't care about this
  }

  /**
   * The device qualifier set here will be passed on to native code,
   * for the purpose of letting native code identify which device this object belongs to.
   */
  @Keep
  public void setDeviceQualifier(String deviceQualifier)
  {
    mDeviceQualifier = deviceQualifier;
  }

  /**
   * If a sensor has been suspended to save battery, this unsuspends it.
   * If the sensor isn't currently suspended, nothing happens.
   *
   * @param axisName The name of any of the sensor's axes.
   */
  @Keep
  public void requestUnsuspendSensor(String axisName)
  {
    for (Map.Entry<Sensor, SensorDetails> entry : mSensorDetails.entrySet())
    {
      if (Arrays.asList(entry.getValue().axisNames).contains(axisName))
      {
        setSensorSuspended(entry.getKey(), entry.getValue(), false);
      }
    }
  }

  private void setSensorSuspended(Sensor sensor, SensorDetails sensorDetails, boolean suspend)
  {
    boolean changeOccurred = false;

    synchronized (sensorDetails)
    {
      if (sensorDetails.isSuspended != suspend)
      {
        ControllerInterface.notifySensorSuspendedState(mDeviceQualifier, sensorDetails.axisNames,
                suspend);

        if (suspend)
          mSensorManager.unregisterListener(this, sensor);
        else
          mSensorManager.registerListener(this, sensor, SAMPLING_PERIOD_US);

        sensorDetails.isSuspended = suspend;

        changeOccurred = true;
      }
    }

    if (changeOccurred)
    {
      Log.info((suspend ? "Suspended sensor " : "Unsuspended sensor ") + sensor.getName());
    }
  }

  @Keep
  public String[] getAxisNames()
  {
    ArrayList<String> axisNames = new ArrayList<>();

    for (SensorDetails sensorDetails : getSensorDetailsSorted())
    {
      Collections.addAll(axisNames, sensorDetails.axisNames);
    }

    return axisNames.toArray(new String[]{});
  }

  @Keep
  public boolean[] getNegativeAxes()
  {
    ArrayList<Boolean> negativeAxes = new ArrayList<>();

    for (SensorDetails sensorDetails : getSensorDetailsSorted())
    {
      int eventAxisIndex = 0;
      int detailsAxisIndex = 0;
      int detailsAxisSetIndex = 0;
      while (detailsAxisIndex < sensorDetails.axisNames.length)
      {
        if (detailsAxisSetIndex < sensorDetails.axisSetDetails.length &&
                sensorDetails.axisSetDetails[detailsAxisSetIndex].firstAxisOfSet == eventAxisIndex)
        {
          negativeAxes.add(false);
          negativeAxes.add(true);
          negativeAxes.add(false);
          negativeAxes.add(true);
          negativeAxes.add(false);
          negativeAxes.add(true);

          eventAxisIndex += 3;
          detailsAxisIndex += 6;
          detailsAxisSetIndex++;
        }
        else
        {
          negativeAxes.add(false);

          eventAxisIndex++;
          detailsAxisIndex++;
        }
      }
    }

    boolean[] result = new boolean[negativeAxes.size()];
    for (int i = 0; i < result.length; i++)
    {
      result[i] = negativeAxes.get(i);
    }

    return result;
  }

  private List<SensorDetails> getSensorDetailsSorted()
  {
    ArrayList<SensorDetails> sensorDetails = new ArrayList<>(mSensorDetails.values());
    Collections.sort(sensorDetails, Comparator.comparingInt(s -> s.sensorType));
    return sensorDetails;
  }

  /**
   * Should be called when an activity or other component that uses sensor events is resumed.
   *
   * Sensor events that contain device coordinates will have the coordinates rotated by the value
   * passed to this function.
   *
   * @param deviceRotation The current rotation of the device (i.e. rotation of the default display)
   */
  public static void setDeviceRotation(int deviceRotation)
  {
    sDeviceRotation = deviceRotation;
  }
}
