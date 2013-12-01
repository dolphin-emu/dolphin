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
import android.preference.ListPreference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import org.dolphinemu.dolphinemu.R;

import javax.microedition.khronos.egl.*;
import javax.microedition.khronos.opengles.GL10;

/**
 * Responsible for handling the loading of the video preferences.
 */
public final class VideoSettingsFragment extends PreferenceFragment
{
	public static String m_GLVersion;
	public static String m_GLVendor;
	public static String m_GLRenderer;
	public static String m_GLExtensions;
	public static float m_QualcommVersion;
	public static boolean m_Inited = false;

	/**
	 * Class which provides a means to retrieve various
	 * info about the OpenGL ES support/features within a device.
	 */
	public static final class VersionCheck
	{
		private EGL10 mEGL;
		private EGLDisplay mEGLDisplay;
		private EGLConfig[] mEGLConfigs;
		private EGLConfig mEGLConfig;
		private EGLContext mEGLContext;
		private EGLSurface mEGLSurface;
		private GL10 mGL;

		String mThreadOwner;

		public VersionCheck()
		{
			int[] version = new int[2];
			int[] attribList = new int[] {
					EGL10.EGL_WIDTH, 1,
					EGL10.EGL_HEIGHT, 1,
					EGL10.EGL_RENDERABLE_TYPE, 4,
					EGL10.EGL_NONE
			};
			int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
			int[] ctx_attribs = new int[] {
					EGL_CONTEXT_CLIENT_VERSION, 2,
					EGL10.EGL_NONE
			};

			// No error checking performed, minimum required code to elucidate logic
			mEGL = (EGL10) EGLContext.getEGL();
			mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
			mEGL.eglInitialize(mEGLDisplay, version);
			mEGLConfig = chooseConfig(); // Choosing a config is a little more complicated
			mEGLContext = mEGL.eglCreateContext(mEGLDisplay, mEGLConfig, EGL10.EGL_NO_CONTEXT, ctx_attribs);
			mEGLSurface = mEGL.eglCreatePbufferSurface(mEGLDisplay, mEGLConfig,  attribList);
			mEGL.eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext);
			mGL = (GL10) mEGLContext.getGL();

			// Record thread owner of OpenGL context
			mThreadOwner = Thread.currentThread().getName();
		}

		/**
		 * Gets the OpenGL ES version string.
		 * 
		 * @return  the OpenGL ES version string.
		 */
		public String getVersion()
		{
			return mGL.glGetString(GL10.GL_VERSION);
		}

		/**
		 * Gets the OpenGL ES vendor string.
		 * 
		 * @return the OpenGL ES vendor string.
		 */
		public String getVendor()
		{
			return mGL.glGetString(GL10.GL_VENDOR);
		}

		/**
		 * Gets the name of the OpenGL ES renderer.
		 * 
		 * @return the name of the OpenGL ES renderer.
		 */
		public String getRenderer()
		{
			return mGL.glGetString(GL10.GL_RENDERER);
		}

		/**
		 * Gets the extension that the device supports
		 *
		 * @return String containing the extensions
		 */
		public String getExtensions()
		{
			return mGL.glGetString(GL10.GL_EXTENSIONS);
		}

		private EGLConfig chooseConfig()
		{
			int[] attribList = new int[] {
					EGL10.EGL_DEPTH_SIZE, 0,
					EGL10.EGL_STENCIL_SIZE, 0,
					EGL10.EGL_RED_SIZE, 8,
					EGL10.EGL_GREEN_SIZE, 8,
					EGL10.EGL_BLUE_SIZE, 8,
					EGL10.EGL_ALPHA_SIZE, 8,
					EGL10.EGL_NONE
			};

			// No error checking performed, minimum required code to elucidate logic
			// Expand on this logic to be more selective in choosing a configuration
			int[] numConfig = new int[1];
			mEGL.eglChooseConfig(mEGLDisplay, attribList, null, 0, numConfig);
			int configSize = numConfig[0];
			mEGLConfigs = new EGLConfig[configSize];
			mEGL.eglChooseConfig(mEGLDisplay, attribList, mEGLConfigs, configSize, numConfig);

			return mEGLConfigs[0];  // Best match is probably the first configuration
		}
	}

	/**
	 * Checks if this device supports OpenGL ES 3.
	 * 
	 * @return true if this device supports OpenGL ES 3; false otherwise.
	 */
	public static boolean SupportsGLES3()
	{
		boolean mSupportsGLES3 = false;
		if (!m_Inited)
		{
			VersionCheck mbuffer = new VersionCheck();
			m_GLVersion = mbuffer.getVersion();
			m_GLVendor = mbuffer.getVendor();
			m_GLRenderer = mbuffer.getRenderer();
			m_GLExtensions = mbuffer.getExtensions();
			m_Inited = true;
		}


		// Check for OpenGL ES 3 support (General case).
		if (m_GLVersion != null && m_GLVersion.contains("OpenGL ES 3.0"))
			mSupportsGLES3 = true;

		// Checking for OpenGL ES 3 support for certain Qualcomm devices.
		if (m_GLVendor != null && m_GLVendor.equals("Qualcomm"))
		{
			if (m_GLRenderer.contains("Adreno (TM) 3"))
			{
				int mVStart = m_GLVersion.indexOf("V@") + 2;
				int mVEnd = 0;

				for (int a = mVStart; a < m_GLVersion.length(); ++a)
				{
					if (m_GLVersion.charAt(a) == ' ')
					{
						mVEnd = a;
						break;
					}
				}

				m_QualcommVersion = Float.parseFloat(m_GLVersion.substring(mVStart, mVEnd));

				if (m_QualcommVersion  >= 14.0f)
					mSupportsGLES3 = true;
			}
		}

		return mSupportsGLES3;
	}

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
		final boolean deviceSupportsGLES3 = SupportsGLES3();

		if (deviceSupportsGLES3)
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
						//mainScreen.getPreference(4).setEnabled(false);

						// Create an alert telling them that their phone sucks
						if (VideoSettingsFragment.SupportsGLES3()
								&& VideoSettingsFragment.m_GLVendor != null
								&& VideoSettingsFragment.m_GLVendor.equals("Qualcomm")
								&& VideoSettingsFragment.m_QualcommVersion == 14.0f)
						{
							AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
							builder.setTitle(R.string.device_compat_warning);
							builder.setMessage(R.string.device_gles3compat_warning_msg);
							builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog, int which) {
									// Do Nothing. Just create the Yes button
								}
							});
							builder.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog, int which)
								{
									// Get an editor.
									SharedPreferences.Editor editor = sPrefs.edit();
									editor.putString("gpuPref", "Software Renderer");
									editor.commit();
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
}
