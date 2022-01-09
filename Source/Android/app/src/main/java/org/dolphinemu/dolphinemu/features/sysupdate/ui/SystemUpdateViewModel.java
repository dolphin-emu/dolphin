package org.dolphinemu.dolphinemu.features.sysupdate.ui;

import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

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

  public void startUpdate(String region)
  {
    mCanceled = false;

    executor.execute(() ->
    {
      int result = WiiUtils.doOnlineUpdate(region, ((processed, total, titleId) ->
      {
        mProgressData.postValue(processed);
        mTotalData.postValue(total);
        mTitleIdData.postValue(titleId);

        return !mCanceled;
      }));

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
  }
}
