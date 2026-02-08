// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.os.Looper;

public class LooperThread extends Thread
{
  private Looper mLooper;

  public LooperThread()
  {
    super();
  }

  public LooperThread(String name)
  {
    super(name);
  }

  @Override
  public void run()
  {
    Looper.prepare();

    synchronized (this)
    {
      mLooper = Looper.myLooper();
      notifyAll();
    }

    Looper.loop();
  }

  public Looper getLooper()
  {
    if (!isAlive())
    {
      throw new IllegalStateException();
    }

    synchronized (this)
    {
      while (mLooper == null)
      {
        try
        {
          wait();
        }
        catch (InterruptedException ignored)
        {
        }
      }
    }

    return mLooper;
  }
}
