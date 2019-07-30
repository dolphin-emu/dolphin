/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package com.nononsenseapps.filepicker;


import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;

import org.dolphinemu.dolphinemu.R;

import java.util.ArrayList;
import java.util.List;

/**
 * An abstract base activity that handles all the fluff you don't care about.
 * <p/>
 * Usage: To start a child activity you could either use an intent starting the
 * activity directly, or you could use an implicit intent with GET_CONTENT, if
 * it
 * is also defined in your manifest. It is defined to be handled here in case
 * you
 * want the user to be able to use other file pickers on the system.
 * <p/>
 * That means using an intent with action GET_CONTENT
 * If you want to be able to select multiple items, include EXTRA_ALLOW_MULTIPLE
 * (default false).
 * <p/>
 * Some non-standard extra arguments are supported as well:
 * EXTRA_ONLY_DIRS - (default false) allows only directories to be selected.
 * EXTRA_START_PATH - (default null) which should specify the starting path.
 * EXTRA_ALLOW_EXISTING_FILE - (default true) if existing files are selectable in 'new file'-mode
 * <p/>
 * The result of the user's action is returned in onActivityResult intent,
 * access it using getUri.
 * In case of multiple choices, these can be accessed with getClipData
 * containing Uri objects.
 * If running earlier than JellyBean you can access them with
 * getStringArrayListExtra(EXTRA_PATHS)
 *
 * @param <T>
 */
public abstract class AbstractFilePickerActivity<T> extends AppCompatActivity
        implements AbstractFilePickerFragment.OnFilePickedListener {
    public static final String EXTRA_START_PATH = "nononsense.intent" + ".START_PATH";
    public static final String EXTRA_MODE = "nononsense.intent.MODE";
    public static final String EXTRA_SINGLE_CLICK = "nononsense.intent" + ".SINGLE_CLICK";
    // For compatibility
    public static final String EXTRA_ALLOW_MULTIPLE = "android.intent.extra" + ".ALLOW_MULTIPLE";
    public static final String EXTRA_ALLOW_EXISTING_FILE = "android.intent.extra" + ".ALLOW_EXISTING_FILE";
    public static final String EXTRA_PATHS = "nononsense.intent.PATHS";
    public static final int MODE_FILE = AbstractFilePickerFragment.MODE_FILE;
    public static final int MODE_FILE_AND_DIR = AbstractFilePickerFragment.MODE_FILE_AND_DIR;
    public static final int MODE_NEW_FILE = AbstractFilePickerFragment.MODE_NEW_FILE;
    public static final int MODE_DIR = AbstractFilePickerFragment.MODE_DIR;
    protected static final String TAG = "filepicker_fragment";
    protected String startPath = null;
    protected int mode = AbstractFilePickerFragment.MODE_FILE;
    protected boolean allowMultiple = false;
    private boolean allowExistingFile = true;
    protected boolean singleClick = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.nnf_activity_filepicker);

        Intent intent = getIntent();
        if (intent != null) {
            startPath = intent.getStringExtra(EXTRA_START_PATH);
            mode = intent.getIntExtra(EXTRA_MODE, mode);
            allowMultiple = intent.getBooleanExtra(EXTRA_ALLOW_MULTIPLE, allowMultiple);
            allowExistingFile = intent.getBooleanExtra(EXTRA_ALLOW_EXISTING_FILE, allowExistingFile);
            singleClick = intent.getBooleanExtra(EXTRA_SINGLE_CLICK, singleClick);
        }

        // Default to cancelled
        setResult(Activity.RESULT_CANCELED);
    }

    @Override
    protected void onResume() {
        super.onResume();

        FragmentManager fm = getSupportFragmentManager();
        Fragment fragment = fm.findFragmentByTag(TAG);

        if (fragment == null) {
            fragment = getFragment(startPath, mode, allowMultiple, allowExistingFile, singleClick);
        }

        if (fragment != null) {
            fm.beginTransaction().replace(R.id.fragment, fragment, TAG).commit();
        }
    }

    protected abstract AbstractFilePickerFragment<T> getFragment(
            @Nullable final String startPath, final int mode, final boolean allowMultiple,
            final boolean allowExistingFile, final boolean singleClick);

    @Override
    public void onSaveInstanceState(Bundle b) {
        b.putString("WORKAROUND_FOR_BUG_19917_KEY", "WORKAROUND_FOR_BUG_19917_VALUE");
        super.onSaveInstanceState(b);
    }

    @Override
    public void onFilePicked(@NonNull final Uri file) {
        Intent i = new Intent();
        i.setData(file);
        setResult(Activity.RESULT_OK, i);
        finish();
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    @Override
    public void onFilesPicked(@NonNull final List<Uri> files) {
        Intent i = new Intent();
        i.putExtra(EXTRA_ALLOW_MULTIPLE, true);

        // Set as String Extras for all versions
        ArrayList<String> paths = new ArrayList<>();
        for (Uri file : files) {
            paths.add(file.toString());
        }
        i.putStringArrayListExtra(EXTRA_PATHS, paths);

        // Set as Clip Data for Jelly bean and above
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            ClipData clip = null;
            for (Uri file : files) {
                if (clip == null) {
                    clip = new ClipData("Paths", new String[]{},
                            new ClipData.Item(file));
                } else {
                    clip.addItem(new ClipData.Item(file));
                }
            }
            i.setClipData(clip);
        }

        setResult(Activity.RESULT_OK, i);
        finish();
    }

    @Override
    public void onCancelled() {
        setResult(Activity.RESULT_CANCELED);
        finish();
    }
}
