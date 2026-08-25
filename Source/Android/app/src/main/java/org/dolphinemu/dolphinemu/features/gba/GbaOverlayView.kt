// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.graphics.*
import android.view.SurfaceHolder
import android.view.SurfaceView

class GbaOverlayView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    var gbaSlot = 0
    var isScreenVisible = true
    var isDimmed = false
    var needsBorderRedraw = false
    var surfaceReady = false
    var nativeWidth = 0
    var nativeHeight = 0
    var onDimensionsChanged: (() -> Unit)? = null

    private val paint = Paint(Paint.FILTER_BITMAP_FLAG)
    private val destRect = Rect()
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val borderTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        textSize = 20f
        typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }

    init {
        holder.setFormat(PixelFormat.TRANSLUCENT)
        setZOrderMediaOverlay(true)
        holder.addCallback(this)
    }

    fun updateNativeDimensions(w: Int, h: Int) {
        if (nativeWidth == w && nativeHeight == h) return
        nativeWidth = w
        nativeHeight = h
        post { onDimensionsChanged?.invoke() }
    }

    fun drawFrame(bitmap: Bitmap) {
        if (!holder.surface.isValid) return

        if (!isScreenVisible) {
            if (needsBorderRedraw) {
                needsBorderRedraw = false
                val canvas = holder.lockCanvas() ?: return
                canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR)
                val rect = RectF(2f, 2f, width - 2f, height - 2f)
                canvas.drawRoundRect(rect, 12f, 12f, borderPaint)
                canvas.drawText("GBA", width / 2f, height / 2f + 8f, borderTextPaint)
                holder.unlockCanvasAndPost(canvas)
            }
            return
        }

        val canvas = holder.lockCanvas() ?: return
        destRect.set(0, 0, width, height)
        canvas.drawColor(Color.BLACK)
        canvas.drawBitmap(bitmap, null, destRect, paint)

        if (isDimmed) {
            canvas.drawColor(Color.argb(150, 0, 0, 0))
        }

        holder.unlockCanvasAndPost(canvas)
    }

    fun onDoubleTap() {
        isScreenVisible = !isScreenVisible
        if (!isScreenVisible) needsBorderRedraw = true
    }

    override fun performClick() = super.performClick() || true

    override fun surfaceCreated(h: SurfaceHolder) {
        surfaceReady = true
    }

    override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, h2: Int) {
        surfaceReady = true
        post { GbaRenderer.requestRedraw(gbaSlot) }
    }

    override fun surfaceDestroyed(h: SurfaceHolder) {
        surfaceReady = false
    }
}
