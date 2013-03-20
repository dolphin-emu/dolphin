package org.dolphinemu.dolphinemu;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

public class NativeGLSurfaceView extends GLSurfaceView {
	static private String FileName;
	static private Thread myRun;
	static private boolean Running = false;
	static private boolean Created = false;
	
	public static native void main(String File, Surface surf);
	
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
	    	  		main(FileName, getHolder().getSurface());
	      		}	
			};
			Created = true;
		}
	}
	
	public void surfaceCreated(SurfaceHolder holder)
	{
		super.surfaceCreated(holder);
		if (!Running)
		{
			myRun.start();
			Running = true;
		}
	}
	
	public void SetFileName(String file)
	{
		FileName = file;
	}
	

}
