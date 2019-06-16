package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;

public class AfterDirectoryInitializationRunner
{
  private DirectoryStateReceiver directoryStateReceiver;

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
   */
  public void run(Context context, Runnable runnable)
  {
    if (!DirectoryInitialization.areDolphinDirectoriesReady())
    {
      // Wait for directories to get initialized
      IntentFilter statusIntentFilter = new IntentFilter(
              DirectoryInitialization.BROADCAST_ACTION);

      directoryStateReceiver = new DirectoryStateReceiver(directoryInitializationState ->
      {
        if (directoryInitializationState ==
                DirectoryInitialization.DirectoryInitializationState.DOLPHIN_DIRECTORIES_INITIALIZED)
        {
          LocalBroadcastManager.getInstance(context).unregisterReceiver(directoryStateReceiver);
          directoryStateReceiver = null;
          runnable.run();
        }
      });
      // Registers the DirectoryStateReceiver and its intent filters
      LocalBroadcastManager.getInstance(context).registerReceiver(
              directoryStateReceiver,
              statusIntentFilter);
    }
    else
    {
      runnable.run();
    }
  }
}
