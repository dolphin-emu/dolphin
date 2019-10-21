package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Request;
import com.squareup.picasso.RequestHandler;

import org.dolphinemu.dolphinemu.model.GameFile;

public class GameBannerRequestHandler extends RequestHandler
{
  private final GameFile mGameFile;

  public GameBannerRequestHandler(GameFile gameFile)
  {
    mGameFile = gameFile;
  }

  @Override
  public boolean canHandleRequest(Request data)
  {
    return true;
  }

  @Override
  public Result load(Request request, int networkPolicy)
  {
    int[] vector = mGameFile.getBanner();
    int width = mGameFile.getBannerWidth();
    int height = mGameFile.getBannerHeight();
    Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    bitmap.setPixels(vector, 0, width, 0, 0, width, height);
    return new Result(bitmap, Picasso.LoadedFrom.DISK);
  }
}
