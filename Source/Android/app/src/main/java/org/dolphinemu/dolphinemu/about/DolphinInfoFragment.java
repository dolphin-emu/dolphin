/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import android.app.ListFragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.EGLHelper;

/**
 * Represents the about screen.
 */
public final class DolphinInfoFragment extends ListFragment
{
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

		final String yes = getString(R.string.yes);
		final String no = getString(R.string.no);

		List<AboutFragmentItem> Input = new ArrayList<AboutFragmentItem>();
		Input.add(new AboutFragmentItem(getString(R.string.build_revision), NativeLibrary.GetVersionString()));
		Input.add(new AboutFragmentItem(getString(R.string.supports_gles3), eglHelper.supportsGLES3() ? yes : no));
		Input.add(new AboutFragmentItem(getString(R.string.supports_neon),  NativeLibrary.SupportsNEON() ? yes : no));

		AboutInfoFragmentAdapter adapter = new AboutInfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
		rootView.setAdapter(adapter);
		rootView.setEnabled(false);  // Makes the list view non-clickable.

		return rootView;
	}
}
