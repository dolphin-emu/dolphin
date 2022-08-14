// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import androidx.core.app.ComponentActivity;
import androidx.lifecycle.Observer;

import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;

public class AfterDirectoryInitializationRunner
{
  private Observer<DirectoryInitializationState> mObserver;

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
  public void runWithLifecycle(ComponentActivity activity, Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runnable.run();
    }
    else
    {
      mObserver = createObserver(runnable);
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
  public void runWithoutLifecycle(Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runnable.run();
    }
    else
    {
      mObserver = createObserver(runnable);
      DirectoryInitialization.getDolphinDirectoriesState().observeForever(mObserver);
    }
  }

  private Observer<DirectoryInitializationState> createObserver(Runnable runnable)
  {
    return (state) ->
    {
      if (state == DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
      {
        cancel();
        runnable.run();
      }
    };
  }

  public void cancel()
  {
    DirectoryInitialization.getDolphinDirectoriesState().removeObserver(mObserver);
  }
}
