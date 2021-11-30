// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.widget.Toast;

import androidx.core.app.ComponentActivity;
import androidx.lifecycle.Observer;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;

public class AfterDirectoryInitializationRunner
{
  private Observer<DirectoryInitializationState> mObserver;
  private Runnable mUnregisterCallback;

  /**
   * Sets a Runnable which will be called when:
   *
   * 1. The Runnable supplied to {@link #runWithLifecycle}/{@link #runWithoutLifecycle}
   * is just about to run, or
   * 2. {@link #runWithLifecycle}/{@link #runWithoutLifecycle} was called with
   * abortOnFailure == true and there is a failure
   *
   * @return this
   */
  public AfterDirectoryInitializationRunner setFinishedCallback(Runnable runnable)
  {
    mUnregisterCallback = runnable;
    return this;
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
   * If abortOnFailure is true and external storage was not found, a message
   * will be shown to the user and the Runnable will not run. If it is false,
   * the attempt to run the Runnable will never be aborted, and the Runnable
   * is guaranteed to run if directory initialization ever finishes.
   *
   * If the passed-in activity gets destroyed before this operation finishes,
   * it will be automatically canceled.
   */
  public void runWithLifecycle(ComponentActivity activity, boolean abortOnFailure,
          Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runFinishedCallback();
      runnable.run();
    }
    else if (abortOnFailure &&
            showErrorMessage(activity,
                    DirectoryInitialization.getDolphinDirectoriesState().getValue()))
    {
      runFinishedCallback();
    }
    else
    {
      mObserver = createObserver(activity, abortOnFailure, runnable);
      DirectoryInitialization.getDolphinDirectoriesState().observe(activity, mObserver);
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
   * If abortOnFailure is true and external storage was not found, a message
   * will be shown to the user and the Runnable will not run. If it is false,
   * the attempt to run the Runnable will never be aborted, and the Runnable
   * is guaranteed to run if directory initialization ever finishes.
   */
  public void runWithoutLifecycle(Context context, boolean abortOnFailure, Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runFinishedCallback();
      runnable.run();
    }
    else if (abortOnFailure &&
            showErrorMessage(context,
                    DirectoryInitialization.getDolphinDirectoriesState().getValue()))
    {
      runFinishedCallback();
    }
    else
    {
      mObserver = createObserver(context, abortOnFailure, runnable);
      DirectoryInitialization.getDolphinDirectoriesState().observeForever(mObserver);
    }
  }

  private Observer<DirectoryInitializationState> createObserver(Context context,
          boolean abortOnFailure, Runnable runnable)
  {
    return (state) ->
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
    };
  }

  public void cancel()
  {
    DirectoryInitialization.getDolphinDirectoriesState().removeObserver(mObserver);
  }

  private static boolean showErrorMessage(Context context, DirectoryInitializationState state)
  {
    switch (state)
    {
      case CANT_FIND_EXTERNAL_STORAGE:
        Toast.makeText(context, R.string.external_storage_not_mounted, Toast.LENGTH_LONG).show();
        return true;

      default:
        return false;
    }
  }
}
