// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.core.content.FileProvider
import com.nononsenseapps.filepicker.FilePickerFragment
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable
import java.io.File
import java.util.Locale

class CustomFilePickerFragment : FilePickerFragment() {
    private var extensions: HashSet<String>? = null

    fun setExtensions(extensions: HashSet<String>?) {
        var b = arguments
        if (b == null)
            b = Bundle()
        b.putSerializable(KEY_EXTENSIONS, extensions)
        arguments = b
    }

    override fun toUri(file: File): Uri {
        return FileProvider.getUriForFile(
            requireContext(),
            "${requireContext().applicationContext.packageName}.filesprovider",
            file
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        extensions = requireArguments().serializable(KEY_EXTENSIONS) as HashSet<String>?

        if (mode == MODE_DIR) {
            val ok = requireActivity().findViewById<TextView>(R.id.nnf_button_ok)
            ok.setText(R.string.select_dir)

            val cancel = requireActivity().findViewById<TextView>(R.id.nnf_button_cancel)
            cancel.visibility = View.GONE
        }
    }

    override fun isItemVisible(file: File): Boolean {
        // Some users jump to the conclusion that Dolphin isn't able to detect their
        // files if the files don't show up in the file picker when mode == MODE_DIR.
        // To avoid this, show files even when the user needs to select a directory.
        return (showHiddenItems || !file.isHidden) &&
                (file.isDirectory || extensions!!.contains(fileExtension(file.name).lowercase(Locale.ENGLISH)))
    }

    override fun isCheckable(file: File): Boolean {
        // We need to make a small correction to the isCheckable logic due to
        // overriding isItemVisible to show files when mode == MODE_DIR.
        // AbstractFilePickerFragment always treats files as checkable when
        // allowExistingFile == true, but we don't want files to be checkable when mode == MODE_DIR.
        return super.isCheckable(file) && !(mode == MODE_DIR && file.isFile)
    }

    companion object {
        const val KEY_EXTENSIONS = "KEY_EXTENSIONS"

        private fun fileExtension(filename: String): String {
            val i = filename.lastIndexOf('.')
            return if (i < 0) "" else filename.substring(i + 1)
        }
    }
}
