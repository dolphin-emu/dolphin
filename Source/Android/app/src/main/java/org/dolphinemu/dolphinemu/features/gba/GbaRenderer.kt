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
    private var activeViews = emptyList<GbaOverlayView>()
    @Volatile
    private var attached = false

    fun isAttached() = attached

    @Keep
    @JvmStatic
    fun onGbaFrameBuffer(slot: Int, buffer: ByteBuffer) {
        if (!attached) return
        val view = activeViews.find { it.gbaSlot == slot } ?: return

        buffer.rewind()
        val w = if (buffer.remaining() == GBA_BUFFER_SIZE) GBA_WIDTH else GB_WIDTH
        val h = if (w == GBA_WIDTH) GBA_HEIGHT else GB_HEIGHT

        val bitmap = bitmaps[slot]?.takeIf { (it.width == w) && (it.height == h) }
            ?: createBitmap(w, h).also { bitmaps[slot] = it }

        bitmap.copyPixelsFromBuffer(buffer)
        handler.post {
            if (attached && view.holder.surface.isValid) {
                view.updateNativeDimensions(w, h)
                view.drawFrame(bitmap)
            }
        }
    }

    fun requestRedraw(slot: Int) = bitmaps[slot]?.let { bmp ->
        activeViews.find { it.gbaSlot == slot }?.let { v ->
            handler.post { if (v.holder.surface.isValid) v.drawFrame(bmp) }
        }
    }

    fun attach(views: List<GbaOverlayView>) {
        attached = true
        updateViews(views)
    }

    fun updateViews(views: List<GbaOverlayView>) {
        activeViews = views
        views.forEach { if (it.surfaceReady) requestRedraw(it.gbaSlot) }
    }

    fun detach() {
        handler.removeCallbacksAndMessages(null)
        attached = false
        activeViews = emptyList()
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
