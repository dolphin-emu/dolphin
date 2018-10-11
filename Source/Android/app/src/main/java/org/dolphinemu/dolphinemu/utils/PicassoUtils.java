package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.widget.ImageView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;

import java.io.File;
import java.io.IOException;

public class PicassoUtils
{
	private static boolean isNetworkConnected(Context context)
	{
		ConnectivityManager cm = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
		return activeNetwork != null && activeNetwork.isConnected();
	}

	public static void loadGameBanner(ImageView imageView, GameFile gameFile)
	{
		String gameId = gameFile.getGameId();
		try
		{
			String[] files = imageView.getContext().getAssets().list("GameCovers");
			for(String f : files)
			{
				if(f.contains(gameId))
				{
					Picasso.with(imageView.getContext()).load("file:///android_asset/GameCovers/" + f)
						.noPlaceholder()
						.error(R.drawable.no_banner)
						.into(imageView);
					return;
				}
			}
		}
		catch (IOException e)
		{
			//
		}

		File cover = new File(gameFile.getCustomCoverPath());
		if (cover.exists())
		{
			Picasso.with(imageView.getContext())
				.load(cover)
				.noPlaceholder()
				.error(R.drawable.no_banner)
				.into(imageView);
		}
		else if ((cover = new File(gameFile.getCoverPath())).exists())
		{
			Picasso.with(imageView.getContext())
				.load(cover)
				.noPlaceholder()
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
				.placeholder(R.drawable.no_banner)
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
					public void onError()
					{
						if(!isNetworkConnected(imageView.getContext()))
							return;
						CoverHelper.saveCover(((BitmapDrawable) imageView.getDrawable()).getBitmap(),
							gameFile.getCoverPath());
					}
				});
		}
	}
}
