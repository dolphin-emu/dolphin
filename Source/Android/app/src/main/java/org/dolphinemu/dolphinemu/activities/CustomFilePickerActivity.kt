// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities

import android.os.Bundle
import android.os.Environment
import com.nononsenseapps.filepicker.AbstractFilePickerFragment
import com.nononsenseapps.filepicker.FilePickerActivity
import org.dolphinemu.dolphinemu.fragments.CustomFilePickerFragment
import java.io.File

class CustomFilePickerActivity : FilePickerActivity() {
    private var extensions: HashSet<String>? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (intent != null) {
            extensions = intent.getSerializableExtra(EXTRA_EXTENSIONS) as HashSet<String>?
        }
    }

    override fun getFragment(
        startPath: String?,
        mode: Int,
        allowMultiple: Boolean,
        allowCreateDir: Boolean,
        allowExistingFile: Boolean,
        singleClick: Boolean
    ): AbstractFilePickerFragment<File> {
        val fragment = CustomFilePickerFragment()
        // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
        fragment.setArgs(
            startPath ?: Environment.getExternalStorageDirectory().path,
            mode,
            allowMultiple,
            allowCreateDir,
            allowExistingFile,
            singleClick
        )
        fragment.setExtensions(extensions)
        return fragment
    }

    companion object {
        const val EXTRA_EXTENSIONS = "dolphinemu.org.filepicker.extensions"
    }
}
