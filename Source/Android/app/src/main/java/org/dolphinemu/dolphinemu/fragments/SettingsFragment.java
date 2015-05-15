package org.dolphinemu.dolphinemu.fragments;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import javax.microedition.khronos.opengles.GL10;

public final class SettingsFragment extends PreferenceFragment implements SharedPreferences.OnSharedPreferenceChangeListener
{
	private SharedPreferences mPreferences;
	private ListPreference mVideoBackendPreference;

	private final EGLHelper mEglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);
	private final String mVendor = mEglHelper.getGL().glGetString(GL10.GL_VENDOR);

	private final String mVersion = mEglHelper.getGL().glGetString(GL10.GL_VERSION);

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Load the preferences from an XML resource
		addPreferencesFromResource(R.xml.preferences);

		// TODO Below here is effectively ported from the old VideoSettingsFragment. There is
		// TODO probably a simpler way to do this, but potentially could require UI discussion/feedback.

		// Setting valid video backends.
		mVideoBackendPreference = (ListPreference) findPreference("gpuPref");
		final boolean deviceSupportsGL = mEglHelper.supportsOpenGL();
		final boolean deviceSupportsGLES3 = mEglHelper.supportsGLES3();

		if (deviceSupportsGL)
		{
			mVideoBackendPreference.setEntries(R.array.videoBackendEntriesGL);
			mVideoBackendPreference.setEntryValues(R.array.videoBackendValuesGL);
		}
		else if (deviceSupportsGLES3)
		{
			mVideoBackendPreference.setEntries(R.array.videoBackendEntriesGLES3);
			mVideoBackendPreference.setEntryValues(R.array.videoBackendValuesGLES3);
		}
		else
		{
			mVideoBackendPreference.setEntries(R.array.videoBackendEntriesNoGLES3);
			mVideoBackendPreference.setEntryValues(R.array.videoBackendValuesNoGLES3);
		}

		//
		// Set available post processing shaders
		//

		List<CharSequence> shader_names = new ArrayList<CharSequence>();
		List<CharSequence> shader_values = new ArrayList<CharSequence>();

		// Disabled option
		shader_names.add("Disabled");
		shader_values.add("");

		// TODO Since shaders are included with the APK, we know what they are at build-time. We should
		// TODO be able to run this logic somehow at build-time and not rely on the device doing it.

		File shaders_folder = new File(Environment.getExternalStorageDirectory() + File.separator + "dolphin-emu" + File.separator + "Shaders");
		if (shaders_folder.exists())
		{
			File[] shaders = shaders_folder.listFiles();
			for (File file : shaders)
			{
				if (file.isFile())
				{
					String filename = file.getName();
					if (filename.endsWith(".glsl"))
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
		mPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity());

		if (mVideoBackendPreference.getValue().equals("Software Renderer"))
		{
			findPreference("enhancements").setEnabled(false);
			findPreference("hacks").setEnabled(false);
			findPreference("showFPS").setEnabled(false);
		}
		else if (mVideoBackendPreference.getValue().equals("OGL"))
		{
			findPreference("enhancements").setEnabled(true);
			findPreference("hacks").setEnabled(true);
			findPreference("showFPS").setEnabled(true);

			// Check if we support stereo
			// If we support desktop GL then we must support at least OpenGL 3.2
			// If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
			if ((mEglHelper.supportsOpenGL() && mEglHelper.GetVersion() >= 320) ||
					(mEglHelper.supportsGLES3() && mEglHelper.GetVersion() >= 310 && mEglHelper.SupportsExtension("GL_ANDROID_extension_pack_es31a")))
				findPreference("StereoscopyScreen").setEnabled(true);
			else
				findPreference("StereoscopyScreen").setEnabled(false);
		}

		// Also set a listener, so that if someone changes the video backend, it will disable
		// the video settings, upon the user choosing "Software Rendering".
		mPreferences.registerOnSharedPreferenceChangeListener(this);
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences preferences, String key)
	{
		if (key.equals("gpuPref"))
		{
			if (preferences.getString(key, "Software Renderer").equals("Software Renderer"))
			{
				findPreference("enhancements").setEnabled(false);
				findPreference("hacks").setEnabled(false);
				findPreference("showFPS").setEnabled(false);
			}
			else if (preferences.getString(key, "Software Renderer").equals("OGL"))
			{
				findPreference("enhancements").setEnabled(true);
				findPreference("hacks").setEnabled(true);
				findPreference("showFPS").setEnabled(true);

				// Create an alert telling them that their phone sucks
				if (mEglHelper.supportsGLES3()
						&& mVendor.equals("Qualcomm")
						&& getQualcommVersion() == 14.0f)
				{
					AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
					builder.setTitle(R.string.device_compat_warning);
					builder.setMessage(R.string.device_gles3compat_warning_msg);
					builder.setPositiveButton(R.string.yes, null);
					builder.setNegativeButton(R.string.no, new DialogInterface.OnClickListener()
					{
						public void onClick(DialogInterface dialog, int which)
						{
							// Get an editor.
							SharedPreferences.Editor editor = mPreferences.edit();
							editor.putString("gpuPref", "Software Renderer");
							editor.apply();
							mVideoBackendPreference.setValue("Software Renderer");
						}
					});
					builder.show();
				}
			}
		}
	}

	private float getQualcommVersion()
	{
		final int start = mVersion.indexOf("V@") + 2;
		final StringBuilder versionBuilder = new StringBuilder();

		for (int i = start; i < mVersion.length(); i++)
		{
			char c = mVersion.charAt(i);

			// End of numeric portion of version string.
			if (c == ' ')
				break;

			versionBuilder.append(c);
		}

		return Float.parseFloat(versionBuilder.toString());
	}
}
