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

public class GLInfoFragment extends ListFragment {

    private EGLHelper eglHelper = new EGLHelper(EGLHelper.EGL_OPENGL_BIT);

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
        Input.add(new AboutActivity.AboutFragmentItem("OpenGL Extensions", ExtensionsString));

        AboutActivity.InfoFragmentAdapter adapter = new AboutActivity.InfoFragmentAdapter(getActivity(), R.layout.about_layout, Input);
        rootView.setAdapter(adapter);

        return rootView;
    }
}
