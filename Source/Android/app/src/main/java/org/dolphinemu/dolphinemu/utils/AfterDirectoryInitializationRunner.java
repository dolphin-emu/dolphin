// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.dolphinemu.dolphinemu.utils.DirectoryInitialization.DirectoryInitializationState;

public class AfterDirectoryInitializationRunner
{
  private Observer<DirectoryInitializationState> mObserver;

  /**
   * Executes a Runnable once directory initialization finishes.
   *
   * If this is called when directory initialization already has finished, the Runnable will
   * be executed immediately. If this is called before directory initialization has finished,
   * the Runnable will be executed after directory initialization finishes.
   *
   * If the passed-in LifecycleOwner gets destroyed before this operation finishes,
   * the operation will be automatically canceled.
   */
  public void runWithLifecycle(LifecycleOwner lifecycleOwner, Runnable runnable)
  {
    if (DirectoryInitialization.areDolphinDirectoriesReady())
    {
      runnable.run();
    }
    else
    {
      mObserver = createObserver(runnable);
      DirectoryInitialization.getDolphinDirectoriesState().observe(lifecycleOwner, mObserver);
    }
  }

  /**
   * Executes a Runnable once directory initialization finishes.
   *
   * If this is called when directory initialization already has finished, the Runnable will
   * be executed immediately. If this is called before directory initialization has finished,
   * the Runnable will be executed after directory initialization finishes.
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
