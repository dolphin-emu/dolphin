package org.dolphinemu.dolphinemu.features.gba

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import java.nio.ByteBuffer

object GbaRenderManager {
    private val bitmaps = arrayOfNulls<Bitmap>(4)
    private val frameBuffers = arrayOfNulls<ByteBuffer>(4)
    private val renderThread = HandlerThread("GBA render").apply { start() }
    private val handler = Handler(renderThread.looper)

    @Volatile
    private var activeViews: List<GbaOverlayView> = emptyList()

    @Volatile
    private var attached = false

    fun isAttached() = attached

    fun onFrame(slot: Int, buffer: ByteBuffer) {
        if (!attached || slot < 0 || slot >= 4) return

        buffer.rewind()
        val frameSize = getFrameSize(buffer.remaining()) ?: return
        val bufferCopy = getFrameBuffer(slot, frameSize)

        synchronized(bufferCopy) {
            bufferCopy.rewind()
            bufferCopy.put(buffer)
        }

        handler.post {
            val view = activeViews.firstOrNull { it.gbaSlot == slot } ?: return@post
            if (!view.holder.surface.isValid) return@post

            val bitmap = getBitmap(slot, frameSize)

            view.updateNativeDimensions(frameSize.width, frameSize.height)

            synchronized(bufferCopy) {
                bufferCopy.rewind()
                bitmap.copyPixelsFromBuffer(bufferCopy)
            }

            view.drawFrame(bitmap)
        }
    }

    fun requestRedraw(slot: Int) {
        if (!attached || slot < 0 || slot >= 4) return

        handler.post {
            val view = activeViews.firstOrNull { it.gbaSlot == slot } ?: return@post
            val bitmap = bitmaps[slot] ?: return@post

            if (!view.holder.surface.isValid) return@post
            view.drawFrame(bitmap)
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

    private fun getFrameSize(byteCount: Int): FrameSize? =
        when (byteCount) {
            GBA_WIDTH * GBA_HEIGHT * BYTES_PER_PIXEL -> FrameSize(GBA_WIDTH, GBA_HEIGHT)
            GB_WIDTH * GB_HEIGHT * BYTES_PER_PIXEL -> FrameSize(GB_WIDTH, GB_HEIGHT)
            else -> null
        }

    private fun getFrameBuffer(slot: Int, frameSize: FrameSize): ByteBuffer {
        val byteCount = frameSize.width * frameSize.height * BYTES_PER_PIXEL
        val existing = frameBuffers[slot]

        if (existing != null && existing.capacity() == byteCount) {
            return existing
        }

        return ByteBuffer.allocateDirect(byteCount).also {
            frameBuffers[slot] = it
        }
    }

    private fun getBitmap(slot: Int, frameSize: FrameSize): Bitmap {
        val existing = bitmaps[slot]

        if (
            existing != null &&
            existing.width == frameSize.width &&
            existing.height == frameSize.height
        ) {
            return existing
        }

        return Bitmap.createBitmap(
            frameSize.width,
            frameSize.height,
            Bitmap.Config.ARGB_8888
        ).also {
            bitmaps[slot] = it
        }
    }

    fun detach() {
        handler.removeCallbacksAndMessages(null)
        activeViews = emptyList()
        attached = false
    }

    private data class FrameSize(val width: Int, val height: Int)

    private const val GBA_WIDTH = 240
    private const val GBA_HEIGHT = 160
    private const val GB_WIDTH = 160
    private const val GB_HEIGHT = 144
    private const val BYTES_PER_PIXEL = 4
}
