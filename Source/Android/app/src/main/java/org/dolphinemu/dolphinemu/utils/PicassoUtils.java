// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.view.View;
import android.widget.ImageView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.viewholders.GameViewHolder;

import java.io.File;
import java.io.FileNotFoundException;

public class PicassoUtils
{
  public static void loadGameBanner(ImageView imageView, GameFile gameFile)
  {
    Picasso picassoInstance = new Picasso.Builder(imageView.getContext())
            .addRequestHandler(new GameBannerRequestHandler(gameFile))
            .build();

    picassoInstance
            .load(Uri.parse("iso:/" + gameFile.getPath()))
            .fit()
            .noFade()
            .noPlaceholder()
            .config(Bitmap.Config.RGB_565)
            .error(R.drawable.no_banner)
            .into(imageView);
  }

  public static void loadGameCover(GameViewHolder gameViewHolder, ImageView imageView,
          GameFile gameFile)
  {
    if (BooleanSetting.MAIN_SHOW_GAME_TITLES.getBooleanGlobal() && gameViewHolder != null)
    {
      gameViewHolder.textGameTitle.setText(gameFile.getTitle());
      gameViewHolder.textGameTitle.setVisibility(View.VISIBLE);
      gameViewHolder.textGameTitleInner.setVisibility(View.GONE);
      gameViewHolder.textGameCaption.setVisibility(View.VISIBLE);
    }
    else if (gameViewHolder != null)
    {
      gameViewHolder.textGameTitleInner.setText(gameFile.getTitle());
      gameViewHolder.textGameTitle.setVisibility(View.GONE);
      gameViewHolder.textGameCaption.setVisibility(View.GONE);
    }

    String customCoverPath = gameFile.getCustomCoverPath();
    Uri customCoverUri = null;
    boolean customCoverExists = false;
    if (ContentHandler.isContentUri(customCoverPath))
    {
      try
      {
        customCoverUri = ContentHandler.unmangle(customCoverPath);
        customCoverExists = true;
      }
      catch (FileNotFoundException | SecurityException ignored)
      {
        // Let customCoverExists remain false
      }
    }
    else
    {
      customCoverUri = Uri.parse(customCoverPath);
      customCoverExists = new File(customCoverPath).exists();
    }

    Context context = imageView.getContext();
    File cover;
    if (customCoverExists)
    {
      Picasso.get()
              .load(customCoverUri)
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .error(R.drawable.no_banner)
              .into(imageView, new Callback()
              {
                @Override public void onSuccess()
                {
                  PicassoUtils.disableInnerTitle(gameViewHolder);
                }

                @Override public void onError(Exception e)
                {
                  PicassoUtils.enableInnerTitle(gameViewHolder, gameFile);
                }
              });
    }
    else if ((cover = new File(gameFile.getCoverPath(context))).exists())
    {
      Picasso.get()
              .load(cover)
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .error(R.drawable.no_banner)
              .into(imageView, new Callback()
              {
                @Override public void onSuccess()
                {
                  PicassoUtils.disableInnerTitle(gameViewHolder);
                }

                @Override public void onError(Exception e)
                {
                  PicassoUtils.enableInnerTitle(gameViewHolder, gameFile);
                }
              });
    }
    // GameTDB has a pretty close to complete collection for US/EN covers. First pass at getting
    // the cover will be by the disk's region, second will be the US cover, and third EN.
    else if (BooleanSetting.MAIN_USE_GAME_COVERS.getBooleanGlobal())
    {
      Picasso.get()
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
                          gameFile.getCoverPath(context));
                  PicassoUtils.disableInnerTitle(gameViewHolder);
                }

                @Override
                public void onError(Exception ex) // Second pass using US region
                {
                  Picasso.get()
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
                                      gameFile.getCoverPath(context));
                              PicassoUtils.disableInnerTitle(gameViewHolder);
                            }

                            @Override
                            public void onError(Exception ex) // Third and last pass using EN region
                            {
                              Picasso.get()
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
                                                  gameFile.getCoverPath(context));
                                          PicassoUtils.disableInnerTitle(gameViewHolder);
                                        }

                                        @Override
                                        public void onError(Exception ex)
                                        {
                                          PicassoUtils.enableInnerTitle(gameViewHolder, gameFile);
                                        }
                                      });
                            }
                          });
                }
              });
    }
    else
    {
      Picasso.get()
              .load(R.drawable.no_banner)
              .noFade()
              .noPlaceholder()
              .fit()
              .centerInside()
              .config(Bitmap.Config.ARGB_8888)
              .into(imageView, new Callback()
              {
                @Override public void onSuccess()
                {
                  PicassoUtils.disableInnerTitle(gameViewHolder);
                }

                @Override public void onError(Exception e)
                {
                  PicassoUtils.enableInnerTitle(gameViewHolder, gameFile);
                }
              });
    }
  }

  private static void enableInnerTitle(GameViewHolder gameViewHolder, GameFile gameFile)
  {
    if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.getBooleanGlobal())
    {
      gameViewHolder.textGameTitleInner.setVisibility(View.VISIBLE);
    }
  }

  private static void disableInnerTitle(GameViewHolder gameViewHolder)
  {
    if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.getBooleanGlobal())
    {
      gameViewHolder.textGameTitleInner.setVisibility(View.GONE);
    }
  }
}
