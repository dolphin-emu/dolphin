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
import org.dolphinemu.dolphinemu.settings.input.InputConfigFragment;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.util.List;

import javax.microedition.khronos.opengles.GL10;

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
	private SharedPreferences sharedPrefs;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Retrieve screen dimensions.
		DisplayMetrics dm = getResources().getDisplayMetrics();
		this.screenHeight = dm.heightPixels;
		this.screenWidth = dm.widthPixels;

		// Request window features for the emulation view.
		getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);
		getWindow().addFlags(LayoutParams.FLAG_FULLSCREEN);
		getWindow().requestFeature(Window.FEATURE_ACTION_BAR_OVERLAY);

		// Set the transparency for the action bar.
		ColorDrawable actionBarBackground = new ColorDrawable(Color.parseColor("#303030"));
		actionBarBackground.setAlpha(175);
		getActionBar().setBackgroundDrawable(actionBarBackground);

		// Set the native rendering screen width/height.
		//
		// Due to a bug in Adreno, it renders the screen rotated 90 degrees when using OpenGL
		// Flip the width and height when on Adreno to work around this.
		// This bug is fixed in Qualcomm driver v53
		// Mali isn't affected by this bug.
		sharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		if (hasBuggedDriverDimensions())
			NativeLibrary.SetDimensions((int)screenHeight, (int)screenWidth);
		else
			NativeLibrary.SetDimensions((int)screenWidth, (int)screenHeight);

		// Get the intent passed from the GameList when the game
		// was selected. This is so the path of the game can be retrieved
		// and set on the native side of the code so the emulator can actually
		// load the game.
		Intent gameToEmulate = getIntent();
		NativeLibrary.SetFilename(gameToEmulate.getStringExtra("SelectedGame"));
		Running = true;

		// Set the emulation window.
		setContentView(R.layout.emulation_view);

		// If the input overlay was previously disabled, then don't show it.
		if (!sharedPrefs.getBoolean("showInputOverlay", true))
		{
			findViewById(R.id.emulationControlOverlay).setVisibility(View.INVISIBLE);
		}

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
	public boolean onPrepareOptionsMenu(Menu menu)
	{
		// Determine which string the "Enable Input Overlay" menu item should have
		// depending on its visibility at the time of preparing the options menu.
		if (!sharedPrefs.getBoolean("showInputOverlay", true))
		{
			menu.findItem(R.id.enableInputOverlay).setTitle(R.string.enable_input_overlay);
		}
		else
		{
			menu.findItem(R.id.enableInputOverlay).setTitle(R.string.disable_input_overlay);
		}

		return true;
	}

	@Override
	public boolean onMenuItemSelected(int itemId, MenuItem item)
	{
		switch(item.getItemId())
		{
			// Enable/Disable input overlay.
			case R.id.enableInputOverlay:
			{
				View overlay = findViewById(R.id.emulationControlOverlay);

				// Show the overlay
				if (item.getTitle().equals(getString(R.string.enable_input_overlay)))
				{
					overlay.setVisibility(View.VISIBLE);
					item.setTitle(R.string.disable_input_overlay);
					sharedPrefs.edit().putBoolean("showInputOverlay", true).commit();
				}
				else // Hide the overlay
				{
					overlay.setVisibility(View.INVISIBLE);
					item.setTitle(R.string.enable_input_overlay);
					sharedPrefs.edit().putBoolean("showInputOverlay", false).commit();
				}

				return true;
			}

			// Screenshot capturing
			case R.id.takeScreenshot:
				NativeLibrary.SaveScreenShot();
				return true;
				
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

			// Load state slots
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
				builder.setTitle(R.string.overlay_exit_emulation);
				builder.setMessage(R.string.overlay_exit_emulation_confirm);
				builder.setNegativeButton(R.string.no, null);
				builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which)
					{
						onDestroy();
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
					action = NativeLibrary.ButtonState.PRESSED;
					break;
				case KeyEvent.ACTION_UP:
					action = NativeLibrary.ButtonState.RELEASED;
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

	// For handling bugged driver dimensions (applies mainly to Qualcomm devices)
	private boolean hasBuggedDriverDimensions()
	{
		final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);
		final String vendor = eglHelper.getGL().glGetString(GL10.GL_VENDOR);
		final String version = eglHelper.getGL().glGetString(GL10.GL_VERSION);
		final String renderer = eglHelper.getGL().glGetString(GL10.GL_RENDERER);

		if (sharedPrefs.getString("gpuPref", "Software Rendering").equals("OGL")
				&& eglHelper.supportsGLES3()
				&& vendor.equals("Qualcomm")
				&& renderer.equals("Adreno (TM) 3"))
		{
			final int start = version.indexOf("V@") + 2;
			final StringBuilder versionBuilder = new StringBuilder();
			
			for (int i = start; i < version.length(); i++)
			{
				char c = version.charAt(i);

				// End of numeric portion of version string.
				if (c == ' ')
					break;

				versionBuilder.append(c);
			}

			if (Float.parseFloat(versionBuilder.toString()) < 53.0f)
				return true;
		}
		

		return false;
	}
}
