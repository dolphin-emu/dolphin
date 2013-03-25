package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class NativeGLSurfaceView extends SurfaceView {
	static private String FileName;
	static private Thread myRun;
	static private boolean Running = false;
	static private boolean Created = false;
	static private float width;
	static private float height;
	
	public static native void main(String File, Surface surf, int width, int height);
	
	public static native void UnPauseEmulation();
	public static native void PauseEmulation();
	public static native void StopEmulation();
	
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
	

	public NativeGLSurfaceView(Context context) {
		super(context);
		if (!Created)
		{
			myRun = new Thread() 
			{
				@Override
	      		public void run() {
	    	  		main(FileName, getHolder().getSurface(), (int)width, (int)height);
	      		}	
			};
			getHolder().addCallback(new SurfaceHolder.Callback() {


		            public void surfaceCreated(SurfaceHolder holder) {
		                // TODO Auto-generated method stub
		            	myRun.start();
		            }

					public void surfaceChanged(SurfaceHolder arg0, int arg1,
							int arg2, int arg3) {
						// TODO Auto-generated method stub
						
					}

					public void surfaceDestroyed(SurfaceHolder arg0) {
						// TODO Auto-generated method stub
						
					}
			 });
			Created = true;
		}
	}
	
	public void SetFileName(String file)
	{
		FileName = file;
	}
	public void SetDimensions(float screenWidth, float screenHeight)
	{
		width = screenWidth;
		height = screenHeight;
	}
	

}
