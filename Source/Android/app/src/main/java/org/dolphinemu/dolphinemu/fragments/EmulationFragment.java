package org.dolphinemu.dolphinemu.fragments;

import android.app.Fragment;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.emulation.overlay.InputOverlay;


public final class EmulationFragment extends Fragment
{
	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".emulation_fragment";

	private static final String ARGUMENT_GAME_PATH = BuildConfig.APPLICATION_ID + ".game_path";

	private SharedPreferences mPreferences;

	private InputOverlay mInputOverlay;

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

		mPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity());
	}

	/**
	 * Initialize the UI and start emulation in here.
	 */
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		String path = getArguments().getString(ARGUMENT_GAME_PATH);

		View contents = inflater.inflate(R.layout.fragment_emulation, container, false);

		mInputOverlay = (InputOverlay) contents.findViewById(R.id.surface_input_overlay);

		NativeLibrary.SetFilename(path);


		// If the input overlay was previously disabled, then don't show it.
		if (!mPreferences.getBoolean("showInputOverlay", true))
		{
			mInputOverlay.setVisibility(View.GONE);
		}

		return contents;
	}

	@Override
	public void onStart()
	{
		super.onStart();
		NativeLibrary.UnPauseEmulation();
	}

	@Override
	public void onStop()
	{
		super.onStop();
		NativeLibrary.PauseEmulation();
	}

	@Override
	public void onDestroyView()
	{
		super.onDestroyView();
		NativeLibrary.StopEmulation();
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
}
