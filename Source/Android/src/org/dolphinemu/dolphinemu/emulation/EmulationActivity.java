/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.emulation;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.DisplayMetrics;
import android.view.*;
import android.view.WindowManager.LayoutParams;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.settings.InputConfigFragment;
import org.dolphinemu.dolphinemu.settings.VideoSettingsFragment;

import java.util.List;

/**
 * This is the activity where all of the emulation handling happens.
 * This activity is responsible for displaying the SurfaceView that we render to.
 */
public final class EmulationActivity extends Activity
{
	private boolean Running;
	private boolean IsActionBarHidden = false;
	private float screenWidth;
	private float screenHeight;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Retrieve screen dimensions.
		DisplayMetrics displayMetrics = new DisplayMetrics();
		WindowManager wm = getWindowManager();
		wm.getDefaultDisplay().getMetrics(displayMetrics);
		this.screenHeight = displayMetrics.heightPixels;
		this.screenWidth = displayMetrics.widthPixels;

		// Request window features for the emulation view.
		getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);
		getWindow().addFlags(LayoutParams.FLAG_FULLSCREEN);
		getWindow().requestFeature(Window.FEATURE_ACTION_BAR_OVERLAY);

		// Set the transparency for the action bar.
		ColorDrawable actionBarBackground = new ColorDrawable(Color.parseColor("#303030"));
		actionBarBackground.setAlpha(175);
		getActionBar().setBackgroundDrawable(actionBarBackground);

		// Set the native rendering screen width/height.
		// Also get the intent passed from the GameList when the game
		// was selected. This is so the path of the game can be retrieved
		// and set on the native side of the code so the emulator can actually
		// load the game.
		Intent gameToEmulate = getIntent();

		// Due to a bug in Adreno, it renders the screen rotated 90 degrees when using OpenGL
		// Flip the width and height when on Adreno to work around this.
		// Mali isn't affected by this bug.
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		if (prefs.getString("gpuPref", "Software Rendering").equals("OGL")
				&& VideoSettingsFragment.SupportsGLES3()
				&& VideoSettingsFragment.m_GLVendor != null
				&& VideoSettingsFragment.m_GLVendor.equals("Qualcomm"))
			NativeLibrary.SetDimensions((int)screenHeight, (int)screenWidth);
		else
			NativeLibrary.SetDimensions((int)screenWidth, (int)screenHeight);
		NativeLibrary.SetFilename(gameToEmulate.getStringExtra("SelectedGame"));
		Running = true;

		// Set the emulation window.
		setContentView(R.layout.emulation_view);

		// Hide the action bar by default so it doesn't get in the way.
		getActionBar().hide();
		IsActionBarHidden = true;
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
	
	@Override
	public void onDestroy()
	{
		super.onDestroy();

		if (Running)
		{
			NativeLibrary.StopEmulation();
			Running = false;
		}
	}
	
	@Override
	public boolean onTouchEvent(MotionEvent event)
	{
		float X = event.getX();
		float Y = event.getY();
		int Action = event.getActionMasked();

		// Converts button locations 0 - 1 to OGL screen coords -1.0 - 1.0
		float ScreenX = ((X / screenWidth) * 2.0f) - 1.0f;
		float ScreenY = ((Y / screenHeight) * -2.0f) + 1.0f;

		NativeLibrary.onTouchEvent(Action, ScreenX, ScreenY);
		
		return false;
	}
	
	@Override
	public void onBackPressed()
	{
		// The back button in the emulation
		// window is the toggle for the action bar.
		if (IsActionBarHidden)
		{
			IsActionBarHidden = false;
			getActionBar().show();
		}
		else
		{
			IsActionBarHidden = true;
			getActionBar().hide();
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.emuwindow_overlay, menu);
		return true;
	}

	@Override
	public boolean onMenuItemSelected(int itemId, MenuItem item)
	{
		switch(item.getItemId())
		{
			// Save state slots
			case R.id.saveSlot1:
				NativeLibrary.SaveState(0);
				return true;

			case R.id.saveSlot2:
				NativeLibrary.SaveState(1);
				return true;

			case R.id.saveSlot3:
				NativeLibrary.SaveState(2);
				return true;

			case R.id.saveSlot4:
				NativeLibrary.SaveState(3);
				return true;

			case R.id.saveSlot5:
				NativeLibrary.SaveState(4);
				return true;

			// Load state slot
			case R.id.loadSlot1:
				NativeLibrary.LoadState(0);
				return true;

			case R.id.loadSlot2:
				NativeLibrary.LoadState(1);
				return true;

			case R.id.loadSlot3:
				NativeLibrary.LoadState(2);
				return true;

			case R.id.loadSlot4:
				NativeLibrary.LoadState(3);
				return true;

			case R.id.loadSlot5:
				NativeLibrary.LoadState(4);
				return true;

			case R.id.exitEmulation:
			{
				// Create a confirmation method for quitting the current emulation instance.
				AlertDialog.Builder builder = new AlertDialog.Builder(this);
				builder.setTitle(getString(R.string.overlay_exit_emulation));
				builder.setMessage(R.string.overlay_exit_emulation_confirm);
				builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which)
					{
						finish();
					}
				});
				builder.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which)
					{
						// Do nothing. Just makes the No button appear.
					}
				});
				builder.show();
				return true;
			}

			default:
				return super.onMenuItemSelected(itemId, item);
		}
	}

	// Gets button presses
	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		int action = 0;

		if (Running)
		{
			switch (event.getAction())
			{
				case KeyEvent.ACTION_DOWN:
					// Handling the case where the back button is pressed.
					if (event.getKeyCode() == KeyEvent.KEYCODE_BACK)
					{
						onBackPressed();
						return true;
					}

					// Normal key events.
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
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0) || !Running)
		{
			return super.dispatchGenericMotionEvent(event);
		}

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();

		for (InputDevice.MotionRange range : motions)
		{
			NativeLibrary.onGamePadMoveEvent(InputConfigFragment.getInputDesc(input), range.getAxis(), event.getAxisValue(range.getAxis()));
		}

		return true;
	}
}
