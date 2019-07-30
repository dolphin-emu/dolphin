/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package com.nononsenseapps.filepicker;

import android.annotation.SuppressLint;
import android.os.Environment;
import android.support.annotation.Nullable;

import java.io.File;

@SuppressLint("Registered")
public class FilePickerActivity extends AbstractFilePickerActivity<File> {

    @Override
    protected AbstractFilePickerFragment<File> getFragment(
            @Nullable final String startPath, final int mode, final boolean allowMultiple,
            final boolean allowExistingFile, final boolean singleClick) {
        AbstractFilePickerFragment<File> fragment = new FilePickerFragment();
        // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
        fragment.setArgs(startPath != null ? startPath : Environment.getExternalStorageDirectory().getPath(),
                mode, allowMultiple, allowExistingFile, singleClick);
        return fragment;
    }
}
