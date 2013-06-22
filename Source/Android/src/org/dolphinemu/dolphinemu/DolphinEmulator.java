package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;

import java.io.*;
import java.util.List;

public class DolphinEmulator<MainActivity> extends Activity 
{	
	static private NativeGLSurfaceView GLview = null;
	static private boolean Running = false;
	
	private float screenWidth;
	private float screenHeight;

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
			NativeLibrary.StopEmulation();
	}
	@Override
	public void onPause()
	{
		super.onPause();
		if (Running)
			NativeLibrary.PauseEmulation();
	}
	@Override
	public void onResume()
	{
		super.onResume();
		if (Running)
			NativeLibrary.UnPauseEmulation();
	}

    /** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (savedInstanceState == null)
		{

			Intent ListIntent = new Intent(this, GameListActivity.class);
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
			WindowManager wm = (WindowManager) getApplicationContext().getSystemService(Context.WINDOW_SERVICE); // the results will be higher than using the activity context object or the getWindowManager() shortcut
			wm.getDefaultDisplay().getMetrics(displayMetrics);
			screenWidth = displayMetrics.widthPixels;
			screenHeight = displayMetrics.heightPixels;
			
			String FileName = data.getStringExtra("Select");
			GLview = new NativeGLSurfaceView(this);
			this.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
			String backend = NativeLibrary.GetConfig("Dolphin.ini", "Core", "GFXBackend", "OGL");
			if (backend.equals("OGL"))
				GLview.SetDimensions(screenHeight, screenWidth);
			else
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

		NativeLibrary.onTouchEvent(Action, ScreenX, ScreenY);
		
		return false;
	}

	// Gets button presses
	@Override
	public boolean dispatchKeyEvent(KeyEvent event) {
		int action = 0;
		switch (event.getAction()) {
			case KeyEvent.ACTION_DOWN:
				action = 0;
				break;
			case KeyEvent.ACTION_UP:
				action = 1;
				break;
			default:
				break;
		}
		InputDevice input = event.getDevice();
		NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
		return true;
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)) {
			return super.dispatchGenericMotionEvent(event);
		}

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();
		for (int a = 0; a < motions.size(); ++a)
		{
			InputDevice.MotionRange range;
			range = motions.get(a);
			NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), range.getAxis(), event.getAxisValue(range.getAxis()));
		}

		return true;
	}

}                                