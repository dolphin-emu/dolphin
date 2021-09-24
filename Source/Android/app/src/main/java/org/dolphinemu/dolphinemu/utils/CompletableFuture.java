// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Simplified re-implementation of a subset of {@link java.util.concurrent.CompletableFuture}.
 * Replace this class with that class once we have full Java 8 support (once we require API 24).
 */
public class CompletableFuture<T> implements Future<T>
{
  private final Lock lock = new ReentrantLock();
  private final Condition done = lock.newCondition();

  private boolean isDone = false;
  private T result = null;

  @Override
  public boolean cancel(boolean mayInterruptIfRunning)
  {
    throw new UnsupportedOperationException();
  }

  @Override
  public boolean isCancelled()
  {
    return false;
  }

  @Override
  public boolean isDone()
  {
    return isDone;
  }

  @Override
  public T get() throws ExecutionException, InterruptedException
  {
    lock.lock();
    try
    {
      while (!isDone)
        done.await();

      return result;
    }
    finally
    {
      lock.unlock();
    }
  }

  @Override
  public T get(long timeout, TimeUnit unit)
          throws ExecutionException, InterruptedException, TimeoutException
  {
    lock.lock();
    try
    {
      while (!isDone)
      {
        if (!done.await(timeout, unit))
          throw new TimeoutException();
      }

      return result;
    }
    finally
    {
      lock.unlock();
    }
  }

  public boolean complete(T value)
  {
    lock.lock();
    try
    {
      boolean wasDone = isDone;
      result = value;
      isDone = true;
      done.signalAll();
      return !wasDone;
    }
    finally
    {
      lock.unlock();
    }
  }
}
