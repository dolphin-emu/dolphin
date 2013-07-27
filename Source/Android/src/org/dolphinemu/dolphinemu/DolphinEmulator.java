package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
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
			String BaseDir = Environment.getExternalStorageDirectory()+File.separator+"dolphin-emu";
			File directory = new File(BaseDir);
			directory.mkdirs();
			
			String ConfigDir = BaseDir + File.separator + "Config";
			directory = new File(ConfigDir);
			directory.mkdirs();

			String GCDir = BaseDir + File.separator + "GC";
			directory = new File(GCDir);
			directory.mkdirs();

			// Copy assets if needed
			java.io.File file = new java.io.File(
					Environment.getExternalStorageDirectory()+File.separator+
							"dolphin-emu" + File.separator + "GC" + File.separator + "dsp_coef.bin");
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
						"dolphin-emu" + File.separator + "Config" + File.separator + "GCPadNew.ini");
				CopyAsset("Dolphin.ini",
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "Config" + File.separator + "Dolphin.ini");
				CopyAsset("dsp_coef.bin",
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "GC" + File.separator + "dsp_coef.bin");
				CopyAsset("dsp_rom.bin",
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "GC" + File.separator + "dsp_rom.bin");
				CopyAsset("font_ansi.bin",
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "GC" + File.separator + "font_ansi.bin");
				CopyAsset("font_sjis.bin",
						Environment.getExternalStorageDirectory()+File.separator+
						"dolphin-emu" + File.separator + "GC" + File.separator + "font_sjis.bin");

				SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
				SharedPreferences.Editor editor = prefs.edit();
				editor.putString("cpupref", NativeLibrary.GetConfig("Dolphin.ini", "Core", "CPUCore", "3"));
				editor.putBoolean("dualcorepref", NativeLibrary.GetConfig("Dolphin.ini", "Core", "CPUThread", "False").equals("True") ? true : false);
				editor.putString("gpupref", NativeLibrary.GetConfig("Dolphin.ini", "Core", "GFXBackend ", "Software Renderer"));
				editor.commit();
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
			String backend = NativeLibrary.GetConfig("Dolphin.ini", "Core", "GFXBackend", "Software Renderer");
			NativeLibrary.SetDimensions((int)screenWidth, (int)screenHeight);
			NativeLibrary.SetFilename(FileName);
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

		// Special catch for the back key
		// Currently disabled because stopping and starting emulation is broken.
		/*
		if (    event.getSource() == InputDevice.SOURCE_KEYBOARD
				&& event.getKeyCode() == KeyEvent.KEYCODE_BACK
				&& event.getAction() == KeyEvent.ACTION_UP
				)
		{
			if (Running)
				NativeLibrary.StopEmulation();
			Running = false;
			Intent ListIntent = new Intent(this, GameListActivity.class);
			startActivityForResult(ListIntent, 1);
			return true;
		}
		*/

		if (Running)
		{
			switch (event.getAction()) {
				case KeyEvent.ACTION_DOWN:
					action = 0;
					break;
				case KeyEvent.ACTION_UP:
					action = 1;
					break;
				default:
					return false;
			}
			InputDevice input = event.getDevice();
			NativeLibrary.onGamePadEvent(InputConfigFragment.getInputDesc(input), event.getKeyCode(), action);
			return true;
		}
		return false;
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0) || !Running) {
			return super.dispatchGenericMotionEvent(event);
		}

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();
		for (int a = 0; a < motions.size(); ++a)
		{
			InputDevice.MotionRange range;
			range = motions.get(a);
			NativeLibrary.onGamePadMoveEvent(InputConfigFragment.getInputDesc(input), range.getAxis(), event.getAxisValue(range.getAxis()));
		}

		return true;
	}

}                                