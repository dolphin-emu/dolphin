package org.dolphinemu.dolphinemu;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.MotionEvent;
import android.view.WindowManager;

public class DolphinEmulator<MainActivity> extends Activity {
	
	static private NativeGLSurfaceView GLview = null;
	static private NativeRenderer Renderer = null;
	static private boolean Running = false;
	
	private float screenWidth;
	private float screenHeight;
	
	public static native void SetKey(int Value, int Key);
	
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
	@Override
	public void onStop()
	{
		super.onStop();
		if (Running)
			Renderer.StopEmulation();
	}
	@Override
	public void onPause()
	{
		super.onPause();
		if (Running)
			Renderer.PauseEmulation();
	}
	@Override
	public void onResume()
	{
		super.onResume();
		if (Running)
			Renderer.UnPauseEmulation();
	}

    /** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		if (savedInstanceState == null)
		{
			Intent ListIntent = new Intent(this, NativeListView.class);
			startActivityForResult(ListIntent, 1);
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
			Renderer = new NativeRenderer();
			Renderer.setContext(getApplicationContext());
			
			GLview = new NativeGLSurfaceView(this);
			GLview.setEGLContextClientVersion(2);
			GLview.setRenderer(Renderer);
			
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
		
		int Button = Renderer.ButtonPressed(Action, ((X / screenWidth) * 2.0f) - 1.0f, ((Y / screenHeight) * 2.0f) - 1.0f);
		
		if (Button != -1)
			SetKey(Action, Button);
		
		return false;
	}
	
	public boolean overrideKeys()
	{   
		return false;
	}  

}                                