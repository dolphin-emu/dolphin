// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.app.Dialog
import android.graphics.BitmapFactory
import android.os.Bundle
import android.view.LayoutInflater
import android.widget.ImageView
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import java.net.URL

class CameraPreviewDialogFragment : DialogFragment() {
    private var imageView: ImageView? = null
    private var pollJob: Job? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val view = layoutInflater.inflate(R.layout.dialog_camera_preview, null)
        imageView = view.findViewById(R.id.camera_preview_image)

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.camera_preview)
            .setView(view)
            .setPositiveButton(R.string.ok) { _, _ -> dismiss() }
            .create()
    }

    override fun onStart() {
        super.onStart()
        pollJob = lifecycleScope.launch {
            val ip = NativeLibrary.getTriforceCameraIP() ?: return@launch
            while (isActive) {
                val bitmap = withContext(Dispatchers.IO) {
                    try {
                        val connection = URL("http://$ip/img.jpg").openConnection() as java.net.HttpURLConnection
                        connection.connectTimeout = CONNECTION_TIMEOUT_MS
                        connection.readTimeout = CONNECTION_TIMEOUT_MS
                        connection.requestMethod = "GET"
                        connection.connect()
                        if (connection.responseCode == 200) {
                            connection.inputStream.use { BitmapFactory.decodeStream(it) }
                        } else {
                            null
                        }
                    } catch (e: Exception) {
                        null
                    }
                }
                bitmap?.let { imageView?.setImageBitmap(it) }
                delay(REFRESH_INTERVAL_MS)
            }
        }
    }

    override fun onStop() {
        super.onStop()
        pollJob?.cancel()
        pollJob = null
    }

    companion object {
        const val TAG = "CameraPreviewDialogFragment"
        private const val REFRESH_INTERVAL_MS = 100L
        private const val CONNECTION_TIMEOUT_MS = 500
    }
}
