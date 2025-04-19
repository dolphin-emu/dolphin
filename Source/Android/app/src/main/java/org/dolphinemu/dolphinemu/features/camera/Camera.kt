// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.camera

import android.content.Context
import android.graphics.ImageFormat
import android.hardware.camera2.CameraMetadata
import android.util.Log
import android.util.Size
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
import org.dolphinemu.dolphinemu.utils.PermissionsHandler
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class Camera {
    companion object {
        val TAG = "Camera"
        private var instance: Camera? = null

        private var cameraEntries: Array<String> = arrayOf()
        private var cameraValues: Array<String> = arrayOf()

        private var width = 0
        private var height = 0
        private var isRunning: Boolean = false
        private lateinit var imageCapture: ImageCapture
        private lateinit var cameraExecutor: ExecutorService

        fun getInstance(context: Context) = instance ?: synchronized(this) {
            instance ?: Camera().also {
                instance = it
                val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
                cameraProviderFuture.addListener({
                    val cameraProvider = cameraProviderFuture.get()
                    val cameraInfos = cameraProvider.getAvailableCameraInfos()
                    var index = 0;
                    fun getCameraDescription(type: Int) : String {
                        return when (type) {
                            CameraMetadata.LENS_FACING_BACK -> "Back"
                            CameraMetadata.LENS_FACING_FRONT -> "Front"
                            CameraMetadata.LENS_FACING_EXTERNAL -> "External"
                            else -> "Unknown"
                        }
                    }
                    for (camera in cameraInfos) {
                        cameraEntries += "${index}: ${getCameraDescription(camera.lensFacing)}"
                        cameraValues += index++.toString()
                    }
                }, ContextCompat.getMainExecutor(context))
                cameraExecutor = Executors.newSingleThreadExecutor()
            }
        }

        fun getCameraEntries(): Array<String> {
            return cameraEntries
        }

        fun getCameraValues(): Array<String> {
            return cameraValues
        }

        @JvmStatic
        fun resumeCamera() {
            if (width == 0 || height == 0)
                return
            Log.i(TAG, "resumeCamera")
            startCamera(width, height)
        }

        @JvmStatic
        fun startCamera(width: Int, height: Int) {
            this.width = width
            this.height = height
            if (!PermissionsHandler.hasCameraAccess(DolphinApplication.getAppContext())) {
                PermissionsHandler.requestCameraPermission(DolphinApplication.getAppActivity())
                return
            }

            if (isRunning)
                return
            isRunning = true
            Log.i(TAG, "startCamera: " + width + "x" + height)
            Log.i(TAG, "Selected Camera: " + StringSetting.MAIN_SELECTED_CAMERA.string)

            val cameraProviderFuture = ProcessCameraProvider.getInstance(DolphinApplication.getAppContext())
            cameraProviderFuture.addListener({
                val cameraProvider = cameraProviderFuture.get()

                val cameraSelector = cameraProvider.getAvailableCameraInfos()
                    .get(Integer.parseInt(StringSetting.MAIN_SELECTED_CAMERA.string))
                    .cameraSelector
                val preview = Preview.Builder().build()
                imageCapture = ImageCapture.Builder()
                    .setCaptureMode(ImageCapture.CAPTURE_MODE_MINIMIZE_LATENCY)
                    .build()
                val imageAnalyzer = ImageAnalysis.Builder()
                    .setTargetResolution(Size(width, height))
                    .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
                    .build()
                    .also {
                        it.setAnalyzer(cameraExecutor, ImageProcessor())
                    }

                cameraProvider.bindToLifecycle(DolphinApplication.getAppActivity() as LifecycleOwner, cameraSelector, preview, imageCapture, imageAnalyzer)
            }, ContextCompat.getMainExecutor(DolphinApplication.getAppContext()))
        }

        @JvmStatic
        fun stopCamera() {
            if (!isRunning)
                return
            isRunning = false
            width = 0
            height = 0
            Log.i(TAG, "stopCamera")

            val cameraProviderFuture = ProcessCameraProvider.getInstance(DolphinApplication.getAppContext())
            cameraProviderFuture.addListener({
                val cameraProvider = cameraProviderFuture.get()
                cameraProvider.unbindAll()
            }, ContextCompat.getMainExecutor(DolphinApplication.getAppContext()))
        }

        private class ImageProcessor : ImageAnalysis.Analyzer {
            override fun analyze(image: ImageProxy) {
                if (image.format != ImageFormat.YUV_420_888) {
                    Log.e(TAG, "Error: Unhandled image format: ${image.format}")
                    image.close()
                    stopCamera()
                    return
                }

                Log.i(TAG, "analyze sz=${image.width}x${image.height} / fmt=${image.format} / "
                    + "rot=${image.imageInfo.rotationDegrees} / "
//                    + "px0=${image.planes[0].pixelStride} / px1=${image.planes[1].pixelStride} / px2=${image.planes[2].pixelStride} / "
                    + "row0=${image.planes[0].rowStride} / row1=${image.planes[1].rowStride} / row2=${image.planes[2].rowStride}"
                )

                // Convert YUV_420_888 to YUY2
                val yuy2Image = ByteArray(2 * width * height)
                for (line in 0 until height) {
                    for (col in 0 until width) {
                        val yuy2Pos = 2 * (width * line + col)
                        val yPos = image.planes[0].rowStride * line       + image.planes[0].pixelStride * col
                        var uPos = image.planes[1].rowStride * (line / 2) + image.planes[1].pixelStride * (col / 2)
                        var vPos = image.planes[2].rowStride * (line / 2) + image.planes[2].pixelStride * (col / 2)
                        yuy2Image.set(yuy2Pos, image.planes[0].buffer.get(yPos))
                        yuy2Image.set(yuy2Pos + 1, if (col % 2 == 0) image.planes[1].buffer.get(uPos)
                                                                else image.planes[2].buffer.get(vPos))
                    }
                }
                image.close()
                NativeLibrary.CameraSetData(yuy2Image)
            }
        }
    }
}
