/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL10;

import org.dolphinemu.dolphinemu.R;

import android.app.Activity;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceFragment;

/**
 * Responsible for handling the loading of the video preferences.
 */
public final class VideoSettingsFragment extends PreferenceFragment
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
        if (m_GLVersion != null && (m_GLVersion.contains("OpenGL ES 3.0") || m_GLVersion.equals("OpenGL ES 3.0")))
            mSupportsGLES3 = true;
        
        // Checking for OpenGL ES 3 support for certain Qualcomm devices.
        if (!mSupportsGLES3 && m_GLVendor != null && m_GLVendor.equals("Qualcomm"))
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
            throw new ClassCastException(activity.toString());
        }
    }
    
    @Override
    public void onDestroy()
    {
        super.onDestroy();
        
        // When the fragment is done being used, save the settings to the Dolphin ini file.
        UserPreferences.SaveConfigToDolphinIni(m_activity);
    }
}
