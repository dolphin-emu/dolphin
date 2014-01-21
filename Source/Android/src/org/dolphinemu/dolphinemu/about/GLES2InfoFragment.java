/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import android.app.ListFragment;
import android.opengl.GLES10;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.util.ArrayList;
import java.util.List;

import javax.microedition.khronos.opengles.GL10;

public class GLES2InfoFragment extends ListFragment {

    private EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

    public static final int TYPE_STRING = 0;
    public static final int TYPE_INTEGER = 1;
    public static final int TYPE_INTEGER_RANGE = 2;

    class Limit
    {
        public final String name;
        public final int glEnum;
        public final int type;

        public Limit(String name, int glEnum, int type)
        {
            this.name = name;
            this.glEnum = glEnum;
            this.type = type;
        }
        public String GetValue()
        {
            if (type == TYPE_INTEGER)
                return Integer.toString(eglHelper.glGetInteger(glEnum));
            return eglHelper.glGetString(glEnum);
        }
    }

    private final Limit Limits[] = {
            new Limit("Vendor", GL10.GL_VENDOR, TYPE_STRING),
            new Limit("Version", GL10.GL_VERSION, TYPE_STRING),
            new Limit("Renderer", GL10.GL_RENDERER, TYPE_STRING),
            new Limit("GLSL version", GLES20.GL_SHADING_LANGUAGE_VERSION, TYPE_STRING),
            // GLES 2.0 Limits
            //new Limit("Aliased Point Size", GLES20.GL_ALIASED_POINT_SIZE_RANGE, TYPE_INTEGER_RANGE),
            //new Limit("Aliased Line Width ", GLES20.GL_ALIASED_LINE_WIDTH_RANGE, TYPE_INTEGER_RANGE),
            new Limit("Max Texture Size", GLES20.GL_MAX_TEXTURE_SIZE, TYPE_INTEGER),
            //new Limit("Viewport Dimensions", GLES20.GL_MAX_VIEWPORT_DIMS, TYPE_INTEGER_RANGE),
            new Limit("Subpixel Bits", GLES20.GL_SUBPIXEL_BITS, TYPE_INTEGER),
            new Limit("Max Vertex Attributes", GLES20.GL_MAX_VERTEX_ATTRIBS, TYPE_INTEGER),
            new Limit("Max Vertex Uniform Vectors", GLES20.GL_MAX_VERTEX_UNIFORM_VECTORS, TYPE_INTEGER),
            new Limit("Max Varying Vectors", GLES20.GL_MAX_VARYING_VECTORS, TYPE_INTEGER),
            new Limit("Max Combined Texture Units", GLES20.GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, TYPE_INTEGER),
            new Limit("Max Vertex Texture Units", GLES20.GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, TYPE_INTEGER),
            new Limit("Max Texture Units", GLES20.GL_MAX_TEXTURE_IMAGE_UNITS, TYPE_INTEGER),
            new Limit("Max Fragment Uniform Vectors", GLES20.GL_MAX_FRAGMENT_UNIFORM_VECTORS, TYPE_INTEGER),
            new Limit("Max Cubemap Texture Size", GLES20.GL_MAX_CUBE_MAP_TEXTURE_SIZE, TYPE_INTEGER),
            new Limit("Shader Binary Formats", GLES20.GL_NUM_SHADER_BINARY_FORMATS, TYPE_INTEGER),
            new Limit("Max Framebuffer Size", GLES20.GL_MAX_RENDERBUFFER_SIZE, TYPE_INTEGER),
    };

    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
    {
        ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);


        List<AboutActivity.AboutFragmentItem> Input = new ArrayList<AboutActivity.AboutFragmentItem>();

        for(Limit limit : Limits)
        {
            Log.w("Dolphinemu", "Getting enum " + limit.name);
            Input.add(new AboutActivity.AboutFragmentItem(limit.name, limit.GetValue()));
        }

        // Get extensions manually
        String ExtensionsString = eglHelper.glGetString(GLES20.GL_EXTENSIONS);
        String Extensions[] = ExtensionsString.split(" ");
        String FinalExtensions = "";
        for (String Extension : Extensions)
            FinalExtensions += Extension + "\n";
        Input.add(new AboutActivity.AboutFragmentItem("OpenGL ES 2.0 Extensions", FinalExtensions));

        AboutActivity.InfoFragmentAdapter adapter = new AboutActivity.InfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
        rootView.setAdapter(adapter);

        return rootView;
    }
}
