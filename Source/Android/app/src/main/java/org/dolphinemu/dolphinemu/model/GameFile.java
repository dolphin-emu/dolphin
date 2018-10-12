package org.dolphinemu.dolphinemu.model;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.widget.ImageView;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.CoverHelper;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class GameFile
{
	// Do not rename or move without editing the native code
	private long mPointer;

	private GameFile(long pointer)
	{
		mPointer = pointer;
	}

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

	public native int[] getBanner();

	public native int getBannerWidth();

	public native int getBannerHeight();

	public native String[] getCodes();

	public String getCoverPath()
	{
		return DirectoryInitialization.getCoverDirectory() + File.separator + getGameId() + ".png";
	}

	public List<String> getSavedStates()
	{
		final int NUM_STATES = 10;
		final String statePath = DirectoryInitialization.getDolphinDirectory() + "/StateSaves/";
		final String gameId = getGameId();
		long lastModified = Long.MAX_VALUE;
		ArrayList<String> savedStates = new ArrayList<>();
		for (int i = 1; i < NUM_STATES; ++i)
		{
			String filename = String.format("%s%s.s%02d", statePath, gameId, i);
			File stateFile = new File(filename);
			if (stateFile.exists())
			{
				if (stateFile.lastModified() < lastModified)
				{
					savedStates.add(0, filename);
					lastModified = stateFile.lastModified();
				}
				else
				{
					savedStates.add(filename);
				}
			}
		}
		return savedStates;
	}

	public String getLastSavedState()
	{
		final int NUM_STATES = 10;
		final String statePath = DirectoryInitialization.getDolphinDirectory() + "/StateSaves/";
		final String gameId = getGameId();
		long lastModified = Long.MAX_VALUE;
		String savedState = null;
		for (int i = 1; i < NUM_STATES; ++i)
		{
			String filename = String.format("%s%s.s%02d", statePath, gameId, i);
			File stateFile = new File(filename);
			if (stateFile.exists())
			{
				if (stateFile.lastModified() < lastModified)
				{
					savedState = filename;
					lastModified = stateFile.lastModified();
				}
			}
		}
		return savedState;
	}

	private static final int COVER_UNKNOWN = 0;
	private static final int COVER_ASSETS = 1;
	private static final int COVER_CACHE = 2;
	private static final int COVER_NONE = 3;
	private int mCoverType = COVER_UNKNOWN;
	public void loadGameBanner(ImageView imageView)
	{
		if(mCoverType == COVER_UNKNOWN)
		{
			String gameId = getGameId();
			if(isCoverInAssets(imageView.getContext(), gameId))
			{
				mCoverType = COVER_ASSETS;
				loadFromAssets(imageView, null);
			}
			else if(isCoverInCache(gameId))
			{
				mCoverType = COVER_CACHE;
				loadFromCache(imageView, null);
			}
			else
			{
				mCoverType = COVER_NONE;
				loadFromNetwork(imageView, new Callback()
				{
					@Override public void onSuccess()
					{
						mCoverType = COVER_CACHE;
						CoverHelper.saveCover(((BitmapDrawable) imageView.getDrawable()).getBitmap(), getCoverPath());
					}
					@Override public void onError()
					{
						if(!NativeLibrary.isNetworkConnected(imageView.getContext()))
							return;
						CoverHelper.saveCover(((BitmapDrawable) imageView.getDrawable()).getBitmap(), getCoverPath());
					}
				});
			}
		}
		else if(mCoverType == COVER_ASSETS)
		{
			loadFromAssets(imageView, null);
		}
		else if(mCoverType == COVER_CACHE)
		{
			loadFromCache(imageView, null);
		}
		else if(mCoverType == COVER_NONE)
		{
			imageView.setImageResource(R.drawable.no_banner);
		}
	}

	private void loadFromAssets(ImageView imageView, Callback callback)
	{
		Picasso.with(imageView.getContext())
			.load("file:///android_asset/GameCovers/" + getGameId() + ".png")
			.placeholder(R.drawable.no_banner)
			.error(R.drawable.no_banner)
			.into(imageView, callback);
	}

	private void loadFromCache(ImageView imageView, Callback callback)
	{
		Picasso.with(imageView.getContext())
			.load("file://" + getCoverPath())
			.placeholder(R.drawable.no_banner)
			.error(R.drawable.no_banner)
			.into(imageView, callback);
	}

	private void loadFromNetwork(ImageView imageView, Callback callback)
	{
		Picasso.with(imageView.getContext())
			.load(CoverHelper.buildGameTDBUrl(this))
			.placeholder(R.drawable.no_banner)
			.error(R.drawable.no_banner)
			.into(imageView, callback);
	}

	private static String[] sAssetsCovers;
	private static boolean isCoverInAssets(Context context, String gameId)
	{
		if(sAssetsCovers == null)
		{
			try
			{
				sAssetsCovers = context.getAssets().list("GameCovers");
			}
			catch (IOException e)
			{
				sAssetsCovers = new String[0];
			}
		}

		if(sAssetsCovers != null)
		{
			for(String cover : sAssetsCovers)
			{
				if(cover.contains(gameId))
				{
					return true;
				}
			}
		}
		return false;
	}

	private static String[] sCacheCovers;
	private static boolean isCoverInCache(String gameId)
	{
		if(sCacheCovers == null)
		{
			File dir = new File(DirectoryInitialization.getCoverDirectory());
			if(dir.exists())
			{
				sCacheCovers = dir.list();
			}
		}

		if(sCacheCovers != null)
		{
			for(String cover : sCacheCovers)
			{
				if(cover.contains(gameId))
				{
					return true;
				}
			}
		}
		return false;
	}
}
