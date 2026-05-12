package org.dolphinemu.dolphinemu.features.gba

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import java.nio.ByteBuffer

object GbaRenderManager {
    private val bitmaps = Array(4) { Bitmap.createBitmap(240, 160, Bitmap.Config.ARGB_8888) }
    private val frameBuffers = Array(4) { ByteBuffer.allocateDirect(240 * 160 * 4) }
    private val renderThread = HandlerThread("GBA render").apply { start() }
    private val handler = Handler(renderThread.looper)

    @Volatile
    private var activeViews: List<GbaOverlayView> = emptyList()

    @Volatile
    private var attached = false

    fun isAttached() = attached

    fun onFrame(slot: Int, buffer: ByteBuffer) {
        if (!attached || slot < 0 || slot >= 4) return
        val bufferCopy = frameBuffers[slot]
        synchronized(bufferCopy) {
            bufferCopy.rewind()
            buffer.rewind()
            if (buffer.remaining() == bufferCopy.capacity()) {
                bufferCopy.put(buffer)
            }
        }

        handler.post {
            val view = activeViews.firstOrNull { it.gbaSlot == slot } ?: return@post
            if (!view.holder.surface.isValid) return@post
            synchronized(bufferCopy) {
                bufferCopy.rewind()
                bitmaps[slot].copyPixelsFromBuffer(bufferCopy)
            }
            view.drawFrame(bitmaps[slot])
        }
    }

    fun requestRedraw(slot: Int) {
        if (!attached || slot < 0 || slot >= 4) return
        handler.post {
            val view = activeViews.firstOrNull { it.gbaSlot == slot } ?: return@post
            if (!view.holder.surface.isValid) return@post
            view.drawFrame(bitmaps[slot])
        }
    }

    fun attach(views: List<GbaOverlayView>) {
        handler.removeCallbacksAndMessages(null)
        attached = true
        activeViews = views
        views.forEach {
            it.renderManager = this
            if (it.surfaceReady) requestRedraw(it.gbaSlot)
        }
    }

    fun updateViews(views: List<GbaOverlayView>) {
        activeViews = views
        views.forEach { it.renderManager = this }
        views.forEach { if (it.surfaceReady) requestRedraw(it.gbaSlot) }
    }

    fun detach() {
        handler.removeCallbacksAndMessages(null)
        activeViews = emptyList()
        attached = false
    }
}
