// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import org.dolphinemu.dolphinemu.utils.WiiUpdateCallback;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class SystemUpdateViewModel extends ViewModel
{
  private static final ExecutorService executor = Executors.newFixedThreadPool(1);

  private final MutableLiveData<Integer> mProgressData = new MutableLiveData<>();
  private final MutableLiveData<Integer> mTotalData = new MutableLiveData<>();
  private final MutableLiveData<Long> mTitleIdData = new MutableLiveData<>();
  private final MutableLiveData<Integer> mResultData = new MutableLiveData<>();

  private boolean mCanceled = false;
  private int mRegion;
  private String mDiscPath;

  public SystemUpdateViewModel()
  {
    clear();
  }

  public void setRegion(int region)
  {
    mRegion = region;
  }

  public int getRegion()
  {
    return mRegion;
  }

  public void setDiscPath(String discPath)
  {
    mDiscPath = discPath;
  }

  public String getDiscPath()
  {
    return mDiscPath;
  }

  public MutableLiveData<Integer> getProgressData()
  {
    return mProgressData;
  }

  public MutableLiveData<Integer> getTotalData()
  {
    return mTotalData;
  }

  public MutableLiveData<Long> getTitleIdData()
  {
    return mTitleIdData;
  }

  public MutableLiveData<Integer> getResultData()
  {
    return mResultData;
  }

  public void setCanceled()
  {
    mCanceled = true;
  }

  public void startUpdate()
  {
    if (!mDiscPath.isEmpty())
    {
      startDiscUpdate(mDiscPath);
    }
    else
    {
      final String region;
      switch (mRegion)
      {
        case 0:
          region = "EUR";
          break;
        case 1:
          region = "JPN";
          break;
        case 2:
          region = "KOR";
          break;
        case 3:
          region = "USA";
          break;
        default:
          region = "";
          break;
      }
      startOnlineUpdate(region);
    }
  }

  public void startOnlineUpdate(String region)
  {
    mCanceled = false;

    executor.execute(() ->
    {
      int result = WiiUtils.doOnlineUpdate(region, constructCallback());
      mResultData.postValue(result);
    });
  }

  public void startDiscUpdate(String path)
  {
    mCanceled = false;

    executor.execute(() ->
    {
      int result = WiiUtils.doDiscUpdate(path, constructCallback());
      mResultData.postValue(result);
    });
  }

  public void clear()
  {
    mProgressData.setValue(0);
    mTotalData.setValue(0);
    mTitleIdData.setValue(0l);
    mResultData.setValue(-1);
    mCanceled = false;
    mRegion = -1;
    mDiscPath = "";
  }

  private WiiUpdateCallback constructCallback()
  {
    return (processed, total, titleId) ->
    {
      mProgressData.postValue(processed);
      mTotalData.postValue(total);
      mTitleIdData.postValue(titleId);

      return !mCanceled;
    };
  }
}
