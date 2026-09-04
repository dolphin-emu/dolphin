// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import androidx.annotation.Keep
import androidx.core.graphics.createBitmap
import org.dolphinemu.dolphinemu.NativeLibrary
import java.nio.ByteBuffer

@Keep
object GbaRenderer {
    private const val GBA_WIDTH = 240
    private const val GBA_HEIGHT = 160
    private const val BYTES_PER_PIXEL = 4
    private const val GBA_BUFFER_SIZE = GBA_WIDTH * GBA_HEIGHT * BYTES_PER_PIXEL
    private const val GB_WIDTH = 160
    private const val GB_HEIGHT = 144
    @JvmStatic
    external fun resetGbaCore(slot: Int)
    @JvmStatic
    external fun resetToMultiboot(slot: Int)

    @JvmStatic
    external fun setGbaVolume(slot: Int, volume: Int)
    @JvmStatic
    external fun setTvLeftOffset(offset: Int)
    @JvmStatic
    external fun getTvDrawWidth(): Int
    @JvmStatic
    external fun getTvDrawHeight(): Int
    @JvmStatic
    external fun getTvDrawTop(): Int
    @JvmStatic
    external fun getFrameCount(): Int

    private val bitmaps = arrayOfNulls<Bitmap>(4)
    private val handler = Handler(HandlerThread("GBA render").apply { start() }.looper)

    @Volatile
    private var slotToView = arrayOfNulls<GbaOverlayView>(4)

    @Volatile
    private var attached = false

    fun isAttached() = attached

    @Keep
    @JvmStatic
    fun onGbaFrameBuffer(slot: Int, buffer: ByteBuffer) {
        if (!attached || slot !in 0..3) return
        val view = slotToView[slot] ?: return
        if (!view.isScreenVisible) return

        buffer.rewind()
        val remaining = buffer.remaining()
        val w = if (remaining == GBA_BUFFER_SIZE) GBA_WIDTH else GB_WIDTH
        val h = if (w == GBA_WIDTH) GBA_HEIGHT else GB_HEIGHT

        val bitmap = bitmaps[slot]?.takeIf { it.width == w && it.height == h }
            ?: createBitmap(w, h).also { bitmaps[slot] = it }

        bitmap.copyPixelsFromBuffer(buffer)
        handler.post {
            if (attached && view.holder.surface.isValid) {
                view.updateNativeDimensions(w, h)
                view.drawFrame(bitmap)
            }
        }
    }

    fun requestRedraw(slot: Int) {
        if (slot !in 0..3) return
        val bmp = bitmaps[slot] ?: return
        val view = slotToView[slot] ?: return
        handler.post {
            if (attached && view.holder.surface.isValid) {
                view.drawFrame(bmp)
            }
        }
    }

    fun attach(views: List<GbaOverlayView>) {
        attached = true
        updateViews(views)
    }

    fun updateViews(views: List<GbaOverlayView>) {
        val newMap = arrayOfNulls<GbaOverlayView>(4)
        views.forEach { newMap[it.gbaSlot] = it }
        slotToView = newMap
        views.forEach { if (it.surfaceReady) requestRedraw(it.gbaSlot) }
    }

    fun detach() {
        handler.removeCallbacksAndMessages(null)
        attached = false
        slotToView = arrayOfNulls(4)
        for (i in 0 until 4) {
            bitmaps[i]?.recycle()
            bitmaps[i] = null
        }
    }

    @JvmStatic
    @Keep
    fun onTvSizeChanged() {
        NativeLibrary.getEmulationActivity()?.let { activity ->
            activity.runOnUiThread {
                activity.gba.applyGbaLayout()
            }
        }
    }
}
