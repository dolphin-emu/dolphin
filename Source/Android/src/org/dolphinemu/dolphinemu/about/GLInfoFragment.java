/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import android.app.ListFragment;
import android.opengl.GLES20;
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

import javax.microedition.khronos.opengles.GL10;

/**
 * {@link ListFragment} responsible for displaying
 * information relating to OpenGL.
 */
public final class GLInfoFragment extends ListFragment
{
	private final Limit[] Limits = {
			new Limit("Vendor", GL10.GL_VENDOR, Type.STRING),
			new Limit("Version", GL10.GL_VERSION, Type.STRING),
			new Limit("Renderer", GL10.GL_RENDERER, Type.STRING),
			new Limit("GLSL version", GLES20.GL_SHADING_LANGUAGE_VERSION, Type.STRING),
	};

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		final EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_BIT);

		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();

		for (Limit limit : Limits)
		{
			Log.i("GLInfoFragment", "Getting enum " + limit.name);
			Input.add(new AboutFragmentItem(limit.name, limit.GetValue(eglHelper)));
		}

		// Get extensions manually
		int numExtensions = eglHelper.glGetInteger(GLES30.GL_NUM_EXTENSIONS);
		StringBuilder extensionsBuilder = new StringBuilder();
		for (int i = 0; i < numExtensions; i++)
		{
			extensionsBuilder.append(eglHelper.glGetStringi(GL10.GL_EXTENSIONS, i)).append('\n');
		}
		Input.add(new AboutFragmentItem("OpenGL Extensions", extensionsBuilder.toString()));

		AboutInfoFragmentAdapter adapter = new AboutInfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
		rootView.setAdapter(adapter);

		return rootView;
	}
}
