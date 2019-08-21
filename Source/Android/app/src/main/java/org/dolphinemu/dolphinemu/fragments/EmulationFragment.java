package org.dolphinemu.dolphinemu.fragments;

import android.content.Context;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
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
import org.dolphinemu.dolphinemu.utils.StartupHandler;

import java.io.File;

public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback
{
  private static final String KEY_GAMEPATHS = "gamepaths";

  private SharedPreferences mPreferences;

  private InputOverlay mInputOverlay;

  private EmulationState mEmulationState;

  private DirectoryStateReceiver directoryStateReceiver;

  private EmulationActivity activity;

  public static EmulationFragment newInstance(String[] gamePaths)
  {
    Bundle args = new Bundle();
    args.putStringArray(KEY_GAMEPATHS, gamePaths);

    EmulationFragment fragment = new EmulationFragment();
    fragment.setArguments(args);
    return fragment;
  }

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);

    if (context instanceof EmulationActivity)
    {
      activity = (EmulationActivity) context;
      NativeLibrary.setEmulationActivity((EmulationActivity) context);
    }
    else
    {
      throw new IllegalStateException("EmulationFragment must have EmulationActivity parent");
    }
  }

  /**
   * Initialize anything that doesn't depend on the layout / views in here.
   */
  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    // So this fragment doesn't restart on configuration changes; i.e. rotation.
    setRetainInstance(true);

    mPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity());

    String[] gamePaths = getArguments().getStringArray(KEY_GAMEPATHS);
    mEmulationState = new EmulationState(gamePaths, getTemporaryStateFilePath());
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
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      mEmulationState.run(activity.isActivityRecreated());
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
      LocalBroadcastManager.getInstance(getActivity()).unregisterReceiver(directoryStateReceiver);
      directoryStateReceiver = null;
    }

    if (mEmulationState.isRunning())
      mEmulationState.pause();
    super.onPause();
  }

  @Override
  public void onDetach()
  {
    NativeLibrary.clearEmulationActivity();
    super.onDetach();
  }

  private void setupDolphinDirectoriesThenStartEmulation()
  {
    IntentFilter statusIntentFilter = new IntentFilter(
            DirectoryInitialization.BROADCAST_ACTION);

    directoryStateReceiver =
            new DirectoryStateReceiver(directoryInitializationState ->
            {
              if (directoryInitializationState ==
                      DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
              {
                mEmulationState.run(activity.isActivityRecreated());
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
    LocalBroadcastManager.getInstance(getActivity()).registerReceiver(
            directoryStateReceiver,
            statusIntentFilter);
    DirectoryInitialization.start(getActivity());
  }

  public void toggleInputOverlayVisibility()
  {
    SharedPreferences.Editor editor = mPreferences.edit();

    // If the overlay is currently set to INVISIBLE
    if (!mPreferences.getBoolean("showInputOverlay", false))
    {
      editor.putBoolean("showInputOverlay", true);
    }
    else
    {
      editor.putBoolean("showInputOverlay", false);
    }
    editor.commit();
    mInputOverlay.refreshControls();
  }

  public void initInputPointer()
  {
    mInputOverlay.initTouchPointer();
  }

  public void refreshInputOverlay()
  {
    mInputOverlay.refreshControls();
  }

  public void resetInputOverlay()
  {
    mInputOverlay.resetButtonPlacement();
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
    Log.debug("[EmulationFragment] Surface changed. Resolution: " + width + "x" + height);
    mEmulationState.newSurface(holder.getSurface());
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder)
  {
    mEmulationState.clearSurface();
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
    private Thread mEmulationThread;
    private State state;
    private Surface mSurface;
    private boolean mRunWhenSurfaceIsValid;
    private boolean loadPreviousTemporaryState;
    private final String temporaryStatePath;

    EmulationState(String[] gamePaths, String temporaryStatePath)
    {
      mGamePaths = gamePaths;
      this.temporaryStatePath = temporaryStatePath;
      // Starting state is stopped.
      state = State.STOPPED;
    }

    // Getters for the current state

    public synchronized boolean isStopped()
    {
      return state == State.STOPPED;
    }

    public synchronized boolean isPaused()
    {
      return state == State.PAUSED;
    }

    public synchronized boolean isRunning()
    {
      return state == State.RUNNING;
    }

    // State changing methods

    public synchronized void stop()
    {
      if (state != State.STOPPED)
      {
        Log.debug("[EmulationFragment] Stopping emulation.");
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
      if (state != State.PAUSED)
      {
        state = State.PAUSED;
        Log.debug("[EmulationFragment] Pausing emulation.");

        // Release the surface before pausing, since emulation has to be running for that.
        NativeLibrary.SurfaceDestroyed();
        NativeLibrary.PauseEmulation();
      }
      else
      {
        Log.warning("[EmulationFragment] Pause called while already paused.");
      }
    }

    public synchronized void run(boolean isActivityRecreated)
    {
      if (isActivityRecreated)
      {
        if (NativeLibrary.IsRunning())
        {
          loadPreviousTemporaryState = false;
          state = State.PAUSED;
          deleteFile(temporaryStatePath);
        }
        else
        {
          loadPreviousTemporaryState = true;
        }
      }
      else
      {
        Log.debug("[EmulationFragment] activity resumed or fresh start");
        loadPreviousTemporaryState = false;
        // activity resumed without being killed or this is the first run
        deleteFile(temporaryStatePath);
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
    public synchronized void newSurface(Surface surface)
    {
      mSurface = surface;
      if (mRunWhenSurfaceIsValid)
      {
        runWithValidSurface();
      }
    }

    public synchronized void clearSurface()
    {
      if (mSurface == null)
      {
        Log.warning("[EmulationFragment] clearSurface called, but surface already null.");
      }
      else
      {
        mSurface = null;
        Log.debug("[EmulationFragment] Surface destroyed.");

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
        mEmulationThread = new Thread(() ->
        {
          NativeLibrary.SurfaceChanged(mSurface);
          if (loadPreviousTemporaryState)
          {
            Log.debug("[EmulationFragment] Starting emulation thread from previous state.");
            NativeLibrary.Run(mGamePaths, temporaryStatePath, true);
          }
          else
          {
            Log.debug("[EmulationFragment] Starting emulation thread.");
            NativeLibrary.Run(mGamePaths);
          }
        }, "NativeEmulation");
        mEmulationThread.start();

      }
      else if (state == State.PAUSED)
      {
        Log.debug("[EmulationFragment] Resuming emulation.");
        NativeLibrary.SurfaceChanged(mSurface);
        NativeLibrary.UnPauseEmulation();
      }
      else
      {
        Log.debug("[EmulationFragment] Bug, run called while already running.");
      }
      state = State.RUNNING;
    }
  }

  public void saveTemporaryState()
  {
    NativeLibrary.SaveStateAs(getTemporaryStateFilePath(), true);
  }

  private String getTemporaryStateFilePath()
  {
    return getContext().getFilesDir() + File.separator + "temp.sav";
  }

  private static void deleteFile(String path)
  {
    try
    {
      File file = new File(path);
      file.delete();
    }
    catch (Exception ex)
    {
      // fail safely
    }
  }
}
