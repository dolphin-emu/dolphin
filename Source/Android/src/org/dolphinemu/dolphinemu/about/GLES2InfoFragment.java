/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import android.app.ListFragment;
import android.opengl.GLES20;
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
 * information relating to OpenGL ES 2.
 */
public final class GLES2InfoFragment extends ListFragment
{
	private final Limit[] Limits = {
			new Limit("Vendor", GLES20.GL_VENDOR, Type.STRING),
			new Limit("Version", GLES20.GL_VERSION, Type.STRING),
			new Limit("Renderer", GLES20.GL_RENDERER, Type.STRING),
			new Limit("GLSL version", GLES20.GL_SHADING_LANGUAGE_VERSION, Type.STRING),
			// GLES 2.0 Limits
			new Limit("Aliased Point Size", GLES20.GL_ALIASED_POINT_SIZE_RANGE, Type.INTEGER_RANGE),
			new Limit("Aliased Line Width ", GLES20.GL_ALIASED_LINE_WIDTH_RANGE, Type.INTEGER_RANGE),
			new Limit("Max Texture Size", GLES20.GL_MAX_TEXTURE_SIZE, Type.INTEGER),
			new Limit("Viewport Dimensions", GLES20.GL_MAX_VIEWPORT_DIMS, Type.INTEGER_RANGE),
			new Limit("Subpixel Bits", GLES20.GL_SUBPIXEL_BITS, Type.INTEGER),
			new Limit("Max Vertex Attributes", GLES20.GL_MAX_VERTEX_ATTRIBS, Type.INTEGER),
			new Limit("Max Vertex Uniform Vectors", GLES20.GL_MAX_VERTEX_UNIFORM_VECTORS, Type.INTEGER),
			new Limit("Max Varying Vectors", GLES20.GL_MAX_VARYING_VECTORS, Type.INTEGER),
			new Limit("Max Combined Texture Units", GLES20.GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Vertex Texture Units", GLES20.GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Texture Units", GLES20.GL_MAX_TEXTURE_IMAGE_UNITS, Type.INTEGER),
			new Limit("Max Fragment Uniform Vectors", GLES20.GL_MAX_FRAGMENT_UNIFORM_VECTORS, Type.INTEGER),
			new Limit("Max Cubemap Texture Size", GLES20.GL_MAX_CUBE_MAP_TEXTURE_SIZE, Type.INTEGER),
			new Limit("Shader Binary Formats", GLES20.GL_NUM_SHADER_BINARY_FORMATS, Type.INTEGER),
			new Limit("Max Framebuffer Size", GLES20.GL_MAX_RENDERBUFFER_SIZE, Type.INTEGER),
	};

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();

		for (Limit limit : Limits)
		{
			Log.i("GLES2InfoFragment", "Getting enum " + limit.name);
			Input.add(new AboutFragmentItem(limit.name, limit.GetValue(eglHelper)));
		}

		// Get extensions manually
		String[] extensions = eglHelper.glGetString(GLES20.GL_EXTENSIONS).split(" ");
		StringBuilder extensionsBuilder = new StringBuilder();
		for (String extension : extensions)
		{
			extensionsBuilder.append(extension).append('\n');
		}
		Input.add(new AboutFragmentItem("OpenGL ES 2.0 Extensions", extensionsBuilder.toString()));

		AboutInfoFragmentAdapter adapter = new AboutInfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
		rootView.setAdapter(adapter);

		return rootView;
	}
}
