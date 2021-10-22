// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.model;

import android.content.Context;

import androidx.annotation.Keep;

public class GameFile
{
  @Keep
  private long mPointer;

  @Keep
  private GameFile(long pointer)
  {
    mPointer = pointer;
  }

  public native static GameFile parse(String path);

  @Override
  public native void finalize();

  public native int getPlatform();

  public native String getTitle();

  public native String getDescription();

  public native String getCompany();

  public native int getCountry();

  public native int getRegion();

  public native String getPath();

  public native String getGameId();

  public native String getGameTdbId();

  public native int getDiscNumber();

  public native int getRevision();

  public native int getBlobType();

  public native String getFileFormatName();

  public native long getBlockSize();

  public native String getCompressionMethod();

  public native boolean shouldShowFileFormatDetails();

  public native boolean shouldAllowConversion();

  public native long getFileSize();

  public native boolean isDatelDisc();

  public native boolean isNKit();

  public native int[] getBanner();

  public native int getBannerWidth();

  public native int getBannerHeight();

  public String getCoverPath(Context context)
  {
    return context.getExternalCacheDir().getPath() + "/GameCovers/" + getGameTdbId() + ".png";
  }

  public String getCustomCoverPath()
  {
    return getPath().substring(0, getPath().lastIndexOf(".")) + ".cover.png";
  }
}
