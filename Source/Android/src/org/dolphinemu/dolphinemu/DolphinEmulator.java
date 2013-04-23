package org.dolphinemu.dolphinemu;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import net.simonvt.menudrawer.MenuDrawer;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.MotionEvent;
import android.view.WindowManager;

public class DolphinEmulator<MainActivity> extends Activity 
{	
	static private NativeGLSurfaceView GLview = null;
	static private boolean Running = false;
	
	private float screenWidth;
	private float screenHeight;
	
	public static native void onTouchEvent(int Action, float X, float Y);
	
	static
	{
		try
		{
			System.loadLibrary("dolphin-emu-nogui"); 
		}
		catch (Exception ex)
		{
			Log.w("me", ex.toString());
		}
	}
	private void CopyAsset(String asset, String output) {
        InputStream in = null;
        OutputStream out = null;
        try {
          in = getAssets().open(asset);
          out = new FileOutputStream(output);
          copyFile(in, out);
          in.close();
          in = null;
          out.flush();
          out.close();
          out = null;
        } catch(IOException e) {
            Log.e("tag", "Failed to copy asset file: " + asset, e);
        }       
	}
	private void copyFile(InputStream in, OutputStream out) throws IOException {
	    byte[] buffer = new byte[1024];
	    int read;
	    while((read = in.read(buffer)) != -1){
	      out.write(buffer, 0, read);
	    }
	}
	
	@Override
	public void onStop()
	{
		super.onStop();
		if (Running)
			NativeGLSurfaceView.StopEmulation();
	}
	@Override
	public void onPause()
	{
		super.onPause();
		if (Running)
			NativeGLSurfaceView.PauseEmulation();
	}
	@Override
	public void onResume()
	{
		super.onResume();
		if (Running)
			NativeGLSurfaceView.UnPauseEmulation();
	}

    /** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (savedInstanceState == null)
		{

			Intent ListIntent = new Intent(this, GameListView.class);
			startActivityForResult(ListIntent, 1);
			
			// Make the assets directory
			String strDir = Environment.getExternalStorageDirectory()+File.separator+"dolphin-emu";
			File directory = new File(strDir);
			directory.mkdirs();
			
			strDir += File.separator+"Config";
			directory = new File(strDir);
			directory.mkdirs();
			
			// Copy assets if needed
			java.io.File file = new java.io.File(
					Environment.getExternalStorageDirectory()+File.separator+"dolphin-emu" + File.separator + "NoBanner.png");
			if(!file.exists())
			{
				CopyAsset("ButtonA.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "ButtonA.png");
				CopyAsset("ButtonB.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "ButtonB.png");
				CopyAsset("ButtonStart.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "ButtonStart.png");
				CopyAsset("NoBanner.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "NoBanner.png");
				CopyAsset("Dolphin.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "Dolphin.png");
				CopyAsset("Back.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "Back.png");
				CopyAsset("Folder.png", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "Folder.png");
				CopyAsset("Background.glsl", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "Background.glsl");
				CopyAsset("GCPadNew.ini", 
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator +"Config"+ File.separator +"GCPadNew.ini");
			}
		}
	}

	@Override
	public void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		super.onActivityResult(requestCode, resultCode, data);
		
		if (resultCode == Activity.RESULT_OK)
		{
			DisplayMetrics displayMetrics = new DisplayMetrics();
			WindowManager wm = (WindowManager) getApplicationContext().getSystemService(getApplicationContext().WINDOW_SERVICE); // the results will be higher than using the activity context object or the getWindowManager() shortcut
			wm.getDefaultDisplay().getMetrics(displayMetrics);
			screenWidth = displayMetrics.widthPixels;
			screenHeight = displayMetrics.heightPixels;
			
			String FileName = data.getStringExtra("Select");
			GLview = new NativeGLSurfaceView(this);
			//this.getWindow().setUiOptions(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN, View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN);
			GLview.SetDimensions(screenWidth, screenHeight);
			GLview.SetFileName(FileName);
			setContentView(GLview);
			Running = true;
		}
	}
	
	@Override
	public boolean onTouchEvent(MotionEvent event)
	{
		float X, Y;
		int Action;
		X = event.getX();
		Y = event.getY();
		Action = event.getActionMasked();
		
		// Converts button locations 0 - 1 to OGL screen coords -1.0 - 1.0
		float ScreenX = ((X / screenWidth) * 2.0f) - 1.0f;
		float ScreenY = ((Y / screenHeight) * -2.0f) + 1.0f;

		onTouchEvent(Action, ScreenX, ScreenY);
		
		return false;
	}
	
	public boolean overrideKeys()
	{   
		return false;
	}  

}                                