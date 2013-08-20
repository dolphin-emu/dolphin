package org.dolphinemu.dolphinemu.gamelist;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import org.dolphinemu.dolphinemu.NativeLibrary;

public final class GameListItem implements Comparable<GameListItem>
{
    private String name;
    private final String data;
    private final String path;
    private final boolean isValid;
    private Bitmap image;

    public GameListItem(Context ctx, String name, String data, String path, boolean isValid)
    {
        this.name = name;
        this.data = data;
        this.path = path;
	    this.isValid = isValid;
	    
        File file = new File(path);
        if (!file.isDirectory() && !path.equals(""))
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
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
        		
        	}
        	else
        	{
        		image = Bitmap.createBitmap(Banner, 96, 32, Bitmap.Config.ARGB_8888);
        	}
        	
        	this.name = NativeLibrary.GetTitle(path);
        }
    }
    
    public String getName()
    {
        return name;
    }
    
    public String getData()
    {
        return data;
    }
    
    public String getPath()
    {
        return path;
    }
    
    public Bitmap getImage()
    {
    	return image;
    }
    
	public boolean isValid()
	{
		return isValid;
	}
    
    public int compareTo(GameListItem o) 
    {
        if(this.name != null)
            return this.name.toLowerCase().compareTo(o.getName().toLowerCase()); 
        else 
            throw new IllegalArgumentException();
    }
}

