package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;


public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback
{
	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".emulation_fragment";

	private static final String ARGUMENT_GAME_PATH = BuildConfig.APPLICATION_ID + ".game_path";

	private SharedPreferences mPreferences;

	private SurfaceView mSurfaceView;
	private Surface mSurface;

	private InputOverlay mInputOverlay;

	private Thread mEmulationThread;

	private String mPath;

	private boolean mEmulationStarted;
	private boolean mEmulationRunning;


	public static EmulationFragment newInstance(String path)
	{
		EmulationFragment fragment = new EmulationFragment();

		Bundle arguments = new Bundle();
		arguments.putString(ARGUMENT_GAME_PATH, path);
		fragment.setArguments(arguments);

		return fragment;
	}

	/**
	 * Initialize anything that doesn't depend on the layout / views in here.
	 */
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// So this fragment doesn't restart on configuration changes; i.e. rotation.
		setRetainInstance(true);

		mPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity());
	}

	/**
	 * Initialize the UI and start emulation in here.
	 */
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		mPath = getArguments().getString(ARGUMENT_GAME_PATH);
		NativeLibrary.SetFilename(mPath);

		View contents = inflater.inflate(R.layout.fragment_emulation, container, false);

		mSurfaceView = (SurfaceView) contents.findViewById(R.id.surface_emulation);
		mInputOverlay = (InputOverlay) contents.findViewById(R.id.surface_input_overlay);

		mSurfaceView.getHolder().addCallback(this);

		// If the input overlay was previously disabled, then don't show it.
		if (mInputOverlay != null)
		{
			if (!mPreferences.getBoolean("showInputOverlay", true))
			{
				mInputOverlay.setVisibility(View.GONE);
			}
		}


		if (savedInstanceState == null)
		{
			mEmulationThread = new Thread(mEmulationRunner);
		}
		else
		{
			// Likely a rotation occurred.
			// TODO Pass native code the Surface, which will have been recreated, from surfaceChanged()
			// TODO Also, write the native code that will get the video backend to accept the new Surface as one of its own.
		}

		return contents;
	}

	@Override
	public void onStart()
	{
		super.onStart();
		startEmulation();
	}

	@Override
	public void onStop()
	{
		super.onStop();
		pauseEmulation();
	}

	@Override
	public void onDestroyView()
	{
		super.onDestroyView();
		if (getActivity().isFinishing() && mEmulationStarted)
		{
			NativeLibrary.StopEmulation();
		}
	}

	public void toggleInputOverlayVisibility()
	{
		SharedPreferences.Editor editor = mPreferences.edit();

		// If the overlay is currently set to INVISIBLE
		if (!mPreferences.getBoolean("showInputOverlay", false))
		{
			// Set it to VISIBLE
			mInputOverlay.setVisibility(View.VISIBLE);
			editor.putBoolean("showInputOverlay", true);
		}
		else
		{
			// Set it to INVISIBLE
			mInputOverlay.setVisibility(View.GONE);
			editor.putBoolean("showInputOverlay", false);
		}

		editor.apply();
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder)
	{
		Log.d("DolphinEmu", "Surface created.");
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		Log.d("DolphinEmu", "Surface changed. Resolution: " + width + "x" + height);
		mSurface = holder.getSurface();
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder)
	{
		Log.d("DolphinEmu", "Surface destroyed.");

		if (mEmulationRunning)
		{
			pauseEmulation();
		}
	}

	private void startEmulation()
	{
		if (!mEmulationStarted)
		{
			Log.d("DolphinEmu", "Starting emulation thread.");

			mEmulationThread.start();
		}
		else
		{
			Log.d("DolphinEmu", "Resuming emulation.");
			NativeLibrary.UnPauseEmulation();
		}

		mEmulationRunning = true;
	}

	private void pauseEmulation()
	{
		Log.d("DolphinEmu", "Pausing emulation.");

		NativeLibrary.PauseEmulation();
		mEmulationRunning = false;
	}

	/**
	 * Called by containing activity to tell the Fragment emulation is already stopping,
	 * so it doesn't try to stop emulation on its way to the garbage collector.
	 */
	public void notifyEmulationStopped()
	{
		mEmulationStarted = false;
		mEmulationRunning = false;
	}

	private Runnable mEmulationRunner = new Runnable()
	{
		@Override
		public void run()
		{
			mEmulationRunning = true;
			mEmulationStarted = true;

			// Loop until onSurfaceCreated succeeds
			while (mSurface == null)
			{
				if (!mEmulationRunning)
				{
					// So that if the user quits before this gets a surface, we don't loop infinitely.
					return;
				}
			}

			Log.i("DolphinEmu", "Starting emulation: " + mSurface);

			// Start emulation using the provided Surface.
			NativeLibrary.Run(mSurface);
		}
	};
}
