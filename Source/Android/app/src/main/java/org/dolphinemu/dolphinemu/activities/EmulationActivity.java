package org.dolphinemu.dolphinemu.activities;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.dialogs.RunningSettingDialog;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.ui.main.MainActivity;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.ControllerMappingHelper;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.Java_GCAdapter;
import org.dolphinemu.dolphinemu.utils.Java_WiimoteAdapter;

import java.io.File;
import java.util.List;

public final class EmulationActivity extends AppCompatActivity
{
	public static final int REQUEST_CHANGE_DISC = 1;

	private SensorManager mSensorManager;
	private View mDecorView;
	private EmulationFragment mEmulationFragment;

	private SharedPreferences mPreferences;
	private ControllerMappingHelper mControllerMappingHelper;

	private boolean mStopEmulation;
	private boolean mMenuVisible;

	private static boolean sIsGameCubeGame;

	private boolean activityRecreated;
	private String mSelectedTitle;
	private int mPlatform;
	private String mPath;
	private String mSavedState;

	public static final String RUMBLE_PREF_KEY = "phoneRumble";
	private Vibrator mVibrator;

	public static final String EXTRA_SELECTED_GAME = "SelectedGame";
	public static final String EXTRA_SELECTED_TITLE = "SelectedTitle";
	public static final String EXTRA_PLATFORM = "Platform";
	public static final String EXTRA_SAVED_STATE = "SavedState";

	public static void launch(FragmentActivity activity, GameFile gameFile, String savedState)
	{
		Intent launcher = new Intent(activity, EmulationActivity.class);

		launcher.putExtra(EXTRA_SELECTED_GAME, gameFile.getPath());
		launcher.putExtra(EXTRA_SELECTED_TITLE, gameFile.getTitle());
		launcher.putExtra(EXTRA_PLATFORM, gameFile.getPlatform());
		launcher.putExtra(EXTRA_SAVED_STATE, savedState);
		Bundle options = new Bundle();

		// I believe this warning is a bug. Activities are FragmentActivity from the support lib
		//noinspection RestrictedApi
		activity.startActivityForResult(launcher, MainPresenter.REQUEST_EMULATE_GAME, options);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		if (savedInstanceState == null)
		{
			// Get params we were passed
			Intent gameToEmulate = getIntent();
			mPath = gameToEmulate.getStringExtra(EXTRA_SELECTED_GAME);
			mSelectedTitle = gameToEmulate.getStringExtra(EXTRA_SELECTED_TITLE);
			mPlatform = gameToEmulate.getIntExtra(EXTRA_PLATFORM, 0);
			mSavedState = gameToEmulate.getStringExtra(EXTRA_SAVED_STATE);
			activityRecreated = false;
		}
		else
		{
			activityRecreated = true;
			restoreState(savedInstanceState);
		}

		// TODO: The accurate way to find out which console we're emulating is to
		// first launch emulation and then ask the core which console we're emulating
		sIsGameCubeGame = Platform.fromNativeInt(mPlatform) == Platform.GAMECUBE;
		mControllerMappingHelper = new ControllerMappingHelper();

		// Get a handle to the Window containing the UI.
		mDecorView = getWindow().getDecorView();
		mDecorView.setOnSystemUiVisibilityChangeListener(visibility ->
		{
			if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0)
			{
				// Go back to immersive fullscreen mode in 3s
				Handler handler = new Handler(getMainLooper());
				handler.postDelayed(this::enableFullscreenImmersive, 3000 /* 3s */);
			}
		});
		// Set these options now so that the SurfaceView the game renders into is the right size.
		mStopEmulation = false;
		enableFullscreenImmersive();

		setTheme(R.style.DolphinEmulationBase);

		Java_GCAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);
		Java_WiimoteAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);

		setContentView(R.layout.activity_emulation);

		// Find or create the EmulationFragment
		mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
			.findFragmentById(R.id.frame_emulation_fragment);
		if (mEmulationFragment == null)
		{
			mEmulationFragment = EmulationFragment.newInstance(mPath);
			getSupportFragmentManager().beginTransaction()
				.add(R.id.frame_emulation_fragment, mEmulationFragment)
				.commit();
		}

		setTitle(mSelectedTitle);

		mPreferences = PreferenceManager.getDefaultSharedPreferences(this);
		mSensorManager = (SensorManager)getSystemService(Context.SENSOR_SERVICE);
		setRumbeState(mPreferences.getBoolean(RUMBLE_PREF_KEY, true));
	}

	@Override
	protected void onSaveInstanceState(Bundle outState)
	{
		saveTemporaryState();
		outState.putString(EXTRA_SELECTED_GAME, mPath);
		outState.putString(EXTRA_SELECTED_TITLE, mSelectedTitle);
		outState.putInt(EXTRA_PLATFORM, mPlatform);
		outState.putString(EXTRA_SAVED_STATE, mSavedState);
		super.onSaveInstanceState(outState);
	}

	protected void restoreState(Bundle savedInstanceState)
	{
		mPath = savedInstanceState.getString(EXTRA_SELECTED_GAME);
		mSelectedTitle = savedInstanceState.getString(EXTRA_SELECTED_TITLE);
		mPlatform = savedInstanceState.getInt(EXTRA_PLATFORM);
		mSavedState = savedInstanceState.getString(EXTRA_SAVED_STATE);
	}

	@Override
	public void onBackPressed()
	{
		if (mMenuVisible)
		{
			mStopEmulation = true;
			mEmulationFragment.stopEmulation();
			finish();
		}
		else
		{
			disableFullscreenImmersive();
		}
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent result)
	{
		switch (requestCode)
		{
			case REQUEST_CHANGE_DISC:
				// If the user picked a file, as opposed to just backing out.
				if (resultCode == MainActivity.RESULT_OK)
				{
					String newDiscPath = FileBrowserHelper.getSelectedDirectory(result);
					if (!TextUtils.isEmpty(newDiscPath))
					{
						NativeLibrary.ChangeDisc(newDiscPath);
					}
				}
				break;
		}
	}

	private void enableFullscreenImmersive()
	{
		if (mStopEmulation)
		{
			return;
		}
		mMenuVisible = false;
		// It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
		mDecorView.setSystemUiVisibility(
			View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
				View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.SYSTEM_UI_FLAG_IMMERSIVE);
	}

	private void disableFullscreenImmersive()
	{
		mMenuVisible = true;
		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
			| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
			| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Inflate the menu; this adds items to the action bar if it is present.
		if (sIsGameCubeGame)
		{
			getMenuInflater().inflate(R.menu.menu_emulation, menu);
		}
		else
		{
			getMenuInflater().inflate(R.menu.menu_emulation_wii, menu);
		}
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		switch (item.getItemId())
		{
			// Edit the placement of the controls
			case R.id.menu_emulation_edit_layout:
				editControlsPlacement();
				break;

			case R.id.menu_emulation_joystick_settings:
				showJoystickSettings();
				break;

			case R.id.menu_emulation_sensor_settings:
				showSensorSettings();
				break;

			// Enable/Disable specific buttons or the entire input overlay.
			case R.id.menu_emulation_toggle_controls:
				toggleControls();
				break;

			// Adjust the scale of the overlay controls.
			case R.id.menu_emulation_adjust_scale:
				adjustScale();
				break;

			// (Wii games only) Change the controller for the input overlay.
			case R.id.menu_emulation_choose_controller:
				chooseController();
				break;

			/*case R.id.menu_refresh_wiimotes:
				NativeLibrary.RefreshWiimotes();
				break;*/

			// Screenshot capturing
			case R.id.menu_emulation_screenshot:
				NativeLibrary.SaveScreenShot();
				break;

			// Quick save / load
			case R.id.menu_quicksave:
				NativeLibrary.SaveState(9, false);
				break;

			case R.id.menu_quickload:
				NativeLibrary.LoadState(9);
				break;

			// Save state slots
			case R.id.menu_emulation_save_1:
				NativeLibrary.SaveState(0, false);
				break;

			case R.id.menu_emulation_save_2:
				NativeLibrary.SaveState(1, false);
				break;

			case R.id.menu_emulation_save_3:
				NativeLibrary.SaveState(2, false);
				break;

			case R.id.menu_emulation_save_4:
				NativeLibrary.SaveState(3, false);
				break;

			case R.id.menu_emulation_save_5:
				NativeLibrary.SaveState(4, false);
				break;

			// Load state slots
			case R.id.menu_emulation_load_1:
				NativeLibrary.LoadState(0);
				break;

			case R.id.menu_emulation_load_2:
				NativeLibrary.LoadState(1);
				break;

			case R.id.menu_emulation_load_3:
				NativeLibrary.LoadState(2);
				break;

			case R.id.menu_emulation_load_4:
				NativeLibrary.LoadState(3);
				break;

			case R.id.menu_emulation_load_5:
				NativeLibrary.LoadState(4);
				break;

			case R.id.menu_change_disc:
				FileBrowserHelper.openFilePicker(this, REQUEST_CHANGE_DISC);
				break;

			case R.id.menu_running_setting:
				RunningSettingDialog.newInstance()
					.show(getSupportFragmentManager(), "RunningSettingDialog");
				break;

			default:
				return false;
		}

		return true;
	}

	private void showJoystickSettings()
	{
		final SharedPreferences.Editor editor = mPreferences.edit();
		int joystick = mPreferences
			.getInt(InputOverlay.JOYSTICK_PREF_KEY, InputOverlay.JOYSTICK_RELATIVE_CENTER);
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_joystick_settings);

		builder.setSingleChoiceItems(R.array.joystickSettings, joystick,
			(dialog, indexSelected) ->
			{
				editor.putInt(InputOverlay.JOYSTICK_PREF_KEY, indexSelected);
			});

		builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
		{
			editor.apply();
			InputOverlay.JoyStickSetting = mPreferences.getInt(InputOverlay.JOYSTICK_PREF_KEY,
				InputOverlay.JOYSTICK_RELATIVE_CENTER);
			mEmulationFragment.refreshInputOverlay();
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void showSensorSettings()
	{
		final SharedPreferences.Editor editor = mPreferences.edit();
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_sensor_settings);

		if(sIsGameCubeGame)
		{
			int sensor = mPreferences
				.getInt(InputOverlay.SENSOR_GAMECUBE_KEY, InputOverlay.SENSOR_GC_JOYSTICK);

			builder.setSingleChoiceItems(R.array.gcSensorSettings, sensor,
				(dialog, indexSelected) ->
				{
					editor.putInt(InputOverlay.SENSOR_GAMECUBE_KEY, indexSelected);
				});

			builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
			{
				editor.apply();
				InputOverlay.SensorGCSetting = mPreferences.getInt(InputOverlay.SENSOR_GAMECUBE_KEY,
					InputOverlay.SENSOR_GC_JOYSTICK);
			});
		}
		else
		{
			int sensor = mPreferences
				.getInt(InputOverlay.SENSOR_WIIMOTE_KEY, InputOverlay.SENSOR_WII_DPAD);

			builder.setSingleChoiceItems(R.array.wiiSensorSettings, sensor,
				(dialog, indexSelected) ->
				{
					editor.putInt(InputOverlay.SENSOR_WIIMOTE_KEY, indexSelected);
				});

			builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
			{
				editor.apply();
				InputOverlay.SensorWiiSetting = mPreferences.getInt(InputOverlay.SENSOR_WIIMOTE_KEY,
					InputOverlay.SENSOR_WII_DPAD);
			});
		}

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void registerSensor()
	{
		if(mSensorManager == null)
		{
			mSensorManager = (SensorManager)getSystemService(Context.SENSOR_SERVICE);
			Sensor rotationVector = mSensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);
			if(rotationVector != null)
			{
				mSensorManager.registerListener(mEmulationFragment, rotationVector, SensorManager.SENSOR_DELAY_NORMAL, SensorManager.SENSOR_DELAY_UI);
			}
		}
	}

	private void resumeSensor()
	{
		if(mSensorManager != null)
		{
			Sensor rotationVector = mSensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);
			if(rotationVector != null)
			{
				mSensorManager.registerListener(mEmulationFragment, rotationVector, SensorManager.SENSOR_DELAY_NORMAL, SensorManager.SENSOR_DELAY_UI);
			}
		}
	}

	private void pauseSensor()
	{
		if(mSensorManager != null)
		{
				mSensorManager.unregisterListener(mEmulationFragment);
		}
	}

	private void editControlsPlacement()
	{
		if (mEmulationFragment.isConfiguringControls())
		{
			mEmulationFragment.stopConfiguringControls();
		}
		else
		{
			mEmulationFragment.startConfiguringControls();
		}
	}

	@Override
	protected void onResume()
	{
		super.onResume();
		resumeSensor();
	}

	@Override
	protected void onPause() {
		super.onPause();
		pauseSensor();
	}

	// Gets button presses
	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		if (mMenuVisible)
		{
			return super.dispatchKeyEvent(event);
		}

		int action;
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
		return NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
	}

	private void toggleControls()
	{
		final SharedPreferences.Editor editor = mPreferences.edit();
		boolean[] enabledButtons = new boolean[14];
		int controller = mPreferences
			.getInt(InputOverlay.CONTROL_TYPE_PREF_KEY, InputOverlay.CONTROLLER_WIINUNCHUK);
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_toggle_controls);

		if (sIsGameCubeGame || controller == InputOverlay.CONTROLLER_GAMECUBE)
		{
			for (int i = 0; i < enabledButtons.length; i++)
			{
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleGc" + i, true);
			}
			builder.setMultiChoiceItems(R.array.gcpadButtons, enabledButtons,
				(dialog, indexSelected, isChecked) -> editor
					.putBoolean("buttonToggleGc" + indexSelected, isChecked));
		}
		else if (controller == InputOverlay.COCONTROLLER_CLASSIC)
		{
			for (int i = 0; i < enabledButtons.length; i++)
			{
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleClassic" + i, true);
			}
			builder.setMultiChoiceItems(R.array.classicButtons, enabledButtons,
				(dialog, indexSelected, isChecked) -> editor
					.putBoolean("buttonToggleClassic" + indexSelected, isChecked));
		}
		else
		{
			for (int i = 0; i < enabledButtons.length; i++)
			{
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleWii" + i, true);
			}
			if (controller == InputOverlay.CONTROLLER_WIINUNCHUK)
			{
				builder.setMultiChoiceItems(R.array.nunchukButtons, enabledButtons,
					(dialog, indexSelected, isChecked) -> editor
						.putBoolean("buttonToggleWii" + indexSelected, isChecked));
			}
			else
			{
				builder.setMultiChoiceItems(R.array.wiimoteButtons, enabledButtons,
					(dialog, indexSelected, isChecked) -> editor
						.putBoolean("buttonToggleWii" + indexSelected, isChecked));
			}
		}

		builder.setNeutralButton(getString(R.string.emulation_toggle_all),
			(dialogInterface, i) -> mEmulationFragment.toggleInputOverlayVisibility());
		builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
		{
			editor.apply();
			mEmulationFragment.refreshInputOverlay();
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void adjustScale()
	{
		LayoutInflater inflater = LayoutInflater.from(this);
		View view = inflater.inflate(R.layout.dialog_seekbar, null);

		final SeekBar seekbar = (SeekBar) view.findViewById(R.id.seekbar);
		final TextView value = (TextView) view.findViewById(R.id.text_value);
		final TextView units = (TextView) view.findViewById(R.id.text_units);

		seekbar.setMax(150);
		seekbar.setProgress(mPreferences.getInt(InputOverlay.CONTROL_SCALE_PREF_KEY, 50));
		seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener()
		{
			public void onStartTrackingTouch(SeekBar seekBar)
			{
				// Do nothing
			}

			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser)
			{
				value.setText(String.valueOf(progress + 50));
			}

			public void onStopTrackingTouch(SeekBar seekBar)
			{
				// Do nothing
			}
		});

		value.setText(String.valueOf(seekbar.getProgress() + 50));
		units.setText("%");

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_control_scale);
		builder.setView(view);
		builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
		{
			SharedPreferences.Editor editor = mPreferences.edit();
			editor.putInt(InputOverlay.CONTROL_SCALE_PREF_KEY, seekbar.getProgress());
			editor.apply();
			mEmulationFragment.refreshInputOverlay();
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void chooseController()
	{
		final SharedPreferences.Editor editor = mPreferences.edit();
		int controller = mPreferences.getInt(InputOverlay.CONTROL_TYPE_PREF_KEY,
			InputOverlay.CONTROLLER_WIINUNCHUK);
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_choose_controller);
		builder.setSingleChoiceItems(R.array.controllersEntries, controller,
			(dialog, indexSelected) ->
			{
				editor.putInt(InputOverlay.CONTROL_TYPE_PREF_KEY, indexSelected);
				NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Extension",
					getResources().getStringArray(R.array.controllersValues)[indexSelected]);
			});
		builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
		{
			editor.apply();
			mEmulationFragment.refreshInputOverlay();
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		if (mMenuVisible)
		{
			return false;
		}

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
			int axis = range.getAxis();
			float origValue = event.getAxisValue(axis);
			float value = mControllerMappingHelper.scaleAxis(input, axis, origValue);
			// If the input is still in the "flat" area, that means it's really zero.
			// This is used to compensate for imprecision in joysticks.
			if (Math.abs(value) > range.getFlat())
			{
				NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, value);
			}
			else
			{
				NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, 0.0f);
			}
		}

		return true;
	}

	public void setRumbeState(boolean rumble)
	{
		if(rumble)
		{
			if(mVibrator == null)
			{
				mVibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
				if (mVibrator != null && !mVibrator.hasVibrator())
				{
					mVibrator = null;
				}
			}
		}
		else
		{
			mVibrator = null;
		}
	}

	public void rumble(int padID, double state)
	{
		if (mVibrator != null)
		{
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
			{
				mVibrator.vibrate(VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE));
			}
			else
			{
				mVibrator.vibrate(100);
			}
		}
	}

	public static boolean isGameCubeGame()
	{
		return sIsGameCubeGame;
	}

	public boolean isActivityRecreated()
	{
		return activityRecreated;
	}

	public String getSavedState()
	{
		return mSavedState;
	}

	public void saveTemporaryState()
	{
		mSavedState = getFilesDir() + File.separator + "temp.sav";
		NativeLibrary.SaveStateAs(mSavedState, true);
	}
}
