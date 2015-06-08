package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.settings.input.InputConfigFragment;

import java.util.List;

public final class EmulationActivity extends Activity
{
	private View mDecorView;

	private boolean mDeviceHasTouchScreen;
	private boolean mSystemUiVisible;

	/**
	 * Handlers are a way to pass a message to an Activity telling it to do something
	 * on the UI thread. This Handler responds to any message, even blank ones, by
	 * hiding the system UI.
	 */
	private Handler mSystemUiHider = new Handler()
	{
		@Override
		public void handleMessage(Message msg)
		{
			hideSystemUI();
		}
	};

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		mDeviceHasTouchScreen = getPackageManager().hasSystemFeature("android.hardware.touchscreen");

		// Get a handle to the Window containing the UI.
		mDecorView = getWindow().getDecorView();

		// Set these options now so that the SurfaceView the game renders into is the right size.
		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

		// Set the ActionBar to follow the navigation/status bar's visibility changes.
		mDecorView.setOnSystemUiVisibilityChangeListener(
				new View.OnSystemUiVisibilityChangeListener()
				{
					@Override
					public void onSystemUiVisibilityChange(int flags)
					{
						mSystemUiVisible = (flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0;

						if (mSystemUiVisible)
						{
							getActionBar().show();
							hideSystemUiAfterDelay();
						}
						else
						{
							getActionBar().hide();
						}
					}
				}
		);

		setContentView(R.layout.activity_emulation);

		Intent gameToEmulate = getIntent();
		String path = gameToEmulate.getStringExtra("SelectedGame");
		String title = gameToEmulate.getStringExtra("SelectedTitle");

		setTitle(title);

		// Instantiate an EmulationFragment.
		EmulationFragment emulationFragment = EmulationFragment.newInstance(path);

		// Add fragment to the activity - this triggers all its lifecycle callbacks.
		getFragmentManager().beginTransaction()
				.add(R.id.frame_content, emulationFragment, EmulationFragment.FRAGMENT_TAG)
				.commit();
	}

	@Override
	protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);

		// Give the user a few seconds to see what the controls look like, then hide them.
		hideSystemUiAfterDelay();
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);

		if (hasFocus)
		{
			hideSystemUiAfterDelay();
		}
		else
		{
			// If the window loses focus (i.e. a dialog box, or a popup menu is on screen
			// stop hiding the UI.
			mSystemUiHider.removeMessages(0);
		}
	}

	@Override
	public void onBackPressed()
	{
		if (!mDeviceHasTouchScreen && !mSystemUiVisible)
		{
			showSystemUI();
		}
		else
		{
			// Let the system handle it; i.e. quit the activity TODO or show "are you sure?" dialog.
			super.onBackPressed();
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.menu_emulation, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		switch (item.getItemId())
		{
			// Enable/Disable input overlay.
			case R.id.enableInputOverlay:
			{
				EmulationFragment emulationFragment = (EmulationFragment) getFragmentManager()
						.findFragmentByTag(EmulationFragment.FRAGMENT_TAG);

				emulationFragment.toggleInputOverlayVisibility();

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
				builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener()
				{
					public void onClick(DialogInterface dialog, int which)
					{
						onDestroy();
					}
				});
				builder.show();
				return true;
			}

			default:
				return super.onOptionsItemSelected(item);
		}
	}

	// Gets button presses
	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		int action = 0;

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
		boolean handled = NativeLibrary.onGamePadEvent(InputConfigFragment.getInputDesc(input), event.getKeyCode(), action);
		return handled;
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0))
		{
			return super.dispatchGenericMotionEvent(event);
		}

		// Don't attempt to do anything if we are disconnecting a device.
		if (event.getActionMasked() == MotionEvent.ACTION_CANCEL)
			return true;

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();

		for (InputDevice.MotionRange range : motions)
		{
			NativeLibrary.onGamePadMoveEvent(InputConfigFragment.getInputDesc(input), range.getAxis(), event.getAxisValue(range.getAxis()));
		}

		return true;
	}

	private void hideSystemUiAfterDelay()
	{
		// Clear any pending hide events.
		mSystemUiHider.removeMessages(0);

		// Add a new hide event, to occur 3 seconds from now.
		mSystemUiHider.sendEmptyMessageDelayed(0, 3000);
	}

	private void hideSystemUI()
	{
		mSystemUiVisible = false;

		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
				View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.SYSTEM_UI_FLAG_IMMERSIVE);
	}

	private void showSystemUI()
	{
		mSystemUiVisible = true;

		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

		hideSystemUiAfterDelay();
	}
}
