/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.video;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.os.Environment;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import javax.microedition.khronos.opengles.GL10;

/**
 * Responsible for handling the loading of the video preferences.
 */
public final class VideoSettingsFragment extends PreferenceFragment
{
	private final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);
	private final String vendor = eglHelper.getGL().glGetString(GL10.GL_VENDOR);
	private final String version = eglHelper.getGL().glGetString(GL10.GL_VERSION);

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Load the preferences from an XML resource
		addPreferencesFromResource(R.xml.video_prefs);

		//
		// Setting valid video backends.
		//
		final ListPreference videoBackends = (ListPreference) findPreference("gpuPref");
		final boolean deviceSupportsGL = eglHelper.supportsOpenGL();
		final boolean deviceSupportsGLES3 = eglHelper.supportsGLES3();

		if (deviceSupportsGL)
		{
			videoBackends.setEntries(R.array.videoBackendEntriesGL);
			videoBackends.setEntryValues(R.array.videoBackendValuesGL);
		}
		else if (deviceSupportsGLES3)
		{
			videoBackends.setEntries(R.array.videoBackendEntriesGLES3);
			videoBackends.setEntryValues(R.array.videoBackendValuesGLES3);
		}
		else
		{
			videoBackends.setEntries(R.array.videoBackendEntriesNoGLES3);
			videoBackends.setEntryValues(R.array.videoBackendValuesNoGLES3);
		}

		//
		// Set available post processing shaders
		//

		List<CharSequence> shader_names = new ArrayList<CharSequence>();
		List<CharSequence> shader_values = new ArrayList<CharSequence>();

		// Disabled option
		shader_names.add("Disabled");
		shader_values.add("");

		File shaders_folder = new File(Environment.getExternalStorageDirectory()+ File.separator+"dolphin-emu"+ File.separator+"Shaders");
		if (shaders_folder.exists())
		{
			File[] shaders = shaders_folder.listFiles();
			for (File file : shaders)
			{
				if (file.isFile())
				{
					String filename = file.getName();
					if (filename.contains(".glsl"))
					{
						// Strip the extension and put it in to the list
						shader_names.add(filename.substring(0, filename.lastIndexOf('.')));
						shader_values.add(filename.substring(0, filename.lastIndexOf('.')));
					}
				}
			}
		}

		final ListPreference shader_preference = (ListPreference) findPreference("postProcessingShader");
		shader_preference.setEntries(shader_names.toArray(new CharSequence[shader_names.size()]));
		shader_preference.setEntryValues(shader_values.toArray(new CharSequence[shader_values.size()]));

		//
		// Disable all options if Software Rendering is used.
		//
		// Note that the numeric value in 'getPreference()'
		// denotes the placement on the UI. So if more elements are
		// added to the video settings, these may need to change.
		//
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		final PreferenceScreen mainScreen = getPreferenceScreen();

		if (videoBackends.getValue().equals("Software Renderer"))
		{
			mainScreen.getPreference(0).setEnabled(false);
			mainScreen.getPreference(1).setEnabled(false);
			mainScreen.getPreference(3).setEnabled(false);
		}
		else if (videoBackends.getValue().equals("OGL"))
		{
			mainScreen.getPreference(0).setEnabled(true);
			mainScreen.getPreference(1).setEnabled(true);
			mainScreen.getPreference(3).setEnabled(true);
		}

		// Also set a listener, so that if someone changes the video backend, it will disable
		// the video settings, upon the user choosing "Software Rendering".
		sPrefs.registerOnSharedPreferenceChangeListener(new OnSharedPreferenceChangeListener()
		{
			@Override
			public void onSharedPreferenceChanged(SharedPreferences preference, String key)
			{
				if (key.equals("gpuPref"))
				{
					if (preference.getString(key, "Software Renderer").equals("Software Renderer"))
					{
						mainScreen.getPreference(0).setEnabled(false);
						mainScreen.getPreference(1).setEnabled(false);
						mainScreen.getPreference(3).setEnabled(false);
					}
					else if (preference.getString(key, "Software Renderer").equals("OGL"))
					{
						mainScreen.getPreference(0).setEnabled(true);
						mainScreen.getPreference(1).setEnabled(true);
						mainScreen.getPreference(3).setEnabled(true);

						// Create an alert telling them that their phone sucks
						if (eglHelper.supportsGLES3()
								&& vendor.equals("Qualcomm")
								&& getQualcommVersion() == 14.0f)
						{
							AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
							builder.setTitle(R.string.device_compat_warning);
							builder.setMessage(R.string.device_gles3compat_warning_msg);
							builder.setPositiveButton(R.string.yes, null);
							builder.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog, int which)
								{
									// Get an editor.
									SharedPreferences.Editor editor = sPrefs.edit();
									editor.putString("gpuPref", "Software Renderer");
									editor.apply();
									videoBackends.setValue("Software Renderer");
									videoBackends.setSummary("Software Renderer");
								}
							});
							builder.show();
						}
					}
				}
			}
		});
	}

	private float getQualcommVersion()
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

		return Float.parseFloat(versionBuilder.toString());
	}
}
