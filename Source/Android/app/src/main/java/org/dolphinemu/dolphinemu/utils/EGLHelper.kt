/*
 * Copyright 2013 Dolphin Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

package org.dolphinemu.dolphinemu.utils

import android.opengl.GLES30

import org.dolphinemu.dolphinemu.NativeLibrary

import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.egl.EGLContext
import javax.microedition.khronos.egl.EGLDisplay
import javax.microedition.khronos.egl.EGLSurface

/**
 * Utility class that abstracts all the stuff about
 * EGL initialization out of the way if all that is
 * wanted is to query the underlying GL API for information.
 */
class EGLHelper(width: Int, height: Int, renderableType: Int) {
    private val mEGL: EGL10 = EGLContext.getEGL() as EGL10
    private val mDisplay: EGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY)
    private lateinit var mEGLConfigs: Array<EGLConfig>
    private lateinit var mEGLContext: EGLContext
    private lateinit var mEGLSurface: EGLSurface

    // GL support flags
    private var supportGL = false
    private var supportGLES3 = false

    /**
     * Constructor
     *
     * Initializes the underlying [EGLSurface] with a width and height of 1.
     * This is useful if all you need to use this class for is to query information
     * from specific API contexts.
     *
     * @param renderableType Bitmask indicating which types of client API contexts
     * the framebuffer config must support.
     */
    constructor(renderableType: Int) : this(1, 1, renderableType)

    init {
        // If a display is present, initialize EGL.
        if (mDisplay != EGL10.EGL_NO_DISPLAY) {
            val version = IntArray(2)
            if (mEGL.eglInitialize(mDisplay, version)) {
                // Detect supported GL APIs, initialize configs, etc.
                detect()

                // Create context and surface
                create(width, height, renderableType)
            } else {
                Log.error("[EGLHelper] Error initializing EGL.")
            }
        } else {
            Log.error("[EGLHelper] Error initializing EGL display.")
        }
    }

    /**
     * Whether or not this device supports OpenGL.
     *
     * @return true if this device supports OpenGL; false otherwise.
     */
    fun supportsOpenGL(): Boolean {
        return supportGL
    }

    /**
     * Whether or not this device supports OpenGL ES 3.
     *
     * Note that if this returns true, then OpenGL ES 1 and 2 are also supported.
     *
     * @return true if this device supports OpenGL ES 3; false otherwise.
     */
    fun supportsGLES3(): Boolean {
        return supportGLES3
    }

    // Detects the specific kind of GL modes that are supported
    private fun detect() {
        // Get total number of configs available.
        val numConfigs = IntArray(1)
        if (!mEGL.eglGetConfigs(mDisplay, null, 0, numConfigs)) {
            Log.error("[EGLHelper] Error retrieving number of EGL configs available.")
            return
        }

        // Now get all the configurations
        val eglConfigs = arrayOfNulls<EGLConfig>(numConfigs[0])
        if (!mEGL.eglGetConfigs(mDisplay, eglConfigs, eglConfigs.size, numConfigs)) {
            Log.error("[EGLHelper] Error retrieving all EGL configs.")
            return
        }
        mEGLConfigs = Array(eglConfigs.size) { i -> eglConfigs[i]!! }

        for (eglConfig in mEGLConfigs) {
            val attribVal = IntArray(1)
            val ret =
                mEGL.eglGetConfigAttrib(mDisplay, eglConfig, EGL10.EGL_RENDERABLE_TYPE, attribVal)
            if (ret) {
                if ((attribVal[0] and EGL_OPENGL_BIT) != 0) supportGL = true

                if ((attribVal[0] and EGL_OPENGL_ES3_BIT_KHR) != 0) supportGLES3 = true
            }
        }
    }

    // Creates the context and surface.
    private fun create(width: Int, height: Int, renderableType: Int) {
        val attribs = intArrayOf(
            EGL10.EGL_WIDTH, width, EGL10.EGL_HEIGHT, height, EGL10.EGL_NONE
        )

        // Initially we just assume GLES2 will be the default context.
        val eglContextClientVersion = 0x3098
        val ctxAttribs = intArrayOf(
            eglContextClientVersion, 2, EGL10.EGL_NONE
        )

        // Determine the type of context that will be created
        // and change the attribute arrays accordingly.
        when (renderableType) {
            EGL_OPENGL_ES_BIT -> ctxAttribs[1] = 1
            EGL_OPENGL_BIT -> ctxAttribs[0] = EGL10.EGL_NONE
            EGL_OPENGL_ES3_BIT_KHR -> ctxAttribs[1] = 3
            EGL_OPENGL_ES2_BIT -> ctxAttribs[1] = 2
            else -> ctxAttribs[1] = 2 // Fall-back to GLES 2.
        }
        if (renderableType == EGL_OPENGL_BIT) {
            NativeLibrary.eglBindAPI(EGL_OPENGL_API)
        } else {
            NativeLibrary.eglBindAPI(EGL_OPENGL_ES_API)
        }

        mEGLContext =
            mEGL.eglCreateContext(mDisplay, mEGLConfigs[0], EGL10.EGL_NO_CONTEXT, ctxAttribs)
        mEGLSurface = mEGL.eglCreatePbufferSurface(mDisplay, mEGLConfigs[0], attribs)
        mEGL.eglMakeCurrent(mDisplay, mEGLSurface, mEGLSurface, mEGLContext)
    }

    fun supportsExtension(extension: String): Boolean {
        val numExt = IntArray(1)
        GLES30.glGetIntegerv(GLES30.GL_NUM_EXTENSIONS, numExt, 0)

        for (i in 0 until numExt[0]) {
            val ext = GLES30.glGetStringi(GLES30.GL_EXTENSIONS, i)
            if (ext.equals(extension)) return true
        }
        return false
    }

    fun getVersion(): Int {
        val major = IntArray(1)
        val minor = IntArray(1)
        GLES30.glGetIntegerv(GLES30.GL_MAJOR_VERSION, major, 0)
        GLES30.glGetIntegerv(GLES30.GL_MINOR_VERSION, minor, 0)
        return major[0] * 100 + minor[0] * 10
    }

    companion object {
        // Renderable type bitmasks
        const val EGL_OPENGL_ES_BIT = 0x0001
        const val EGL_OPENGL_ES2_BIT = 0x0004
        const val EGL_OPENGL_BIT = 0x0008
        const val EGL_OPENGL_ES3_BIT_KHR = 0x0040

        // API types
        const val EGL_OPENGL_ES_API = 0x30A0
        const val EGL_OPENGL_API = 0x30A2
    }
}
