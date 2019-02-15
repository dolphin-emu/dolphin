package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.widget.ImageView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;

import java.io.File;

public class PicassoUtils
{
  public static void loadGameBanner(ImageView imageView, GameFile gameFile)
  {
    File cover = new File(gameFile.getCustomCoverPath());
    if (cover.exists())
    {
      Picasso.with(imageView.getContext())
              .load(cover)
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .error(R.drawable.no_banner)
              .into(imageView);
    }
    else if ((cover = new File(gameFile.getCoverPath())).exists())
    {
      Picasso.with(imageView.getContext())
              .load(cover)
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .error(R.drawable.no_banner)
              .into(imageView);
    }
    /**
     * GameTDB has a pretty close to complete collection for US/EN covers. First pass at getting
     * the cover will be by the disk's region, second will be the US cover, and third EN.
     */
    else
    {
      Picasso.with(imageView.getContext())
              .load(CoverHelper.buildGameTDBUrl(gameFile, CoverHelper.getRegion(gameFile)))
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .error(R.drawable.no_banner)
              .into(imageView, new Callback()
              {
                @Override
                public void onSuccess()
                {
                  CoverHelper.saveCover(((BitmapDrawable) imageView.getDrawable()).getBitmap(),
                          gameFile.getCoverPath());
                }

                @Override
                public void onError() // Second pass using US region
                {
                  Picasso.with(imageView.getContext())
                          .load(CoverHelper.buildGameTDBUrl(gameFile, "US"))
                          .fit()
                          .noFade()
                          .fit()
                          .centerInside()
                          .noPlaceholder()
                          .config(Bitmap.Config.ARGB_8888)
                          .error(R.drawable.no_banner)
                          .into(imageView, new Callback()
                          {
                            @Override
                            public void onSuccess()
                            {
                              CoverHelper.saveCover(
                                      ((BitmapDrawable) imageView.getDrawable()).getBitmap(),
                                      gameFile.getCoverPath());
                            }

                            @Override
                            public void onError() // Third and last pass using EN region
                            {
                              Picasso.with(imageView.getContext())
                                      .load(CoverHelper.buildGameTDBUrl(gameFile, "EN"))
                                      .fit()
                                      .noFade()
                                      .fit()
                                      .centerInside()
                                      .noPlaceholder()
                                      .config(Bitmap.Config.ARGB_8888)
                                      .error(R.drawable.no_banner)
                                      .into(imageView, new Callback()
                                      {
                                        @Override
                                        public void onSuccess()
                                        {
                                          CoverHelper.saveCover(
                                                  ((BitmapDrawable) imageView.getDrawable())
                                                          .getBitmap(),
                                                  gameFile.getCoverPath());
                                        }

                                        @Override
                                        public void onError()
                                        {
                                        }
                                      });
                            }
                          });
                }
              });
    }
  }
}
