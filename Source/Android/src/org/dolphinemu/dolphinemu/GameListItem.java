package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public class GameListItem implements Comparable<GameListItem>{
    private String name;
    private String data;
    private String path;
    private Bitmap image;
	private boolean m_valid;

    public GameListItem(Context ctx, String n,String d,String p, boolean valid)
    {
        name = n;
        data = d;
        path = p;
	    m_valid = valid;
        File file = new File(path);
        if (!file.isDirectory() && !path.equals(""))
        {
        	int[] Banner = NativeLibrary.GetBanner(path);
        	if (Banner[0] == 0)
        	{
        		try {
					InputStream path = ctx.getAssets().open("NoBanner.png");
					image = BitmapFactory.decodeStream(path);
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
        		
        	}
        	else
        		image = Bitmap.createBitmap(Banner, 96, 32, Bitmap.Config.ARGB_8888);
        	name = NativeLibrary.GetTitle(path);
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
		return m_valid;
	}
    
    public int compareTo(GameListItem o) 
    {
        if(this.name != null)
            return this.name.toLowerCase().compareTo(o.getName().toLowerCase()); 
        else 
            throw new IllegalArgumentException();
    }
}

