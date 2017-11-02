package org.dolphinemu.dolphinemu.activities;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.support.annotation.IntDef;
import android.support.v4.app.ActivityOptionsCompat;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;
import android.util.SparseIntArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.fragments.MenuFragment;
import org.dolphinemu.dolphinemu.fragments.SaveLoadStateFragment;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.Animations;
import org.dolphinemu.dolphinemu.utils.ControllerMappingHelper;
import org.dolphinemu.dolphinemu.utils.Java_GCAdapter;
import org.dolphinemu.dolphinemu.utils.Java_WiimoteAdapter;
import org.dolphinemu.dolphinemu.utils.Log;

import java.lang.annotation.Retention;
import java.util.List;

import static java.lang.annotation.RetentionPolicy.SOURCE;

public final class EmulationActivity extends AppCompatActivity
{
	private static final String BACKSTACK_NAME_MENU = "menu";
	private static final String BACKSTACK_NAME_SUBMENU = "submenu";
	private View mDecorView;
	private ImageView mImageView;
	private EmulationFragment mEmulationFragment;

	private SharedPreferences mPreferences;
	private ControllerMappingHelper mControllerMappingHelper;

	// So that MainActivity knows which view to invalidate before the return animation.
	private int mPosition;

	private boolean mDeviceHasTouchScreen;
	private boolean mMenuVisible;

	private static boolean sIsGameCubeGame;

	private String mScreenPath;
	private String mSelectedTitle;

	@Retention(SOURCE)
	@IntDef({MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
			MENU_ACTION_CHOOSE_CONTROLLER, MENU_ACTION_REFRESH_WIIMOTES, MENU_ACTION_TAKE_SCREENSHOT,
			MENU_ACTION_QUICK_SAVE, MENU_ACTION_QUICK_LOAD, MENU_ACTION_SAVE_ROOT,
			MENU_ACTION_LOAD_ROOT, MENU_ACTION_SAVE_SLOT1, MENU_ACTION_SAVE_SLOT2,
			MENU_ACTION_SAVE_SLOT3, MENU_ACTION_SAVE_SLOT4, MENU_ACTION_SAVE_SLOT5,
			MENU_ACTION_SAVE_SLOT6, MENU_ACTION_LOAD_SLOT1, MENU_ACTION_LOAD_SLOT2,
			MENU_ACTION_LOAD_SLOT3, MENU_ACTION_LOAD_SLOT4, MENU_ACTION_LOAD_SLOT5,
			MENU_ACTION_LOAD_SLOT6, MENU_ACTION_EXIT})
	public @interface MenuAction {
	}

	public static final int MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0;
	public static final int MENU_ACTION_TOGGLE_CONTROLS = 1;
	public static final int MENU_ACTION_ADJUST_SCALE = 2;
	public static final int MENU_ACTION_CHOOSE_CONTROLLER = 3;
	public static final int MENU_ACTION_REFRESH_WIIMOTES = 4;
	public static final int MENU_ACTION_TAKE_SCREENSHOT = 5;
	public static final int MENU_ACTION_QUICK_SAVE = 6;
	public static final int MENU_ACTION_QUICK_LOAD = 7;
	public static final int MENU_ACTION_SAVE_ROOT = 8;
	public static final int MENU_ACTION_LOAD_ROOT = 9;
	public static final int MENU_ACTION_SAVE_SLOT1 = 10;
	public static final int MENU_ACTION_SAVE_SLOT2 = 11;
	public static final int MENU_ACTION_SAVE_SLOT3 = 12;
	public static final int MENU_ACTION_SAVE_SLOT4 = 13;
	public static final int MENU_ACTION_SAVE_SLOT5 = 14;
	public static final int MENU_ACTION_SAVE_SLOT6 = 15;
	public static final int MENU_ACTION_LOAD_SLOT1 = 16;
	public static final int MENU_ACTION_LOAD_SLOT2 = 17;
	public static final int MENU_ACTION_LOAD_SLOT3 = 18;
	public static final int MENU_ACTION_LOAD_SLOT4 = 19;
	public static final int MENU_ACTION_LOAD_SLOT5 = 20;
	public static final int MENU_ACTION_LOAD_SLOT6 = 21;
	public static final int MENU_ACTION_EXIT = 22;


	private static SparseIntArray buttonsActionsMap = new SparseIntArray();
	static {
		buttonsActionsMap.append(R.id.menu_emulation_edit_layout, EmulationActivity.MENU_ACTION_EDIT_CONTROLS_PLACEMENT);
		buttonsActionsMap.append(R.id.menu_emulation_toggle_controls, EmulationActivity.MENU_ACTION_TOGGLE_CONTROLS);
		buttonsActionsMap.append(R.id.menu_emulation_adjust_scale, EmulationActivity.MENU_ACTION_ADJUST_SCALE);
		buttonsActionsMap.append(R.id.menu_emulation_choose_controller, EmulationActivity.MENU_ACTION_CHOOSE_CONTROLLER);
		buttonsActionsMap.append(R.id.menu_refresh_wiimotes, EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES);
		buttonsActionsMap.append(R.id.menu_emulation_screenshot, EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT);

		buttonsActionsMap.append(R.id.menu_quicksave, EmulationActivity.MENU_ACTION_QUICK_SAVE);
		buttonsActionsMap.append(R.id.menu_quickload, EmulationActivity.MENU_ACTION_QUICK_LOAD);
		buttonsActionsMap.append(R.id.menu_emulation_save_root, EmulationActivity.MENU_ACTION_SAVE_ROOT);
		buttonsActionsMap.append(R.id.menu_emulation_load_root, EmulationActivity.MENU_ACTION_LOAD_ROOT);
		buttonsActionsMap.append(R.id.menu_emulation_save_1, EmulationActivity.MENU_ACTION_SAVE_SLOT1);
		buttonsActionsMap.append(R.id.menu_emulation_save_2, EmulationActivity.MENU_ACTION_SAVE_SLOT2);
		buttonsActionsMap.append(R.id.menu_emulation_save_3, EmulationActivity.MENU_ACTION_SAVE_SLOT3);
		buttonsActionsMap.append(R.id.menu_emulation_save_4, EmulationActivity.MENU_ACTION_SAVE_SLOT4);
		buttonsActionsMap.append(R.id.menu_emulation_save_5, EmulationActivity.MENU_ACTION_SAVE_SLOT5);
		buttonsActionsMap.append(R.id.menu_emulation_load_1, EmulationActivity.MENU_ACTION_LOAD_SLOT1);
		buttonsActionsMap.append(R.id.menu_emulation_load_2, EmulationActivity.MENU_ACTION_LOAD_SLOT2);
		buttonsActionsMap.append(R.id.menu_emulation_load_3, EmulationActivity.MENU_ACTION_LOAD_SLOT3);
		buttonsActionsMap.append(R.id.menu_emulation_load_4, EmulationActivity.MENU_ACTION_LOAD_SLOT4);
		buttonsActionsMap.append(R.id.menu_emulation_load_5, EmulationActivity.MENU_ACTION_LOAD_SLOT5);
		buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
	}

	public static void launch(FragmentActivity activity, String path, String title, String screenshotPath, int position, View sharedView)
	{
		Intent launcher = new Intent(activity, EmulationActivity.class);

		launcher.putExtra("SelectedGame", path);
		launcher.putExtra("SelectedTitle", title);
		launcher.putExtra("ScreenPath", screenshotPath);
		launcher.putExtra("GridPosition", position);

		ActivityOptionsCompat options = ActivityOptionsCompat.makeSceneTransitionAnimation(
				activity,
				sharedView,
				"image_game_screenshot");

		// I believe this warning is a bug. Activities are FragmentActivity from the support lib
		//noinspection RestrictedApi
		activity.startActivityForResult(launcher, MainPresenter.REQUEST_EMULATE_GAME, options.toBundle());
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Get params we were passed
		Intent gameToEmulate = getIntent();
		String path = gameToEmulate.getStringExtra("SelectedGame");
		sIsGameCubeGame = Platform.fromNativeInt(NativeLibrary.GetPlatform(path)) == Platform.GAMECUBE;
		mSelectedTitle = gameToEmulate.getStringExtra("SelectedTitle");
		mScreenPath = gameToEmulate.getStringExtra("ScreenPath");
		mPosition = gameToEmulate.getIntExtra("GridPosition", -1);
		mDeviceHasTouchScreen = getPackageManager().hasSystemFeature("android.hardware.touchscreen");
		mControllerMappingHelper = new ControllerMappingHelper();

		int themeId;
		if (mDeviceHasTouchScreen)
		{
			themeId = R.style.DolphinEmulationGamecube;

			// Get a handle to the Window containing the UI.
			mDecorView = getWindow().getDecorView();
			mDecorView.setOnSystemUiVisibilityChangeListener
					(new View.OnSystemUiVisibilityChangeListener() {
				@Override
				public void onSystemUiVisibilityChange(int visibility) {
					if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0)
					{
						// Go back to immersive fullscreen mode in 3s
						Handler handler = new Handler(getMainLooper());
						handler.postDelayed(new Runnable()
						{
							@Override
							public void run()
							{
								enableFullscreenImmersive();
							}
						},
						3000 /* 3s */);
					}
				}
			});
			// Set these options now so that the SurfaceView the game renders into is the right size.
			enableFullscreenImmersive();
		}
		else
		{
			themeId = R.style.DolphinEmulationTvGamecube;
		}

		setTheme(themeId);

		Java_GCAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);
		Java_WiimoteAdapter.manager = (UsbManager) getSystemService(Context.USB_SERVICE);

		setContentView(R.layout.activity_emulation);

		mImageView = (ImageView) findViewById(R.id.image_screenshot);

		// Find or create the EmulationFragment
		mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
				.findFragmentById(R.id.frame_emulation_fragment);
		if (mEmulationFragment == null)
		{
			mEmulationFragment = EmulationFragment.newInstance(path);
			getSupportFragmentManager().beginTransaction()
					.add(R.id.frame_emulation_fragment, mEmulationFragment)
					.commit();
		}

		if (savedInstanceState == null)
		{
			// Picasso will take a while to load these big-ass screenshots. So don't run
			// the animation until we say so.
			postponeEnterTransition();

			Picasso.with(this)
					.load(mScreenPath)
					.noFade()
					.noPlaceholder()
					.into(mImageView, new Callback()
					{
						@Override
						public void onSuccess()
						{
							supportStartPostponedEnterTransition();
						}

						@Override
						public void onError()
						{
							// Still have to do this, or else the app will crash.
							supportStartPostponedEnterTransition();
						}
					});

			Animations.fadeViewOut(mImageView)
					.setStartDelay(2000)
					.withEndAction(new Runnable()
					{
						@Override
						public void run()
						{
							mImageView.setVisibility(View.GONE);
						}
					});
		}
		else
		{
			mImageView.setVisibility(View.GONE);
		}

		if (mDeviceHasTouchScreen)
		{
			setTitle(mSelectedTitle);
		}

		mPreferences = PreferenceManager.getDefaultSharedPreferences(this);

	}

	@Override
	public void onBackPressed()
	{
		if (!mDeviceHasTouchScreen)
		{
			boolean popResult = getSupportFragmentManager().popBackStackImmediate(
				BACKSTACK_NAME_SUBMENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);
			if (!popResult)
			{
				toggleMenu();
			}
		}
		else
		{
			mEmulationFragment.stopEmulation();
			exitWithAnimation();
		}

	}

	private void enableFullscreenImmersive()
	{
		// It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
		mDecorView.setSystemUiVisibility(
				View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
				View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.SYSTEM_UI_FLAG_IMMERSIVE);
	}

	private void toggleMenu()
	{
		boolean result = getSupportFragmentManager().popBackStackImmediate(
				BACKSTACK_NAME_MENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);
		mMenuVisible = false;

		if (!result) {
			// Removing the menu failed, so that means it wasn't visible. Add it.
			Fragment fragment = MenuFragment.newInstance(mSelectedTitle);
			getSupportFragmentManager().beginTransaction()
					.setCustomAnimations(
							R.animator.menu_slide_in_from_left,
							R.animator.menu_slide_out_to_left,
							R.animator.menu_slide_in_from_left,
							R.animator.menu_slide_out_to_left)
					.add(R.id.frame_menu, fragment)
					.addToBackStack(BACKSTACK_NAME_MENU)
					.commit();
			mMenuVisible = true;
		}
	}

	public void exitWithAnimation()
	{
		runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				Picasso.with(EmulationActivity.this)
						.invalidate(mScreenPath);

				Picasso.with(EmulationActivity.this)
						.load(mScreenPath)
						.noFade()
						.noPlaceholder()
						.into(mImageView, new Callback()
						{
							@Override
							public void onSuccess()
							{
								showScreenshot();
							}

							@Override
							public void onError()
							{
								finish();
							}
						});
			}
		});
	}

	private void showScreenshot()
	{
		Animations.fadeViewIn(mImageView)
				.withEndAction(afterShowingScreenshot);
	}

	private Runnable afterShowingScreenshot = new Runnable()
	{
		@Override
		public void run()
		{
			setResult(mPosition);
			supportFinishAfterTransition();
		}
	};

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

	@SuppressWarnings("WrongConstant")
	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		int action = buttonsActionsMap.get(item.getItemId(), -1);
		if (action >= 0)
		{
			handleMenuAction(action);
		}
		return true;
	}

	public void handleMenuAction(@MenuAction int menuAction)
	{
		switch (menuAction)
		{
			// Edit the placement of the controls
			case MENU_ACTION_EDIT_CONTROLS_PLACEMENT:
				editControlsPlacement();
				break;

			// Enable/Disable specific buttons or the entire input overlay.
			case MENU_ACTION_TOGGLE_CONTROLS:
				toggleControls();
				return;

			// Adjust the scale of the overlay controls.
			case MENU_ACTION_ADJUST_SCALE:
				adjustScale();
				return;

			// (Wii games only) Change the controller for the input overlay.
			case MENU_ACTION_CHOOSE_CONTROLLER:
				chooseController();
				return;

			case MENU_ACTION_REFRESH_WIIMOTES:
				NativeLibrary.RefreshWiimotes();
				return;

			// Screenshot capturing
			case MENU_ACTION_TAKE_SCREENSHOT:
				NativeLibrary.SaveScreenShot();
				return;

			// Quick save / load
			case MENU_ACTION_QUICK_SAVE:
				NativeLibrary.SaveState(9);
				return;

			case MENU_ACTION_QUICK_LOAD:
				NativeLibrary.LoadState(9);
				return;

			// TV Menu only
			case MENU_ACTION_SAVE_ROOT:
				if (!mDeviceHasTouchScreen)
				{
					showSubMenu(SaveLoadStateFragment.SaveOrLoad.SAVE);
				}
				return;

			case MENU_ACTION_LOAD_ROOT:
				if (!mDeviceHasTouchScreen)
				{
					showSubMenu(SaveLoadStateFragment.SaveOrLoad.LOAD);
				}
				return;

			// Save state slots
			case MENU_ACTION_SAVE_SLOT1:
				NativeLibrary.SaveState(0);
				return;

			case MENU_ACTION_SAVE_SLOT2:
				NativeLibrary.SaveState(1);
				return;

			case MENU_ACTION_SAVE_SLOT3:
				NativeLibrary.SaveState(2);
				return;

			case MENU_ACTION_SAVE_SLOT4:
				NativeLibrary.SaveState(3);
				return;

			case MENU_ACTION_SAVE_SLOT5:
				NativeLibrary.SaveState(4);
				return;

			case MENU_ACTION_SAVE_SLOT6:
				NativeLibrary.SaveState(5);
				return;

			// Load state slots
			case MENU_ACTION_LOAD_SLOT1:
				NativeLibrary.LoadState(0);
				return;

			case MENU_ACTION_LOAD_SLOT2:
				NativeLibrary.LoadState(1);
				return;

			case MENU_ACTION_LOAD_SLOT3:
				NativeLibrary.LoadState(2);
				return;

			case MENU_ACTION_LOAD_SLOT4:
				NativeLibrary.LoadState(3);
				return;

			case MENU_ACTION_LOAD_SLOT5:
				NativeLibrary.LoadState(4);
				return;

			case MENU_ACTION_LOAD_SLOT6:
				NativeLibrary.LoadState(5);
				return;

			case MENU_ACTION_EXIT:
				toggleMenu();  // Hide the menu (it will be showing since we just clicked it)
				mEmulationFragment.stopEmulation();
				exitWithAnimation();
				return;
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

	private void toggleControls() {
		final SharedPreferences.Editor editor = mPreferences.edit();
		boolean[] enabledButtons = new boolean[14];
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_toggle_controls);
		if (sIsGameCubeGame || mPreferences.getInt("wiiController", 3) == 0) {
			for (int i = 0; i < enabledButtons.length; i++) {
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleGc" + i, true);
			}
			builder.setMultiChoiceItems(R.array.gcpadButtons, enabledButtons,
					new DialogInterface.OnMultiChoiceClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int indexSelected, boolean isChecked) {
							editor.putBoolean("buttonToggleGc" + indexSelected, isChecked);
						}
					});
		} else if (mPreferences.getInt("wiiController", 3) == 4) {
			for (int i = 0; i < enabledButtons.length; i++) {
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleClassic" + i, true);
			}
			builder.setMultiChoiceItems(R.array.classicButtons, enabledButtons,
					new DialogInterface.OnMultiChoiceClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int indexSelected, boolean isChecked) {
							editor.putBoolean("buttonToggleClassic" + indexSelected, isChecked);
						}
					});
		} else {
			for (int i = 0; i < enabledButtons.length; i++) {
				enabledButtons[i] = mPreferences.getBoolean("buttonToggleWii" + i, true);
			}
			if (mPreferences.getInt("wiiController", 3) == 3) {
				builder.setMultiChoiceItems(R.array.nunchukButtons, enabledButtons,
						new DialogInterface.OnMultiChoiceClickListener() {
							@Override
							public void onClick(DialogInterface dialog, int indexSelected, boolean isChecked) {
								editor.putBoolean("buttonToggleWii" + indexSelected, isChecked);
							}
						});
			} else {
				builder.setMultiChoiceItems(R.array.wiimoteButtons, enabledButtons,
						new DialogInterface.OnMultiChoiceClickListener() {
							@Override
							public void onClick(DialogInterface dialog, int indexSelected, boolean isChecked) {
								editor.putBoolean("buttonToggleWii" + indexSelected, isChecked);
							}
						});
			}
		}
		builder.setNeutralButton(getString(R.string.emulation_toggle_all), new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialogInterface, int i)
			{
				mEmulationFragment.toggleInputOverlayVisibility();
			}
		});
		builder.setPositiveButton(getString(R.string.ok), new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialogInterface, int i)
			{
				editor.apply();

				mEmulationFragment.refreshInputOverlay();
			}
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void adjustScale() {
		LayoutInflater inflater = LayoutInflater.from(this);
		View view = inflater.inflate(R.layout.dialog_seekbar, null);

		final SeekBar seekbar = (SeekBar) view.findViewById(R.id.seekbar);
		final TextView value = (TextView) view.findViewById(R.id.text_value);
		final TextView units = (TextView) view.findViewById(R.id.text_units);

		seekbar.setMax(150);
		seekbar.setProgress(mPreferences.getInt("controlScale", 50));
		seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			public void onStartTrackingTouch(SeekBar seekBar) {
				// Do nothing
			}

			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				value.setText(String.valueOf(progress + 50));
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				// Do nothing
			}
		});

		value.setText(String.valueOf(seekbar.getProgress() + 50));
		units.setText("%");

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_control_scale);
		builder.setView(view);
		builder.setPositiveButton(getString(R.string.ok), new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialogInterface, int i) {
				SharedPreferences.Editor editor = mPreferences.edit();
				editor.putInt("controlScale", seekbar.getProgress());
				editor.apply();

				mEmulationFragment.refreshInputOverlay();
			}
		});

		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	private void chooseController() {
		final SharedPreferences.Editor editor = mPreferences.edit();
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.emulation_choose_controller);
		builder.setSingleChoiceItems(R.array.controllersEntries, mPreferences.getInt("wiiController", 3),
				new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int indexSelected) {
						editor.putInt("wiiController", indexSelected);

						NativeLibrary.SetConfig("WiimoteNew.ini", "Wiimote1", "Extension",
								getResources().getStringArray(R.array.controllersValues)[indexSelected]);
					}
				});
		builder.setPositiveButton(getString(R.string.ok), new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialogInterface, int i) {
				editor.apply();

				mEmulationFragment.refreshInputOverlay();

				Toast.makeText(getApplication(), R.string.emulation_controller_changed, Toast.LENGTH_SHORT).show();
			}
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

	private void showSubMenu(SaveLoadStateFragment.SaveOrLoad saveOrLoad)
	{
		// Get rid of any visible submenu
		getSupportFragmentManager().popBackStack(
				BACKSTACK_NAME_SUBMENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);

		Fragment fragment = SaveLoadStateFragment.newInstance(saveOrLoad);
		getSupportFragmentManager().beginTransaction()
				.setCustomAnimations(
						R.animator.menu_slide_in_from_right,
						R.animator.menu_slide_out_to_right,
						R.animator.menu_slide_in_from_right,
						R.animator.menu_slide_out_to_right)
				.replace(R.id.frame_submenu, fragment)
				.addToBackStack(BACKSTACK_NAME_SUBMENU)
				.commit();
	}

	public String getSelectedTitle()
	{
		return mSelectedTitle;
	}

	public static boolean isGameCubeGame()
	{
		return sIsGameCubeGame;
	}
}
