// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import androidx.annotation.Keep
import androidx.core.graphics.createBitmap
import org.dolphinemu.dolphinemu.NativeLibrary
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

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
    external fun setTvRightOffset(offset: Int)
    @JvmStatic
    external fun setTvTopOffset(offset: Int)
    @JvmStatic
    external fun setTvBottomOffset(offset: Int)
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

    private val isFramePending = Array(4) { AtomicBoolean(false) }
    private val frameBuffers = Array(4) { ByteArray(GBA_BUFFER_SIZE) }
    private val byteBufferWrappers = Array(4) { ByteBuffer.wrap(frameBuffers[it]) }
    private val frameBufferSizes = IntArray(4)

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

        // If the handler is still processing a previous frame for this slot, skip this one.
        if (isFramePending[slot].get()) return

        buffer.rewind()
        val remaining = buffer.remaining().coerceAtMost(GBA_BUFFER_SIZE)

        // Copy to pre-allocated buffer and ensure the native buffer can be reused immediately
        buffer.get(frameBuffers[slot], 0, remaining)
        frameBufferSizes[slot] = remaining
        isFramePending[slot].set(true)

        handler.post {
            if (!attached) {
                isFramePending[slot].set(false)
                return@post
            }

            val size = frameBufferSizes[slot]
            val w = if (size == GBA_BUFFER_SIZE) GBA_WIDTH else GB_WIDTH
            val h = if (w == GBA_WIDTH) GBA_HEIGHT else GB_HEIGHT

            val bitmap = bitmaps[slot]?.takeIf { it.width == w && it.height == h }
                ?: createBitmap(w, h).also { bitmaps[slot] = it }

            val wrapper = byteBufferWrappers[slot]
            wrapper.clear().limit(size)
            bitmap.copyPixelsFromBuffer(wrapper)

            isFramePending[slot].set(false)

            view.updateNativeDimensions(w, h)

            if (view.holder.surface.isValid) {
                view.drawFrame(bitmap)
            }
        }
    }

    fun requestRedraw(slot: Int) {
        if (slot !in 0..3) return
        val view = slotToView[slot] ?: return
        handler.post {
            if (!attached) return@post
            val bmp = bitmaps[slot] ?: return@post
            if (view.holder.surface.isValid) {
                view.drawFrame(bmp)
            }
        }
    }

    fun attach(views: List<GbaOverlayView>) {
        attached = true
        updateViews(views)
    }

    fun updateViews(views: List<GbaOverlayView>) {
        slotToView = arrayOfNulls<GbaOverlayView>(4).apply {
            views.forEach { this[it.gbaSlot] = it }
        }
        views.forEach { if (it.surfaceReady) requestRedraw(it.gbaSlot) }
    }

    fun detach() {
        handler.removeCallbacksAndMessages(null)
        attached = false
        bitmaps.forEachIndexed { i, _ ->
            bitmaps[i]?.recycle(); bitmaps[i] = null
            isFramePending[i].set(false)
        }
        slotToView = arrayOfNulls(4)
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
