/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation;

import org.dolphinemu.dolphinemu.NativeLibrary;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * The {@link SurfaceView} that rendering is performed on.
 */
public final class NativeGLSurfaceView extends SurfaceView
{
	private static Thread myRun;
	private static boolean Running = false;
	private static boolean Created = false;

	/**
	 * Constructor.
	 * 
	 * @param context The current {@link Context}.
	 * @param attribs An AttributeSet for retrieving data from XML files.
	 */
	public NativeGLSurfaceView(Context context, AttributeSet attribs)
	{
		super(context, attribs);

		if (!Created)
		{
			myRun = new Thread() 
			{
				@Override
				public void run() {

					NativeLibrary.Run(getHolder().getSurface());
					Created = false;
					Running = false;
				}
			};

			getHolder().addCallback(new SurfaceHolder.Callback()
			{
					public void surfaceCreated(SurfaceHolder holder)
					{
						// TODO Auto-generated method stub
						if (!Running)
						{
							myRun.start();
							Running = true;
						}
					}

					public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
					{
						// TODO Auto-generated method stub
					}

					public void surfaceDestroyed(SurfaceHolder holder)
					{
						// TODO Auto-generated method stub
					}
			 });

			Created = true;
		}
	}
}
