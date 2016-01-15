package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.Fragment;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;
import org.dolphinemu.dolphinemu.fragments.LoadStateFragment;
import org.dolphinemu.dolphinemu.fragments.MenuFragment;
import org.dolphinemu.dolphinemu.fragments.SaveStateFragment;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.utils.Animations;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.List;

public final class EmulationActivity extends AppCompatActivity
{
	private View mDecorView;
	private ImageView mImageView;

	private FrameLayout mFrameEmulation;
	private LinearLayout mMenuLayout;

	private String mSubmenuFragmentTag;

	// So that MainActivity knows which view to invalidate before the return animation.
	private int mPosition;

	private boolean mDeviceHasTouchScreen;
	private boolean mSystemUiVisible;
	private boolean mMenuVisible;

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
	private String mScreenPath;
	private FrameLayout mFrameContent;
	private String mSelectedTitle;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		mDeviceHasTouchScreen = getPackageManager().hasSystemFeature("android.hardware.touchscreen");

		int themeId;
		if (mDeviceHasTouchScreen)
		{
			themeId = R.style.DolphinEmulationGamecube;

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
								getSupportActionBar().show();
								hideSystemUiAfterDelay();
							}
							else
							{
								getSupportActionBar().hide();
							}
						}
					}
			);
		}
		else
		{
			themeId = R.style.DolphinEmulationTvGamecube;
		}

		setTheme(themeId);
		super.onCreate(savedInstanceState);

		// Picasso will take a while to load these big-ass screenshots. So don't run
		// the animation until we say so.
		postponeEnterTransition();

		setContentView(R.layout.activity_emulation);

		mImageView = (ImageView) findViewById(R.id.image_screenshot);
		mFrameContent = (FrameLayout) findViewById(R.id.frame_content);
		mFrameEmulation = (FrameLayout) findViewById(R.id.frame_emulation_fragment);
		mMenuLayout = (LinearLayout) findViewById(R.id.layout_ingame_menu);

		Intent gameToEmulate = getIntent();
		String path = gameToEmulate.getStringExtra("SelectedGame");
		mSelectedTitle = gameToEmulate.getStringExtra("SelectedTitle");
		mScreenPath = gameToEmulate.getStringExtra("ScreenPath");
		mPosition = gameToEmulate.getIntExtra("GridPosition", -1);

		if (savedInstanceState == null)
		{
			Picasso.with(this)
					.load(mScreenPath)
					.noFade()
					.noPlaceholder()
					.into(mImageView, new Callback()
					{
						@Override
						public void onSuccess()
						{
							scheduleStartPostponedTransition(mImageView);
						}

						@Override
						public void onError()
						{
							// Still have to do this, or else the app will crash.
							scheduleStartPostponedTransition(mImageView);
						}
					});

			Animations.fadeViewOut(mImageView)
					.setStartDelay(2000)
					.withStartAction(new Runnable()
					{
						@Override
						public void run()
						{
							mFrameEmulation.setVisibility(View.VISIBLE);
						}
					})
					.withEndAction(new Runnable()
					{
						@Override
						public void run()
						{
							mImageView.setVisibility(View.GONE);
						}
					});

			// Instantiate an EmulationFragment.
			EmulationFragment emulationFragment = EmulationFragment.newInstance(path);

			// Add fragment to the activity - this triggers all its lifecycle callbacks.
			getFragmentManager().beginTransaction()
					.add(R.id.frame_emulation_fragment, emulationFragment, EmulationFragment.FRAGMENT_TAG)
					.commit();
		}
		else
		{
			mImageView.setVisibility(View.GONE);
			mFrameEmulation.setVisibility(View.VISIBLE);
		}

		if (mDeviceHasTouchScreen)
		{
			setTitle(mSelectedTitle);
		}
		else
		{
			MenuFragment menuFragment = (MenuFragment) getFragmentManager()
					.findFragmentById(R.id.fragment_menu);

			if (menuFragment != null)
			{
				menuFragment.setTitleText(mSelectedTitle);
			}
		}
	}

	@Override
	protected void onStart()
	{
		super.onStart();
		Log.debug("[EmulationActivity] EmulationActivity starting.");
		NativeLibrary.setEmulationActivity(this);
	}

	@Override
	protected void onStop()
	{
		super.onStop();
		Log.debug("[EmulationActivity] EmulationActivity stopping.");

		NativeLibrary.setEmulationActivity(null);
	}

	@Override
	protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);

		if (mDeviceHasTouchScreen)
		{
			// Give the user a few seconds to see what the controls look like, then hide them.
			hideSystemUiAfterDelay();
		}
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);

		if (mDeviceHasTouchScreen)
		{
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
	}

	@Override
	public void onBackPressed()
	{
		if (!mDeviceHasTouchScreen)
		{
			if (mSubmenuFragmentTag != null)
			{
				removeSubMenu();
			}
			else
			{
				toggleMenu();
			}
		}
		else
		{
			stopEmulation();
		}
	}

	private void toggleMenu()
	{
		if (mMenuVisible)
		{
			mMenuVisible = false;

			Animations.fadeViewOutToLeft(mMenuLayout)
					.withEndAction(new Runnable()
					{
						@Override
						public void run()
						{
							if (mMenuVisible)
							{
								mMenuLayout.setVisibility(View.GONE);
							}
						}
					});
		}
		else
		{
			mMenuVisible = true;
			Animations.fadeViewInFromLeft(mMenuLayout);
		}
	}

	private void stopEmulation()
	{
		EmulationFragment fragment = (EmulationFragment) getFragmentManager()
				.findFragmentByTag(EmulationFragment.FRAGMENT_TAG);
		fragment.notifyEmulationStopped();

		NativeLibrary.StopEmulation();
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
			mFrameContent.removeView(mFrameEmulation);
			setResult(mPosition);
			finishAfterTransition();
		}
	};

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
		onMenuItemClicked(item.getItemId());
		return true;
	}

	public void onMenuItemClicked(int id)
	{
		switch (id)
		{
			// Enable/Disable input overlay.
			case R.id.menu_emulation_input_overlay:
			{
				EmulationFragment emulationFragment = (EmulationFragment) getFragmentManager()
						.findFragmentByTag(EmulationFragment.FRAGMENT_TAG);

				emulationFragment.toggleInputOverlayVisibility();

				return;
			}

			// Screenshot capturing
			case R.id.menu_emulation_screenshot:
				NativeLibrary.SaveScreenShot();
				return;

			// Quicksave / Load
			case R.id.menu_quicksave:
				NativeLibrary.SaveState(9);
				return;

			case R.id.menu_quickload:
				NativeLibrary.LoadState(9);
				return;

			// TV Menu only
			case R.id.menu_emulation_save_root:
				showMenu(SaveStateFragment.FRAGMENT_ID);
				return;

			case R.id.menu_emulation_load_root:
				showMenu(LoadStateFragment.FRAGMENT_ID);
				return;

			// Save state slots
			case R.id.menu_emulation_save_1:
				NativeLibrary.SaveState(0);
				return;

			case R.id.menu_emulation_save_2:
				NativeLibrary.SaveState(1);
				return;

			case R.id.menu_emulation_save_3:
				NativeLibrary.SaveState(2);
				return;

			case R.id.menu_emulation_save_4:
				NativeLibrary.SaveState(3);
				return;

			case R.id.menu_emulation_save_5:
				NativeLibrary.SaveState(4);
				return;

			// Load state slots
			case R.id.menu_emulation_load_1:
				NativeLibrary.LoadState(0);
				return;

			case R.id.menu_emulation_load_2:
				NativeLibrary.LoadState(1);
				return;

			case R.id.menu_emulation_load_3:
				NativeLibrary.LoadState(2);
				return;

			case R.id.menu_emulation_load_4:
				NativeLibrary.LoadState(3);
				return;

			case R.id.menu_emulation_load_5:
				NativeLibrary.LoadState(4);
				return;

			case R.id.menu_exit:
				toggleMenu();
				stopEmulation();
				return;
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
		boolean handled = NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
		return handled;
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
			NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), range.getAxis(), event.getAxisValue(range.getAxis()));
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


	private void scheduleStartPostponedTransition(final View sharedElement)
	{
		sharedElement.getViewTreeObserver().addOnPreDrawListener(
				new ViewTreeObserver.OnPreDrawListener()
				{
					@Override
					public boolean onPreDraw()
					{
						sharedElement.getViewTreeObserver().removeOnPreDrawListener(this);
						startPostponedEnterTransition();
						return true;
					}
				});
	}

	private void showMenu(int menuId)
	{
		Fragment fragment;

		switch (menuId)
		{
			case SaveStateFragment.FRAGMENT_ID:
				fragment = SaveStateFragment.newInstance();
				mSubmenuFragmentTag = SaveStateFragment.FRAGMENT_TAG;
				break;

			case LoadStateFragment.FRAGMENT_ID:
				fragment = LoadStateFragment.newInstance();
				mSubmenuFragmentTag = LoadStateFragment.FRAGMENT_TAG;
				break;

			default:
				return;
		}

		getFragmentManager().beginTransaction()
				.setCustomAnimations(R.animator.menu_slide_in, R.animator.menu_slide_out)
				.replace(R.id.frame_submenu, fragment, mSubmenuFragmentTag)
				.commit();
	}

	private void removeSubMenu()
	{
		if (mSubmenuFragmentTag != null)
		{
			final Fragment fragment = getFragmentManager().findFragmentByTag(mSubmenuFragmentTag);

			if (fragment != null)
			{
				// When removing a fragment without replacement, its animation must be done
				// manually beforehand.
				Animations.fadeViewOutToRight(fragment.getView())
						.withEndAction(new Runnable()
						{
							@Override
							public void run()
							{
								if (mMenuVisible)
								{
									getFragmentManager().beginTransaction()
											.remove(fragment)
											.commit();
								}
							}
						});
			}
			else
			{
				Log.error("[EmulationActivity] Fragment not found, can't remove.");
			}

			mSubmenuFragmentTag = null;
		}
		else
		{
			Log.error("[EmulationActivity] Fragment Tag empty.");
		}
	}

	public String getSelectedTitle()
	{
		return mSelectedTitle;
	}

	public static void launch(Activity activity, String path, String title, String screenshotPath, int position, View sharedView)
	{
		Intent launcher = new Intent(activity, EmulationActivity.class);

		launcher.putExtra("SelectedGame", path);
		launcher.putExtra("SelectedTitle", title);
		launcher.putExtra("ScreenPath", screenshotPath);
		launcher.putExtra("GridPosition", position);

		ActivityOptions options = ActivityOptions.makeSceneTransitionAnimation(
				activity,
				sharedView,
				"image_game_screenshot");

		activity.startActivityForResult(launcher, MainPresenter.REQUEST_EMULATE_GAME, options.toBundle());
	}
}
