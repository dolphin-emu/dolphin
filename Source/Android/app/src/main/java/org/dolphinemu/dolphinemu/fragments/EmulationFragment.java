package org.dolphinemu.dolphinemu.fragments;

import android.content.IntentFilter;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.content.LocalBroadcastManager;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;
import org.dolphinemu.dolphinemu.utils.DirectoryStateReceiver;
import org.dolphinemu.dolphinemu.utils.Log;


public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback, SensorEventListener
{
  private static final String KEY_GAMEPATHS = "gamepaths";

  private InputOverlay mInputOverlay;
  private EmulationState mEmulationState;
  private DirectoryStateReceiver directoryStateReceiver;
  private EmulationActivity mActivity;

  public static EmulationFragment newInstance(String[] gamePaths)
  {
    Bundle args = new Bundle();
    args.putStringArray(KEY_GAMEPATHS, gamePaths);

    EmulationFragment fragment = new EmulationFragment();
    fragment.setArguments(args);
    return fragment;
  }

  /**
   * Initialize anything that doesn't depend on the layout / views in here.
   */
  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    mActivity = (EmulationActivity)getActivity();

    // So this fragment doesn't restart on configuration changes; i.e. rotation.
    setRetainInstance(true);

    String[] gamePaths = getArguments().getStringArray(KEY_GAMEPATHS);
    mEmulationState = new EmulationState(gamePaths);
  }

  /**
   * Initialize the UI and start emulation in here.
   */
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View contents = inflater.inflate(R.layout.fragment_emulation, container, false);

    SurfaceView surfaceView = contents.findViewById(R.id.surface_emulation);
    surfaceView.getHolder().addCallback(this);

    mInputOverlay = contents.findViewById(R.id.surface_input_overlay);

    Button doneButton = contents.findViewById(R.id.done_control_config);
    if (doneButton != null)
    {
      doneButton.setOnClickListener(v -> stopConfiguringControls());
    }

    // The new Surface created here will get passed to the native code via onSurfaceChanged.

    return contents;
  }

  @Override
  public void onResume()
  {
    super.onResume();
    if (DirectoryInitialization.isReady())
    {
      mEmulationState.run(mActivity.getSavedState());
    }
    else
    {
      setupDolphinDirectoriesThenStartEmulation();
    }
  }

  @Override
  public void onPause()
  {
    if (directoryStateReceiver != null)
    {
      LocalBroadcastManager.getInstance(mActivity).unregisterReceiver(directoryStateReceiver);
      directoryStateReceiver = null;
    }

    mEmulationState.pause();
    super.onPause();
  }

  private void setupDolphinDirectoriesThenStartEmulation()
  {
    IntentFilter statusIntentFilter = new IntentFilter(
      DirectoryInitialization.BROADCAST_ACTION);

    directoryStateReceiver =
      new DirectoryStateReceiver(directoryInitializationState ->
      {
        if (directoryInitializationState == DirectoryInitializationState.DIRECTORIES_INITIALIZED)
        {
          mEmulationState.run(mActivity.getSavedState());
        }
        else if (directoryInitializationState ==
          DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED)
        {
          Toast.makeText(getContext(), R.string.write_permission_needed, Toast.LENGTH_SHORT)
            .show();
        }
        else if (directoryInitializationState ==
          DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE)
        {
          Toast.makeText(getContext(), R.string.external_storage_not_mounted,
            Toast.LENGTH_SHORT)
            .show();
        }
      });

    // Registers the DirectoryStateReceiver and its intent filters
    LocalBroadcastManager.getInstance(mActivity).registerReceiver(
      directoryStateReceiver,
      statusIntentFilter);
    DirectoryInitialization.start(mActivity);
  }

  public void refreshInputOverlay()
  {
    mInputOverlay.refreshControls();
  }

  public void resetCurrentLayout()
  {
    mInputOverlay.resetCurrentLayout();
  }

  @Override
  public void surfaceCreated(SurfaceHolder holder)
  {
    // We purposely don't do anything here.
    // All work is done in surfaceChanged, which we are guaranteed to get even for surface creation.
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
  {
    mEmulationState.newSurface(holder.getSurface());
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder)
  {
    mEmulationState.clearSurface();
  }

  @Override
  public void onSensorChanged(SensorEvent event)
  {
    int sensorType = event.sensor.getType();
    if(sensorType == Sensor.TYPE_GAME_ROTATION_VECTOR)
    {
      float[] rotationMtx = new float[9];
      float[] rotationVal = new float[3];
      SensorManager.getRotationMatrixFromVector(rotationMtx, event.values);
      SensorManager.remapCoordinateSystem(rotationMtx, SensorManager.AXIS_X, SensorManager.AXIS_Y, rotationMtx);
      SensorManager.getOrientation(rotationMtx, rotationVal);
      mInputOverlay.onSensorChanged(rotationVal);
    }
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy)
  {
    mInputOverlay.onAccuracyChanged(accuracy);
  }

  public void setTouchPointer(int type)
  {
    mInputOverlay.setTouchPointer(type);
  }

  public void updateTouchPointer()
  {
    mInputOverlay.updateTouchPointer();
  }

  public void stopEmulation()
  {
    mEmulationState.stop();
  }

  public void startConfiguringControls()
  {
    getView().findViewById(R.id.done_control_config).setVisibility(View.VISIBLE);
    mInputOverlay.setIsInEditMode(true);
  }

  public void stopConfiguringControls()
  {
    getView().findViewById(R.id.done_control_config).setVisibility(View.GONE);
    mInputOverlay.setIsInEditMode(false);
  }

  public boolean isConfiguringControls()
  {
    return mInputOverlay.isInEditMode();
  }

  private static class EmulationState
  {
    private enum State
    {
      STOPPED, RUNNING, PAUSED
    }

    private final String[] mGamePaths;
    private State state;
    private Surface mSurface;
    private boolean mRunWhenSurfaceIsValid;

    private String mStatePath;

    EmulationState(String[] gamePaths)
    {
      mGamePaths = gamePaths;
      // Starting state is stopped.
      state = State.STOPPED;
    }

    public synchronized void stop()
    {
      if (state != State.STOPPED)
      {
        state = State.STOPPED;
        NativeLibrary.StopEmulation();
      }
      else
      {
        Log.warning("[EmulationFragment] Stop called while already stopped.");
      }
    }

    public synchronized void pause()
    {
      if (state == State.RUNNING)
      {
        state = State.PAUSED;
        // Release the surface before pausing, since emulation has to be running for that.
        NativeLibrary.SurfaceDestroyed();
        NativeLibrary.PauseEmulation();
      }
    }

    public synchronized void run(String statePath)
    {
      mStatePath = statePath;

      if (NativeLibrary.IsRunning())
      {
        state = State.PAUSED;
      }

      // If the surface is set, run now. Otherwise, wait for it to get set.
      if (mSurface != null)
      {
        runWithValidSurface();
      }
      else
      {
        mRunWhenSurfaceIsValid = true;
      }
    }

    // Surface callbacks
    private synchronized void newSurface(Surface surface)
    {
      mSurface = surface;
      if (mRunWhenSurfaceIsValid)
      {
        runWithValidSurface();
      }
    }

    private synchronized void clearSurface()
    {
      if (mSurface == null)
      {
        Log.warning("[EmulationFragment] clearSurface called, but surface already null.");
      }
      else
      {
        mSurface = null;
        if (state == State.RUNNING)
        {
          NativeLibrary.SurfaceDestroyed();
          state = State.PAUSED;
        }
        else if (state == State.PAUSED)
        {
          Log.warning("[EmulationFragment] Surface cleared while emulation paused.");
        }
        else
        {
          Log.warning("[EmulationFragment] Surface cleared while emulation stopped.");
        }
      }
    }

    private void runWithValidSurface()
    {
      mRunWhenSurfaceIsValid = false;
      if (state == State.STOPPED)
      {
        new Thread(() ->
        {
          NativeLibrary.SurfaceChanged(mSurface);
          NativeLibrary.Run(mGamePaths, mStatePath);
        }, "NativeEmulation").start();
      }
      else if (state == State.PAUSED)
      {
        NativeLibrary.SurfaceChanged(mSurface);
        NativeLibrary.UnPauseEmulation();
      }
      else
      {
        Log.warning("[EmulationFragment] Bug, run called while already running.");
      }
      state = State.RUNNING;
    }
  }
}
