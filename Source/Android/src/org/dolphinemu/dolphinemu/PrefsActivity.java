package org.dolphinemu.dolphinemu;

import android.app.Activity;
import android.content.Intent;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;

import javax.microedition.khronos.egl.*;
import javax.microedition.khronos.opengles.GL10;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class PrefsActivity extends PreferenceActivity {
	private PrefsActivity m_activity;
	private String m_GLVersion;
	private String m_GLVendor;
	private String m_GLRenderer;


	public class VersionCheck {
		GLSurfaceView.Renderer mRenderer; // borrow this interface

		EGL10 mEGL;
		EGLDisplay mEGLDisplay;
		EGLConfig[] mEGLConfigs;
		EGLConfig mEGLConfig;
		EGLContext mEGLContext;
		EGLSurface mEGLSurface;
		GL10 mGL;

		String mThreadOwner;

		public VersionCheck() {

			int[] version = new int[2];
			int[] attribList = new int[] {
					EGL10.EGL_WIDTH, 1,
					EGL10.EGL_HEIGHT, 1,
					EGL10.EGL_RENDERABLE_TYPE, 4,
					EGL10.EGL_NONE
			};

			// No error checking performed, minimum required code to elucidate logic
			mEGL = (EGL10) EGLContext.getEGL();
			mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
			mEGL.eglInitialize(mEGLDisplay, version);
			mEGLConfig = chooseConfig(); // Choosing a config is a little more complicated
			mEGLContext = mEGL.eglCreateContext(mEGLDisplay, mEGLConfig, EGL10.EGL_NO_CONTEXT, null);
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
		private EGLConfig chooseConfig() {
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



    public class PrefsFragment extends PreferenceFragment {

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // Load the preferences from an XML resource
            addPreferencesFromResource(R.layout.prefs);

	        final ListPreference etp = new ListPreference(m_activity);
	        CharSequence[] _entries;
	        CharSequence[] _entryvalues;

	        if (Build.CPU_ABI.contains("x86"))
	        {
		        _entries = new CharSequence[] {
			        "Interpreter",
			        "JIT64 Recompiler",
			        "JITIL Recompiler",
		        };
		        _entryvalues = new CharSequence[] {"0", "1", "2"};
	        }
	        else if (Build.CPU_ABI.contains("arm"))
	        {
		        _entries = new CharSequence[] {
				        "Interpreter",
				        "JIT ARM Recompiler",
		        };
		        _entryvalues = new CharSequence[] {"0", "3"};
	        }
	        else
	        {
		        _entries = new CharSequence[] {
				        "Interpreter",
		        };
		        _entryvalues = new CharSequence[] {"0"};
	        }

	        etp.setEntries(_entries);
	        etp.setEntryValues(_entryvalues);
	        etp.setKey("cpupref");
	        etp.setTitle("CPU Core");
	        etp.setSummary("Emulation core to use");

	        PreferenceCategory mCategory = (PreferenceCategory) findPreference("cpuprefcat");
	        mCategory.addPreference(etp);
	        VersionCheck mbuffer = new VersionCheck();
	        m_GLVersion = mbuffer.getVersion();
	        m_GLVendor = mbuffer.getVendor();
	        m_GLRenderer = mbuffer.getRenderer();

	        boolean mSupportsGLES3 = false;

	        if (m_GLVersion.contains("OpenGL ES 3.0")) // 3.0 support
	            mSupportsGLES3 = true;
			if (!mSupportsGLES3 && m_GLVendor.equals("Qualcomm"))
	        {
		        if (m_GLRenderer.contains("Adreno (TM) 3"))
		        {
			        int mVStart, mVEnd = 0;
			        float mVersion;
			        mVStart = m_GLVersion.indexOf("V@") + 2;
			        for (int a = mVStart; a < m_GLVersion.length(); ++a)
				        if (m_GLVersion.charAt(a) == ' ')
				        {
					        mVEnd = a;
					        break;
				        }
			        mVersion = Float.parseFloat(m_GLVersion.substring(mVStart, mVEnd));

			        if (mVersion >= 14.0f)
				        mSupportsGLES3 = true;
		        }
	        }

	        if (!mSupportsGLES3)
	        {
		        mCategory = (PreferenceCategory) findPreference("videoprefcat");
		        ListPreference mPref = (ListPreference) findPreference("gpupref");
		        mCategory.removePreference(mPref);

		        final ListPreference videobackend = new ListPreference(m_activity);

		        _entries = new CharSequence[] {
				        "Software Renderer",
		        };
		        _entryvalues = new CharSequence[] {"Software Renderer"};

		        videobackend.setKey("gpupref");
		        videobackend.setTitle("Video Backend");
		        videobackend.setSummary("Video backend to use");

		        videobackend.setEntries(_entries);
		        videobackend.setEntryValues(_entryvalues);

		        mCategory.addPreference(videobackend);
	        }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

	    m_activity = this;

        getFragmentManager().beginTransaction().replace(android.R.id.content,
                new PrefsFragment()).commit();
    }
    @Override
    public void onBackPressed() {
        Intent intent = new Intent();
        setResult(Activity.RESULT_OK, intent);
        this.finish();
        super.onBackPressed();
    }
}
