// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.PorterDuff
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Typeface
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

    var isLinearFiltering = false
        set(value) {
            field = value
            paint.isFilterBitmap = value
        }

    val aspectRatio: Float
        get() = if ((nativeWidth > 0) && (nativeHeight > 0)) {
            nativeWidth.toFloat() / nativeHeight.toFloat()
        } else {
            1.5f
        }

    private val paint = Paint(Paint.FILTER_BITMAP_FLAG)
    private val destRect = Rect()
    private val borderRect = RectF()
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
        if (!holder.surface.isValid || bitmap.isRecycled) return

        val canvas = holder.surface.lockHardwareCanvas() ?: return
        if (isScreenVisible) {
            destRect.set(0, 0, width, height)
            canvas.drawColor(Color.BLACK)
            canvas.drawBitmap(bitmap, null, destRect, paint)
            if (isDimmed) canvas.drawColor(Color.argb(150, 0, 0, 0))
        } else if (needsBorderRedraw) {
            needsBorderRedraw = false
            canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR)
            borderRect.set(2f, 2f, width.toFloat() - 2f, height.toFloat() - 2f)
            canvas.drawRoundRect(borderRect, 12f, 12f, borderPaint)
            canvas.drawText("GBA ${gbaSlot + 1}", width / 2f, height / 2f + 8f, borderTextPaint)
        }
        holder.surface.unlockCanvasAndPost(canvas)
    }

    fun onDoubleTap() {
        isScreenVisible = !isScreenVisible
        needsBorderRedraw = true
        GbaRenderer.requestRedraw(gbaSlot)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

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
