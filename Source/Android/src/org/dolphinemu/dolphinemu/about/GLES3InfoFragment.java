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

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.sql.Struct;
import java.util.ArrayList;
import java.util.List;

import javax.microedition.khronos.opengles.GL10;

public class GLES3InfoFragment extends ListFragment {

    private EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES3_BIT_KHR);

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
            // GLES 3.0 limits
            new Limit("Max 3D Texture size", GLES30.GL_MAX_3D_TEXTURE_SIZE, TYPE_INTEGER),
            new Limit("Max Element Vertices", GLES30.GL_MAX_ELEMENTS_VERTICES, TYPE_INTEGER),
            new Limit("Max Element Indices", GLES30.GL_MAX_ELEMENTS_INDICES, TYPE_INTEGER),
            new Limit("Max Draw Buffers", GLES30.GL_MAX_DRAW_BUFFERS, TYPE_INTEGER),
            new Limit("Max Fragment Uniform Components", GLES30.GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, TYPE_INTEGER),
            new Limit("Max Vertex Uniform Components", GLES30.GL_MAX_VERTEX_UNIFORM_COMPONENTS, TYPE_INTEGER),
            new Limit("Number of Extensions", GLES30.GL_NUM_EXTENSIONS, TYPE_INTEGER),
            new Limit("Max Array Texture Layers", GLES30.GL_MAX_ARRAY_TEXTURE_LAYERS, TYPE_INTEGER),
            new Limit("Min Program Texel Offset", GLES30.GL_MIN_PROGRAM_TEXEL_OFFSET, TYPE_INTEGER),
            new Limit("Max Program Texel Offset", GLES30.GL_MAX_PROGRAM_TEXEL_OFFSET, TYPE_INTEGER),
            new Limit("Max Varying Components", GLES30.GL_MAX_VARYING_COMPONENTS, TYPE_INTEGER),
            new Limit("Max TF Varying Length", GLES30.GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, TYPE_INTEGER),
            new Limit("Max TF Separate Components", GLES30.GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, TYPE_INTEGER),
            new Limit("Max TF Interleaved Components", GLES30.GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, TYPE_INTEGER),
            new Limit("Max TF Separate Attributes", GLES30.GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, TYPE_INTEGER),
            new Limit("Max Color Attachments", GLES30.GL_MAX_COLOR_ATTACHMENTS, TYPE_INTEGER),
            new Limit("Max Samples", GLES30.GL_MAX_SAMPLES, TYPE_INTEGER),
            new Limit("Max Vertex UBOs", GLES30.GL_MAX_VERTEX_UNIFORM_BLOCKS, TYPE_INTEGER),
            new Limit("Max Fragment UBOs", GLES30.GL_MAX_FRAGMENT_UNIFORM_BLOCKS, TYPE_INTEGER),
            new Limit("Max Combined UBOs", GLES30.GL_MAX_COMBINED_UNIFORM_BLOCKS, TYPE_INTEGER),
            new Limit("Max Uniform Buffer Bindings", GLES30.GL_MAX_UNIFORM_BUFFER_BINDINGS, TYPE_INTEGER),
            new Limit("Max UBO Size", GLES30.GL_MAX_UNIFORM_BLOCK_SIZE, TYPE_INTEGER),
            new Limit("Max Combined Vertex Uniform Components", GLES30.GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, TYPE_INTEGER),
            new Limit("Max Combined Fragment Uniform Components", GLES30.GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, TYPE_INTEGER),
            new Limit("UBO Alignment", GLES30.GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, TYPE_INTEGER),
            new Limit("Max Vertex Output Components", GLES30.GL_MAX_VERTEX_OUTPUT_COMPONENTS, TYPE_INTEGER),
            new Limit("Max Fragment Input Components", GLES30.GL_MAX_FRAGMENT_INPUT_COMPONENTS, TYPE_INTEGER),
            new Limit("Max Server Wait Timeout", GLES30.GL_MAX_SERVER_WAIT_TIMEOUT, TYPE_INTEGER),
            new Limit("Program Binary Formats", GLES30.GL_NUM_PROGRAM_BINARY_FORMATS, TYPE_INTEGER),
            new Limit("Max Element Index", GLES30.GL_MAX_ELEMENT_INDEX, TYPE_INTEGER),
            new Limit("Sample Counts", GLES30.GL_NUM_SAMPLE_COUNTS, TYPE_INTEGER),

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
        int numExtensions = eglHelper.glGetInteger(GLES30.GL_NUM_EXTENSIONS);
        String ExtensionsString = "";
        for (int indx = 0; indx < numExtensions; ++indx)
            ExtensionsString += eglHelper.glGetStringi(GLES10.GL_EXTENSIONS, indx) + "\n";
        Input.add(new AboutActivity.AboutFragmentItem("OpenGL ES 3.0 Extensions", ExtensionsString));

        AboutActivity.InfoFragmentAdapter adapter = new AboutActivity.InfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
        rootView.setAdapter(adapter);

        return rootView;
    }
}
