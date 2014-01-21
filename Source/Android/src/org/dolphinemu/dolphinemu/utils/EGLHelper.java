package org.dolphinemu.dolphinemu.utils;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLES30;
import android.util.Log;

/**
 * Utility class that abstracts all the stuff about
 * EGL initialization out of the way if all that is 
 * wanted is to query the underlying GL API for information.
 */
public final class EGLHelper
{
	private final EGL10 mEGL;
	private final EGLDisplay mDisplay;
	private EGLConfig[] mEGLConfigs;
	private EGLContext mEGLContext;
	private EGLSurface mEGLSurface;
	private GL10 mGL;

	// GL support flags
	private boolean supportGL;
	private boolean supportGLES2;
	private boolean supportGLES3;

	// Renderable type bitmasks
	public static final int EGL_OPENGL_ES_BIT      = 0x0001;
	public static final int EGL_OPENGL_ES2_BIT     = 0x0004;
	public static final int EGL_OPENGL_BIT         = 0x0008;
	public static final int EGL_OPENGL_ES3_BIT_KHR = 0x0040;

	/**
	 * Constructor
	 * <p>
	 * Initializes the underlying {@link EGLSurface} with a width and height of 1.
	 * This is useful if all you need to use this class for is to query information
	 * from specific API contexts.
	 * 
	 * @param renderableType Bitmask indicating which types of client API contexts
	 *                       the framebuffer config must support.
	 */
	public EGLHelper(int renderableType)
	{
		this(1, 1, renderableType);
	}

	/**
	 * Constructor
	 * 
	 * @param width          Width of the underlying {@link EGLSurface}.
	 * @param height         Height of the underlying {@link EGLSurface}. 
	 * @param renderableType Bitmask indicating which types of client API contexts
	 *                       the framebuffer config must support.
	 */
	public EGLHelper(int width, int height, int renderableType)
	{
		// Initialize handle to an EGL display.
		mEGL = (EGL10) EGLContext.getEGL();
		mDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

		// If a display is present, initialize EGL.
		if (mDisplay != EGL10.EGL_NO_DISPLAY)
		{
			int[] version = new int[2];
			if (mEGL.eglInitialize(mDisplay, version))
			{
				// Detect supported GL APIs, initialize configs, etc.
				detect();

				// Create context and surface
				create(width, height, renderableType);
			}
			else
			{
				Log.e("EGLHelper", "Error initializing EGL.");
			}
		}
		else
		{
			Log.e("EGLHelper", "Error initializing EGL display.");
		}
	}

	/**
	 * Releases all resources associated with this helper.
	 * <p>
	 * This should be called whenever this helper is no longer needed. 
	 */
	public void closeHelper()
	{
		mEGL.eglTerminate(mDisplay);
	}

	/**
	 * Gets information through EGL.<br/>
	 * <p>
	 * Index 0: Vendor     <br/>
	 * Index 1: Version    <br/>
	 * Index 2: Renderer   <br/>
	 * Index 3: Extensions <br/>
	 * 
	 * @return information retrieved through EGL.
	 */
	public String[] getEGLInfo()
	{
		String[] info = {
			mGL.glGetString(GL10.GL_VENDOR),
			mGL.glGetString(GL10.GL_VERSION),
			mGL.glGetString(GL10.GL_RENDERER),
			mGL.glGetString(GL10.GL_EXTENSIONS),
		};

		return info;
	}

	/**
	 * Whether or not this device supports OpenGL.
	 * 
	 * @return true if this device supports OpenGL; false otherwise.
	 */
	public boolean supportsOpenGL()
	{
		return supportGL;
	}

	/**
	 * Whether or not this device supports OpenGL ES 2.
	 * <br/>
	 * Note that if this returns true, then OpenGL ES 1 is also supported.
	 * 
	 * @return true if this device supports OpenGL ES 2; false otherwise.
	 */
	public boolean supportsGLES2()
	{
		return supportGLES2;
	}

	/**
	 * Whether or not this device supports OpenGL ES 3.
	 * <br/>
	 * Note that if this returns true, then OpenGL ES 1 and 2 are also supported.
	 * 
	 * @return true if this device supports OpenGL ES 3; false otherwise.
	 */
	public boolean supportsGLES3()
	{
		return supportGLES3;
	}

	/**
	 * Gets the underlying {@link EGL10} instance.
	 * 
	 * @return the underlying {@link EGL10} instance.
	 */
	public EGL10 getEGL()
	{
		return mEGL;
	}

	/**
	 * Gets the underlying {@link GL10} instance.
	 * 
	 * @return the underlying {@link GL10} instance.
	 */
	public GL10 getGL()
	{
		return mGL;
	}

	/**
	 * Gets the underlying {@link EGLDisplay}.
	 * 
	 * @return the underlying {@link EGLDisplay}
	 */
	public EGLDisplay getDisplay()
	{
		return mDisplay;
	}

	/**
	 * Gets all supported framebuffer configurations for this device.
	 * 
	 * @return all supported framebuffer configurations for this device.
	 */
	public EGLConfig[] getConfigs()
	{
		return mEGLConfigs;
	}

	/**
	 * Gets the underlying {@link EGLContext}.
	 * 
	 * @return the underlying {@link EGLContext}.
	 */
	public EGLContext getContext()
	{
		return mEGLContext;
	}

	/**
	 * Gets the underlying {@link EGLSurface}.
	 * 
	 * @return the underlying {@link EGLSurface}.
	 */
	public EGLSurface getSurface()
	{
		return mEGLSurface;
	}

	// Detects the specific kind of GL modes that are supported
	private boolean detect()
	{
		// Attributes for a visual in RGBA format with at least 8 bits per color.
		int[] attribs = {
			EGL10.EGL_RED_SIZE, 8,
			EGL10.EGL_GREEN_SIZE, 8,
			EGL10.EGL_BLUE_SIZE, 8,
			EGL10.EGL_NONE
		};

		// Get total number of configs available.
		int[] numConfigs = new int[1];
		if (!mEGL.eglChooseConfig(mDisplay, attribs, null, 0, numConfigs))
		{
			Log.e("EGLHelper", "Error retrieving number of EGL configs available.");
			return false;
		}

		// Now get all the configurations
		mEGLConfigs = new EGLConfig[numConfigs[0]];
		if (!mEGL.eglChooseConfig(mDisplay, attribs, mEGLConfigs, mEGLConfigs.length, numConfigs))
		{
			Log.e("EGLHelper", "Error retrieving all EGL configs.");
			return false;
		}

		for (int i = 0; i < mEGLConfigs.length; i++)
		{
			int[] attribVal = new int[1];
			boolean ret = mEGL.eglGetConfigAttrib(mDisplay, mEGLConfigs[i], EGL10.EGL_RENDERABLE_TYPE, attribVal);
			if (ret)
			{
				if ((attribVal[0] & EGL_OPENGL_BIT) != 0)
					supportGL = true;

				if ((attribVal[0] & EGL_OPENGL_ES2_BIT) != 0)
					supportGLES2 = true;

				if ((attribVal[0] & EGL_OPENGL_ES3_BIT_KHR) != 0)
					supportGLES3 = true;
			}
		}

		return true;
	}

	// Creates the context and surface.
	private void create(int width, int height, int renderableType)
	{
		int[] attribs = {
			EGL10.EGL_WIDTH, width,
			EGL10.EGL_HEIGHT, height,
			EGL10.EGL_RENDERABLE_TYPE, renderableType,
			EGL10.EGL_NONE
		};

		// Initially we just assume GLES2 will be the default context.
		int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
		int[] ctx_attribs = {
			EGL_CONTEXT_CLIENT_VERSION, 2,
			EGL10.EGL_NONE
		};

		// Determine the type of context that will be created
		// and change the attribute arrays accordingly.
		switch (renderableType)
		{
			case EGL_OPENGL_ES_BIT:
				attribs[5] = EGL_OPENGL_ES_BIT;
				ctx_attribs[1] = 1;
				break;

			case EGL_OPENGL_BIT:
				attribs[5] = EGL_OPENGL_BIT;
				ctx_attribs[0] = EGL10.EGL_NONE;
				break;

			case EGL_OPENGL_ES3_BIT_KHR:
				attribs[5] = EGL_OPENGL_ES3_BIT_KHR;
				ctx_attribs[1] = 3;
				break;

			case EGL_OPENGL_ES2_BIT:
			default: // Fall-back to GLES 2.
				attribs[5] = EGL_OPENGL_ES2_BIT;
				ctx_attribs[1] = 2;
				break;
		}

		mEGLContext = mEGL.eglCreateContext(mDisplay, mEGLConfigs[0], EGL10.EGL_NO_CONTEXT, ctx_attribs);
		mEGLSurface = mEGL.eglCreatePbufferSurface(mDisplay, mEGLConfigs[0], attribs);
		mEGL.eglMakeCurrent(mDisplay, mEGLSurface, mEGLSurface, mEGLContext);
		mGL = (GL10) mEGLContext.getGL();
	}

    public String glGetString(int glEnum)
    {
        return mGL.glGetString(glEnum);
    }

    public String glGetStringi(int glEnum, int index)
    {
        return GLES30.glGetStringi(glEnum, index);
    }

    public int glGetInteger(int glEnum)
    {
        int[] val = new int[1];
        mGL.glGetIntegerv(glEnum, val, 0);
        return val[0];
    }
}
