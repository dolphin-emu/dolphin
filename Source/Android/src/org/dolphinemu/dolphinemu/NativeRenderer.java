package org.dolphinemu.dolphinemu;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.util.Log;

public class NativeRenderer implements GLSurfaceView.Renderer {
	// Button Manager
	private static ButtonManager Buttons;
	
	private static boolean Running = false;
	
	// Context
	private static Context _gContext;
	
	// Native
	public static native void DrawME();
	public static native void DrawButton(int GLTex, int ID);
	public static native void SetButtonCoords(float[] Coords);
	public static native void PrepareME();
	public static native void UnPauseEmulation();
	public static native void PauseEmulation();
	public static native void StopEmulation();
	
	// Texture loading
	private static int buttonA = -1;
	private static int buttonB = -1;
	
	// Get a new texture id:
	private static int newTextureID(GL10 gl) 
	{
	    int[] temp = new int[1];
	    gl.glGenTextures(1, temp, 0);
	    return temp[0];        
	}

	// Will load a texture out of a drawable resource file, and return an OpenGL texture ID:
	private int loadTexture(GL10 gl, String resource) 
	{    
	    // In which ID will we be storing this texture?
	    int id = newTextureID(gl);

	    // Load up, and flip the texture:
	    InputStream File = null;
		try {
			File = _gContext.getAssets().open(resource);
		} 
		catch (IOException e) 
		{
			// TODO Auto-generated catch block
			e.printStackTrace();
			return 0;
		}
		
	    Bitmap bmp = BitmapFactory.decodeStream(File);
	    
	    gl.glBindTexture(GL10.GL_TEXTURE_2D, id);
	    
	    // Set all of our texture parameters:
	    gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_LINEAR_MIPMAP_LINEAR);
	    gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR_MIPMAP_LINEAR);
	    gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, GL10.GL_REPEAT);
	    gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, GL10.GL_REPEAT);
	    
	    // Generate, and load up all of the mipmaps:
	    for(int level=0, height = bmp.getHeight(), width = bmp.getWidth(); true; level++) 
	    {
	        // Push the bitmap onto the GPU:
	        GLUtils.texImage2D(GL10.GL_TEXTURE_2D, level, bmp, 0);
	        
	        // We need to stop when the texture is 1x1:
	        if(height==1 && width==1) break;
	        
	        // Resize, and let's go again:
	        width >>= 1; height >>= 1;
	        if(width<1)  width = 1;
	        if(height<1) height = 1;
	        
	        Bitmap bmp2 = Bitmap.createScaledBitmap(bmp, width, height, true);
	        bmp.recycle();
	        bmp = bmp2;
	    }
	    bmp.recycle();
	    
	    return id;
	}
	
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
		if (!Running)
			Buttons = new ButtonManager();
	}
	
	public void onSurfaceChanged(GL10 gl, int width, int height) 
	{
		gl.glViewport(0, 0, width, height);
	}

	public void onSurfaceCreated(GL10 gl, EGLConfig config) 
	{	
		if (!Running)
		{
			gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			gl.glEnable(GL10.GL_BLEND);
		    gl.glBlendFunc(GL10.GL_ONE, GL10.GL_ONE_MINUS_SRC_ALPHA);
			
			buttonA = loadTexture(gl, "ButtonA.png");
			buttonB = loadTexture(gl, "ButtonB.png");
			SetButtonCoords(Flatten(Buttons.GetButtonCoords()));
		    PrepareME();
		    Running = true;
		}
	}
	public static float[] Flatten(float[][] data) {
		
	    float[] list = new float[data.length * data[0].length];
	    
	    for(int y = 0; y < data.length; ++y)
	    	for (int x = 0; x < data[y].length; ++x)
	            list[y * data[0].length + x] = data[y][x];
	    return list;
	}
	public void onDrawFrame(GL10 gl) 
	{
		if (Running)
		{
			gl.glClear(GL10.GL_COLOR_BUFFER_BIT);
			DrawME();
			
			// -1 is left
			// -1 is bottom
			
			DrawButton(buttonA, 0);
			DrawButton(buttonB, 1);
		}
	}
	public void setContext(Context ctx)
	{
		_gContext = ctx;
	}
	public int ButtonPressed(int action, float x, float y)
	{
		return Buttons.ButtonPressed(action, x, y);
	}
}
