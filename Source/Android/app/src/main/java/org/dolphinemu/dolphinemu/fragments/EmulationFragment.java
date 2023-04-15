// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments;

import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.FragmentEmulationBinding;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;

public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback
{
  private static final String KEY_GAMEPATHS = "gamepaths";
  private static final String KEY_RIIVOLUTION = "riivolution";
  private static final String KEY_SYSTEM_MENU = "systemMenu";

  private InputOverlay mInputOverlay;

  private String[] mGamePaths;
  private boolean mRiivolution;
  private boolean mRunWhenSurfaceIsValid;
  private boolean mLoadPreviousTemporaryState;
  private boolean mLaunchSystemMenu;

  private EmulationActivity activity;

  private FragmentEmulationBinding mBinding;

  public static EmulationFragment newInstance(String[] gamePaths, boolean riivolution,
          boolean systemMenu)
  {
    Bundle args = new Bundle();
    args.putStringArray(KEY_GAMEPATHS, gamePaths);
    args.putBoolean(KEY_RIIVOLUTION, riivolution);
    args.putBoolean(KEY_SYSTEM_MENU, systemMenu);

    EmulationFragment fragment = new EmulationFragment();
    fragment.setArguments(args);
    return fragment;
  }

  @Override
  public void onAttach(@NonNull Context context)
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

    mGamePaths = getArguments().getStringArray(KEY_GAMEPATHS);
    mRiivolution = getArguments().getBoolean(KEY_RIIVOLUTION);
    mLaunchSystemMenu = getArguments().getBoolean(KEY_SYSTEM_MENU);
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    mBinding = FragmentEmulationBinding.inflate(inflater, container, false);
    return mBinding.getRoot();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    // The new Surface created here will get passed to the native code via onSurfaceChanged.
    SurfaceView surfaceView = mBinding.surfaceEmulation;
    surfaceView.getHolder().addCallback(this);

    mInputOverlay = mBinding.surfaceInputOverlay;

    Button doneButton = mBinding.doneControlConfig;
    if (doneButton != null)
    {
      doneButton.setOnClickListener(v -> stopConfiguringControls());
    }

    if (mInputOverlay != null)
    {
      view.post(() ->
      {
        int overlayX = mInputOverlay.getLeft();
        int overlayY = mInputOverlay.getTop();
        mInputOverlay.setSurfacePosition(new Rect(
                surfaceView.getLeft() - overlayX, surfaceView.getTop() - overlayY,
                surfaceView.getRight() - overlayX, surfaceView.getBottom() - overlayY));
      });
    }
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  @Override
  public void onResume()
  {
    super.onResume();

    if (mInputOverlay != null && NativeLibrary.IsGameMetadataValid())
      mInputOverlay.refreshControls();

    run(activity.isActivityRecreated());
  }

  @Override
  public void onPause()
  {
    if (NativeLibrary.IsRunningAndUnpaused() && !NativeLibrary.IsShowingAlertMessage())
    {
      Log.debug("[EmulationFragment] Pausing emulation.");
      NativeLibrary.PauseEmulation();
    }

    super.onPause();
  }

  @Override
  public void onDestroy()
  {
    if (mInputOverlay != null)
      mInputOverlay.onDestroy();

    super.onDestroy();
  }

  @Override
  public void onDetach()
  {
    NativeLibrary.clearEmulationActivity();
    super.onDetach();
  }

  public void toggleInputOverlayVisibility(Settings settings)
  {
    BooleanSetting.MAIN_SHOW_INPUT_OVERLAY
            .setBoolean(settings, !BooleanSetting.MAIN_SHOW_INPUT_OVERLAY.getBoolean());

    if (mInputOverlay != null)
      mInputOverlay.refreshControls();
  }

  public void initInputPointer()
  {
    if (mInputOverlay != null)
      mInputOverlay.initTouchPointer();
  }

  public void refreshInputOverlay()
  {
    if (mInputOverlay != null)
      mInputOverlay.refreshControls();
  }

  public void refreshOverlayPointer()
  {
    if (mInputOverlay != null)
      mInputOverlay.refreshOverlayPointer();
  }

  public void resetInputOverlay()
  {
    if (mInputOverlay != null)
      mInputOverlay.resetButtonPlacement();
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder)
  {
    // We purposely don't do anything here.
    // All work is done in surfaceChanged, which we are guaranteed to get even for surface creation.
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
  {
    Log.debug("[EmulationFragment] Surface changed. Resolution: " + width + "x" + height);
    NativeLibrary.SurfaceChanged(holder.getSurface());
    if (mRunWhenSurfaceIsValid)
    {
      runWithValidSurface();
    }
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder)
  {
    Log.debug("[EmulationFragment] Surface destroyed.");
    NativeLibrary.SurfaceDestroyed();
    mRunWhenSurfaceIsValid = true;
  }

  public void stopEmulation()
  {
    Log.debug("[EmulationFragment] Stopping emulation.");
    NativeLibrary.StopEmulation();
  }

  public void startConfiguringControls()
  {
    if (mInputOverlay != null)
    {
      mBinding.doneControlConfig.setVisibility(View.VISIBLE);
      mInputOverlay.setIsInEditMode(true);
    }
  }

  public void stopConfiguringControls()
  {
    if (mInputOverlay != null)
    {
      mBinding.doneControlConfig.setVisibility(View.GONE);
      mInputOverlay.setIsInEditMode(false);
    }
  }

  public boolean isConfiguringControls()
  {
    return mInputOverlay != null && mInputOverlay.isInEditMode();
  }

  private void run(boolean isActivityRecreated)
  {
    if (isActivityRecreated)
    {
      if (NativeLibrary.IsRunning())
      {
        mLoadPreviousTemporaryState = false;
        deleteFile(getTemporaryStateFilePath());
      }
      else
      {
        mLoadPreviousTemporaryState = true;
      }
    }
    else
    {
      Log.debug("[EmulationFragment] activity resumed or fresh start");
      mLoadPreviousTemporaryState = false;
      // activity resumed without being killed or this is the first run
      deleteFile(getTemporaryStateFilePath());
    }

    // If the surface is set, run now. Otherwise, wait for it to get set.
    if (NativeLibrary.HasSurface())
    {
      runWithValidSurface();
    }
    else
    {
      mRunWhenSurfaceIsValid = true;
    }
  }

  private void runWithValidSurface()
  {
    mRunWhenSurfaceIsValid = false;
    if (!NativeLibrary.IsRunning())
    {
      NativeLibrary.SetIsBooting();

      Thread emulationThread = new Thread(() ->
      {
        if (mLoadPreviousTemporaryState)
        {
          Log.debug("[EmulationFragment] Starting emulation thread from previous state.");
          NativeLibrary.Run(mGamePaths, mRiivolution, getTemporaryStateFilePath(), true);
        }
        if (mLaunchSystemMenu)
        {
          Log.debug("[EmulationFragment] Starting emulation thread for the Wii Menu.");
          NativeLibrary.RunSystemMenu();
        }
        else
        {
          Log.debug("[EmulationFragment] Starting emulation thread.");
          NativeLibrary.Run(mGamePaths, mRiivolution);
        }
        EmulationActivity.stopIgnoringLaunchRequests();
      }, "NativeEmulation");
      emulationThread.start();
    }
    else
    {
      if (!EmulationActivity.getHasUserPausedEmulation() && !NativeLibrary.IsShowingAlertMessage())
      {
        Log.debug("[EmulationFragment] Resuming emulation.");
        NativeLibrary.UnPauseEmulation();
      }
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
      if (!file.delete())
      {
        Log.error("[EmulationFragment] Failed to delete " + file.getAbsolutePath());
      }
    }
    catch (Exception ignored)
    {
    }
  }
}
