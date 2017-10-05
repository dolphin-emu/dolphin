package org.dolphinemu.dolphinemu.fragments;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.overlay.InputOverlay;
import org.dolphinemu.dolphinemu.utils.Log;

public final class EmulationFragment extends Fragment implements SurfaceHolder.Callback
{
	private SharedPreferences mPreferences;

	private Surface mSurface;

	private InputOverlay mInputOverlay;

	private Thread mEmulationThread;

	private String mGamePath;
	private final EmulationState mEmulationState = new EmulationState();

	@Override
	public void onAttach(Context context)
	{
		super.onAttach(context);

		if (context instanceof EmulationActivity)
		{
			NativeLibrary.setEmulationActivity((EmulationActivity) context);
		}
		else
		{
			throw new IllegalStateException("EmulationFragment must have EmulationActivity parent");
		}
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

		SurfaceView surfaceView = contents.findViewById(R.id.surface_emulation);
		surfaceView.getHolder().addCallback(this);

		mInputOverlay = contents.findViewById(R.id.surface_input_overlay);
		if (mInputOverlay != null)
		{
			// If the input overlay was previously disabled, then don't show it.
			if (!mPreferences.getBoolean("showInputOverlay", true))
			{
				mInputOverlay.setVisibility(View.GONE);
			}
		}

		Button doneButton = contents.findViewById(R.id.done_control_config);
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

		// The new Surface created here will get passed to the native code via onSurfaceChanged.

		return contents;
	}

	@Override
	public void onStop()
	{
		pauseEmulation();
		super.onStop();
	}

	@Override
	public void onDetach()
	{
		NativeLibrary.clearEmulationActivity();
		super.onDetach();
	}

	public void setGamePath(String setPath)
	{
		this.mGamePath = setPath;
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

		if (mEmulationState.isPaused())
		{
			NativeLibrary.UnPauseEmulation();
		}

		mSurface = holder.getSurface();
		NativeLibrary.SurfaceChanged(mSurface);
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder)
	{
		Log.debug("[EmulationFragment] Surface destroyed.");
		NativeLibrary.SurfaceDestroyed();

		if (mEmulationState.isRunning())
		{
			pauseEmulation();
		}
	}

	public void startEmulation()
	{
		synchronized (mEmulationState)
		{
			if (mEmulationState.isStopped())
			{
				Log.debug("[EmulationFragment] Starting emulation thread.");

				mEmulationThread = new Thread(mEmulationRunner, "NativeEmulation");
				mEmulationThread.start();
				// The thread will call mEmulationState.run()
			}
			else if (mEmulationState.isPaused())
			{
				Log.debug("[EmulationFragment] Resuming emulation.");
				NativeLibrary.UnPauseEmulation();
				mEmulationState.run();
			}
			else
			{
				Log.debug("[EmulationFragment] Bug, startEmulation called while running.");
			}
		}
	}

	public void stopEmulation() {
		synchronized (mEmulationState)
		{
			if (!mEmulationState.isStopped())
			{
				NativeLibrary.StopEmulation();
				mEmulationState.stop();
			}
		}
	}

	private void pauseEmulation()
	{
		synchronized (mEmulationState)
		{
			Log.debug("[EmulationFragment] Pausing emulation.");

			NativeLibrary.PauseEmulation();
			mEmulationState.pause();
		}
	}

	private Runnable mEmulationRunner = new Runnable()
	{
		@Override
		public void run()
		{
			// Busy-wait for surface to be set
			while (mSurface == null) {}

			synchronized (mEmulationState)
			{
				Log.info("[EmulationFragment] Starting emulation: " + mSurface);

				mEmulationState.run();
			}
			// Start emulation using the provided Surface.
			NativeLibrary.Run(mGamePath);
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

	private static class EmulationState
	{
		private enum State
		{
			STOPPED, RUNNING, PAUSED
		}

		private State state;

		EmulationState()
		{
			// Starting state is stopped.
			state = State.STOPPED;
		}

		public boolean isStopped()
		{
			return state == State.STOPPED;
		}

		public boolean isRunning()
		{
			return state == State.RUNNING;
		}

		public boolean isPaused()
		{
			return state == State.PAUSED;
		}

		public void run()
		{
			state = State.RUNNING;
		}

		public void pause()
		{
			state = State.PAUSED;
		}

		public void stop()
		{
			state = State.STOPPED;
		}
	}
}
