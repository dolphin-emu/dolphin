/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import android.app.ListFragment;
import android.opengl.GLES30;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.about.Limit.Type;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link ListFragment} responsible for displaying
 * information relating to OpenGL ES 3.
 */
public final class GLES3InfoFragment extends ListFragment
{
	private final Limit[] Limits = {
			new Limit("Vendor", GLES30.GL_VENDOR, Type.STRING),
			new Limit("Version", GLES30.GL_VERSION, Type.STRING),
			new Limit("Renderer", GLES30.GL_RENDERER, Type.STRING),
			new Limit("GLSL version", GLES30.GL_SHADING_LANGUAGE_VERSION, Type.STRING),
			// GLES 2.0 Limits
			new Limit("Aliased Point Size", GLES30.GL_ALIASED_POINT_SIZE_RANGE, Type.INTEGER_RANGE),
			new Limit("Aliased Line Width ", GLES30.GL_ALIASED_LINE_WIDTH_RANGE, Type.INTEGER_RANGE),
			new Limit("Max Texture Size", GLES30.GL_MAX_TEXTURE_SIZE, Type.INTEGER),
			new Limit("Viewport Dimensions", GLES30.GL_MAX_VIEWPORT_DIMS, Type.INTEGER_RANGE),
			new Limit("Subpixel Bits", GLES30.GL_SUBPIXEL_BITS, Type.INTEGER),
			new Limit("Max Vertex Attributes", GLES30.GL_MAX_VERTEX_ATTRIBS, Type.INTEGER),
			new Limit("Max Vertex Uniform Vectors", GLES30.GL_MAX_VERTEX_UNIFORM_VECTORS, Type.INTEGER),
			new Limit("Max Varying Vectors", GLES30.GL_MAX_VARYING_VECTORS, Type.INTEGER),
			new Limit("Max Combined Texture Units", GLES30.GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Vertex Texture Units", GLES30.GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Texture Units", GLES30.GL_MAX_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Fragment Uniform Vectors", GLES30.GL_MAX_FRAGMENT_UNIFORM_VECTORS, Type.INTEGER),
			new Limit("Max Cubemap Texture Size", GLES30.GL_MAX_CUBE_MAP_TEXTURE_SIZE, Type.INTEGER),
			new Limit("Shader Binary Formats", GLES30.GL_NUM_SHADER_BINARY_FORMATS, Type.INTEGER),
			new Limit("Max Framebuffer Size", GLES30.GL_MAX_RENDERBUFFER_SIZE, Type.INTEGER),
			// GLES 3.0 limits
			new Limit("Max 3D Texture size", GLES30.GL_MAX_3D_TEXTURE_SIZE, Type.INTEGER),
			new Limit("Max Element Vertices", GLES30.GL_MAX_ELEMENTS_VERTICES, Type.INTEGER),
			new Limit("Max Element Indices", GLES30.GL_MAX_ELEMENTS_INDICES, Type.INTEGER),
			new Limit("Max Draw Buffers", GLES30.GL_MAX_DRAW_BUFFERS, Type.INTEGER),
			new Limit("Max Fragment Uniform Components", GLES30.GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, Type.INTEGER),
			new Limit("Max Vertex Uniform Components", GLES30.GL_MAX_VERTEX_UNIFORM_COMPONENTS, Type.INTEGER),
			new Limit("Number of Extensions", GLES30.GL_NUM_EXTENSIONS, Type.INTEGER),
			new Limit("Max Array Texture Layers", GLES30.GL_MAX_ARRAY_TEXTURE_LAYERS, Type.INTEGER),
			new Limit("Min Program Texel Offset", GLES30.GL_MIN_PROGRAM_TEXEL_OFFSET, Type.INTEGER),
			new Limit("Max Program Texel Offset", GLES30.GL_MAX_PROGRAM_TEXEL_OFFSET, Type.INTEGER),
			new Limit("Max Varying Components", GLES30.GL_MAX_VARYING_COMPONENTS, Type.INTEGER),
			new Limit("Max TF Varying Length", GLES30.GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, Type.INTEGER),
			new Limit("Max TF Separate Components", GLES30.GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, Type.INTEGER),
			new Limit("Max TF Interleaved Components", GLES30.GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, Type.INTEGER),
			new Limit("Max TF Separate Attributes", GLES30.GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, Type.INTEGER),
			new Limit("Max Color Attachments", GLES30.GL_MAX_COLOR_ATTACHMENTS, Type.INTEGER),
			new Limit("Max Samples", GLES30.GL_MAX_SAMPLES, Type.INTEGER),
			new Limit("Max Vertex UBOs", GLES30.GL_MAX_VERTEX_UNIFORM_BLOCKS, Type.INTEGER),
			new Limit("Max Fragment UBOs", GLES30.GL_MAX_FRAGMENT_UNIFORM_BLOCKS, Type.INTEGER),
			new Limit("Max Combined UBOs", GLES30.GL_MAX_COMBINED_UNIFORM_BLOCKS, Type.INTEGER),
			new Limit("Max Uniform Buffer Bindings", GLES30.GL_MAX_UNIFORM_BUFFER_BINDINGS, Type.INTEGER),
			new Limit("Max UBO Size", GLES30.GL_MAX_UNIFORM_BLOCK_SIZE, Type.INTEGER),
			new Limit("Max Combined Vertex Uniform Components", GLES30.GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, Type.INTEGER),
			new Limit("Max Combined Fragment Uniform Components", GLES30.GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, Type.INTEGER),
			new Limit("UBO Alignment", GLES30.GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, Type.INTEGER),
			new Limit("Max Vertex Output Components", GLES30.GL_MAX_VERTEX_OUTPUT_COMPONENTS, Type.INTEGER),
			new Limit("Max Fragment Input Components", GLES30.GL_MAX_FRAGMENT_INPUT_COMPONENTS, Type.INTEGER),
			new Limit("Max Server Wait Timeout", GLES30.GL_MAX_SERVER_WAIT_TIMEOUT, Type.INTEGER),
			new Limit("Program Binary Formats", GLES30.GL_NUM_PROGRAM_BINARY_FORMATS, Type.INTEGER),
			new Limit("Max Element Index", GLES30.GL_MAX_ELEMENT_INDEX, Type.INTEGER),
			new Limit("Sample Counts", GLES30.GL_NUM_SAMPLE_COUNTS, Type.INTEGER),
	};

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES3_BIT_KHR);

		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();

		for (Limit limit : Limits)
		{
			Log.i("GLES3InfoFragment", "Getting enum " + limit.name);
			Input.add(new AboutFragmentItem(limit.name, limit.GetValue(eglHelper)));
		}

		// Get extensions manually
		int numExtensions = eglHelper.glGetInteger(GLES30.GL_NUM_EXTENSIONS);
		StringBuilder extensionsBuilder = new StringBuilder();
		for (int i = 0; i < numExtensions; i++)
		{
			extensionsBuilder.append(eglHelper.glGetStringi(GLES30.GL_EXTENSIONS, i)).append('\n');
		}
		Input.add(new AboutFragmentItem("OpenGL ES 3.0 Extensions", extensionsBuilder.toString()));

		AboutInfoFragmentAdapter adapter = new AboutInfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
		rootView.setAdapter(adapter);

		return rootView;
	}
}
