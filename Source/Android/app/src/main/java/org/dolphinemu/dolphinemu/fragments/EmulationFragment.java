package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.utils.Log;

public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback
{
	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".emulation_fragment";

	private static final String ARGUMENT_GAME_PATH = BuildConfig.APPLICATION_ID + ".game_path";

	private SharedPreferences mPreferences;

	private Surface mSurface;

	private InputOverlay mInputOverlay;

	private Thread mEmulationThread;

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
		View contents = inflater.inflate(R.layout.fragment_emulation, container, false);

		SurfaceView surfaceView = (SurfaceView) contents.findViewById(R.id.surface_emulation);
		mInputOverlay = (InputOverlay) contents.findViewById(R.id.surface_input_overlay);

		surfaceView.getHolder().addCallback(this);

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
	public void onViewCreated(View view, Bundle savedInstanceState)
	{
		Button doneButton = (Button) view.findViewById(R.id.done_control_config);
		if (doneButton != null)
		{
			doneButton.setOnClickListener(new View.OnClickListener()
			{
				@Override
				public void onClick(View v)
				{
					stopConfiguringControls();
				}
			});
		}
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

	public void refreshInputOverlay()
	{
		mInputOverlay.refreshControls();
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder)
	{
		Log.debug("[EmulationFragment] Surface created.");
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		Log.debug("[EmulationFragment] Surface changed. Resolution: " + width + "x" + height);
		mSurface = holder.getSurface();
		NativeLibrary.SurfaceChanged(mSurface);
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder)
	{
		Log.debug("[EmulationFragment] Surface destroyed.");
		NativeLibrary.SurfaceDestroyed();

		if (mEmulationRunning)
		{
			pauseEmulation();
		}
	}

	private void startEmulation()
	{
		if (!mEmulationStarted)
		{
			Log.debug("[EmulationFragment] Starting emulation thread.");

			mEmulationThread.start();
		}
		else
		{
			Log.debug("[EmulationFragment] Resuming emulation.");
			NativeLibrary.UnPauseEmulation();
		}

		mEmulationRunning = true;
	}

	private void pauseEmulation()
	{
		Log.debug("[EmulationFragment] Pausing emulation.");

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

			while (mSurface == null)
				if (!mEmulationRunning)
					return;

			Log.info("[EmulationFragment] Starting emulation: " + mSurface);

			// Start emulation using the provided Surface.
			String path = getArguments().getString(ARGUMENT_GAME_PATH);
			NativeLibrary.Run(path);
		}
	};

	public void startConfiguringControls()
	{
		getView().findViewById(R.id.done_control_config).setVisibility(View.VISIBLE);
		mInputOverlay.setIsInEditMode(true);
	}

	public void stopConfiguringControls()
	{
		getView().findViewById(R.id.done_control_config).setVisibility(View.GONE);
		mInputOverlay.setIsInEditMode(false);
	}

	public boolean isConfiguringControls()
	{
		return mInputOverlay.isInEditMode();
	}
}
