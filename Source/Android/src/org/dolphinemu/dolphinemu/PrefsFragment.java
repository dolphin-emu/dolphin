package org.dolphinemu.dolphinemu;

import java.util.HashMap;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;

import javax.microedition.khronos.egl.*;
import javax.microedition.khronos.opengles.GL10;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public final class PrefsFragment extends PreferenceFragment
{
	private Activity m_activity;

	static public class VersionCheck
	{
		EGL10 mEGL;
		EGLDisplay mEGLDisplay;
		EGLConfig[] mEGLConfigs;
		EGLConfig mEGLConfig;
		EGLContext mEGLContext;
		EGLSurface mEGLSurface;
		GL10 mGL;

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

		public String getVersion()
		{
			return mGL.glGetString(GL10.GL_VERSION);
		}

		public String getVendor()
		{
			return mGL.glGetString(GL10.GL_VENDOR);
		}
		
		public String getRenderer()
		{
			return mGL.glGetString(GL10.GL_RENDERER);
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
	
	static public boolean SupportsGLES3()
	{
		VersionCheck mbuffer = new VersionCheck();
		String m_GLVersion = mbuffer.getVersion();
		String m_GLVendor = mbuffer.getVendor();
		String m_GLRenderer = mbuffer.getRenderer();

		boolean mSupportsGLES3 = false;

		// Check for OpenGL ES 3 support (General case).
		if (m_GLVersion.contains("OpenGL ES 3.0") || m_GLVersion.equals("OpenGL ES 3.0"))
			mSupportsGLES3 = true;
		
		// Checking for OpenGL ES 3 support for certain Qualcomm devices.
		if (!mSupportsGLES3 && m_GLVendor.equals("Qualcomm"))
		{
			if (m_GLRenderer.contains("Adreno (TM) 3"))
			{
				int mVStart = m_GLVersion.indexOf("V@") + 2;
				int mVEnd = 0;
				float mVersion;
				
				for (int a = mVStart; a < m_GLVersion.length(); ++a)
				{
					if (m_GLVersion.charAt(a) == ' ')
					{
						mVEnd = a;
						break;
					}
				}
				
				mVersion = Float.parseFloat(m_GLVersion.substring(mVStart, mVEnd));

				if (mVersion >= 14.0f)
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
        addPreferencesFromResource(R.layout.prefs);

        final ListPreference etp = new ListPreference(m_activity);
        final HashMap<CharSequence, CharSequence> entries = new HashMap<CharSequence, CharSequence>();

        if (Build.CPU_ABI.contains("x86"))
        {
	        entries.put(getString(R.string.interpreter), "0");
	        entries.put(getString(R.string.jit64_recompiler), "1");
	        entries.put(getString(R.string.jitil_recompiler), "2");
        }
        else if (Build.CPU_ABI.contains("arm"))
        {
	        entries.put(getString(R.string.interpreter), "0");
	        entries.put(getString(R.string.jit_arm_recompiler), "3");
        }
        else
        {
            entries.put(getString(R.string.interpreter), "0");
        }
        
        // Convert the key/value sections to arrays respectively so the list can be set.
        // If Java had proper generics it wouldn't look this disgusting.
        etp.setEntries(entries.keySet().toArray(new CharSequence[entries.size()]));
        etp.setEntryValues(entries.values().toArray(new CharSequence[entries.size()]));
        etp.setKey("cpupref");
        etp.setTitle(getString(R.string.cpu_core));
        etp.setSummary(getString(R.string.emu_core_to_use));

        PreferenceCategory mCategory = (PreferenceCategory) findPreference("cpuprefcat");
        mCategory.addPreference(etp);

	    boolean mSupportsGLES3 = SupportsGLES3();

        if (!mSupportsGLES3)
        {
	        mCategory = (PreferenceCategory) findPreference("videoprefcat");
	        ListPreference mPref = (ListPreference) findPreference("gpupref");
	        mCategory.removePreference(mPref);

	        final ListPreference videobackend = new ListPreference(m_activity);

	        // Add available graphics renderers to the hashmap to add to the list.
	        entries.clear();
	        entries.put(getString(R.string.software_renderer), "Software Renderer");

	        videobackend.setKey("gpupref");
	        videobackend.setTitle(getString(R.string.video_backend));
	        videobackend.setSummary(getString(R.string.video_backend_to_use));

	        videobackend.setEntries(entries.keySet().toArray(new CharSequence[entries.size()]));
	        videobackend.setEntryValues(entries.values().toArray(new CharSequence[entries.size()]));

	        mCategory.addPreference(videobackend);
        }
    }

	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);

		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try
		{
			m_activity = activity;
		}
		catch (ClassCastException e)
		{
			throw new ClassCastException(activity.toString()
					+ " must implement OnGameListZeroListener");
		}
	}
}
