/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.gamelist;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import org.dolphinemu.dolphinemu.NativeLibrary;

/**
 * Represents an item in the game list.
 */
public final class GameListItem implements Comparable<GameListItem>
{
	private String name;
	private final String data;
	private final String path;
	private Bitmap image;

	/**
	 * Constructor.
	 * 
	 * @param ctx  The current {@link Context}
	 * @param name The name of this GameListItem.
	 * @param data The subtitle for this GameListItem
	 * @param path The file path for the game represented by this GameListItem.
	 */
	public GameListItem(Context ctx, String name, String data, String path)
	{
		this.name = name;
		this.data = data;
		this.path = path;

		File file = new File(path);
		if (!file.isDirectory() && !path.isEmpty())
		{
			int[] Banner = NativeLibrary.GetBanner(path);
			if (Banner[0] == 0)
			{
				try
				{
					// Open the no banner icon.
					InputStream noBannerPath = ctx.getAssets().open("NoBanner.png");

					// Decode the bitmap.
					image = BitmapFactory.decodeStream(noBannerPath);

					// Scale the bitmap to match other banners.
					image = Bitmap.createScaledBitmap(image, 96, 32, false);
				}
				catch (IOException e)
				{
					Log.e("GameListItem", e.toString());
				}
			}
			else
			{
				image = Bitmap.createBitmap(Banner, 96, 32, Bitmap.Config.ARGB_8888);
			}

			this.name = NativeLibrary.GetTitle(path);
		}
	}

	/**
	 * Gets the name of this GameListItem.
	 * 
	 * @return the name of this GameListItem.
	 */
	public String getName()
	{
		return name;
	}

	/**
	 * Gets the subtitle of this GameListItem.
	 * 
	 * @return the subtitle of this GameListItem.
	 */
	public String getData()
	{
		return data;
	}

	/**
	 * Gets the file path of the game represented by this GameListItem.
	 * 
	 * @return the file path of the game represented by this GameListItem.
	 */
	public String getPath()
	{
		return path;
	}

	/**
	 * Gets the image data for this game as a {@link Bitmap}.
	 * 
	 * @return the image data for this game as a {@link Bitmap}.
	 */
	public Bitmap getImage()
	{
		return image;
	}

	@Override
	public int compareTo(GameListItem o) 
	{
		if (name != null)
			return name.toLowerCase().compareTo(o.getName().toLowerCase()); 
		else 
			throw new NullPointerException("The name of this GameListItem is null");
	}
}

