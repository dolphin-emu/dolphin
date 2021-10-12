// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.IntentFilter;
import android.widget.Toast;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;

public class AfterDirectoryInitializationRunner
{
  private DirectoryStateReceiver mDirectoryStateReceiver;
  private LocalBroadcastManager mLocalBroadcastManager;

  private Runnable mUnregisterCallback;

  /**
   * Sets a Runnable which will be called when:
   *
   * 1. The Runnable supplied to {@link #run} is just about to run, or
   * 2. {@link #run} was called with abortOnFailure == true and there is a failure
   */
  public void setFinishedCallback(Runnable runnable)
  {
    mUnregisterCallback = runnable;
  }

  private void runFinishedCallback()
  {
    if (mUnregisterCallback != null)
    {
      mUnregisterCallback.run();
    }
  }

  /**
   * Executes a Runnable after directory initialization has finished.
   *
   * If this is called when directory initialization already is done,
   * the Runnable will be executed immediately. If this is called before
   * directory initialization is done, the Runnable will be executed
   * after directory initialization finishes successfully, or never
   * in case directory initialization doesn't finish successfully.
   *
   * Calling this function multiple times per object is not supported.
   *
   * If abortOnFailure is true and the user has not granted the required
   * permission or the external storage was not found, a message will be
   * shown to the user and the Runnable will not run. If it is false, the
   * attempt to run the Runnable will never be aborted, and the Runnable
   * is guaranteed to run if directory initialization ever finishes.
   */
  public void run(Context context, boolean abortOnFailure, Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runFinishedCallback();
      runnable.run();
    }
    else if (abortOnFailure &&
            showErrorMessage(context, DirectoryInitialization.getDolphinDirectoriesState(context)))
    {
      runFinishedCallback();
    }
    else
    {
      runAfterInitialization(context, abortOnFailure, runnable);
    }
  }

  private void runAfterInitialization(Context context, boolean abortOnFailure, Runnable runnable)
  {
    mDirectoryStateReceiver = new DirectoryStateReceiver(state ->
    {
      boolean done = state == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED;

      if (!done && abortOnFailure)
      {
        done = showErrorMessage(context, state);
      }

      if (done)
      {
        cancel();
        runFinishedCallback();
      }

      if (state == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
      {
        runnable.run();
      }
    });

    mLocalBroadcastManager = LocalBroadcastManager.getInstance(context);

    IntentFilter statusIntentFilter = new IntentFilter(DirectoryInitialization.BROADCAST_ACTION);
    mLocalBroadcastManager.registerReceiver(mDirectoryStateReceiver, statusIntentFilter);
  }

  public void cancel()
  {
    if (mDirectoryStateReceiver != null)
    {
      mLocalBroadcastManager.unregisterReceiver(mDirectoryStateReceiver);
      mDirectoryStateReceiver = null;
      mLocalBroadcastManager = null;
    }
  }

  private static boolean showErrorMessage(Context context, DirectoryInitializationState state)
  {
    switch (state)
    {
      case EXTERNAL_STORAGE_PERMISSION_NEEDED:
        Toast.makeText(context, R.string.write_permission_needed, Toast.LENGTH_LONG).show();
        return true;

      case CANT_FIND_EXTERNAL_STORAGE:
        Toast.makeText(context, R.string.external_storage_not_mounted, Toast.LENGTH_LONG).show();
        return true;

      default:
        return false;
    }
  }
}
