package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.graphics.*

// Passive SurfaceView displaying one GBA screen
// Rendering is driven by GbaRenderManager

class GbaOverlayView(context: Context) : SurfaceView(context),
    SurfaceHolder.Callback {
    var renderManager: GbaRenderManager? = null
    var gbaSlot: Int = 0
    var isScreenVisible = true
    var needsBorderRedraw = false
    var surfaceReady = false

    private val paint = Paint().apply { isFilterBitmap = true }
    private val destRect = Rect()
    private val borderPaint = Paint().apply {
        color = Color.argb(120, 255, 255, 255); style = Paint.Style.STROKE
        strokeWidth = 2f; isAntiAlias = true
    }
    private val borderFillPaint = Paint().apply {
        color = Color.argb(0, 0, 0, 0); style = Paint.Style.FILL
    }
    private val borderTextPaint = Paint().apply {
        color = Color.argb(120, 255, 255, 255); textSize = 20f
        isAntiAlias = true; typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }

    init {
        holder.setFormat(PixelFormat.TRANSLUCENT)
        setZOrderMediaOverlay(true)
        holder.addCallback(this)
    }

    fun drawFrame(bitmap: Bitmap) {
        if (!holder.surface.isValid) return
        if (!isScreenVisible) {
            if (needsBorderRedraw) {
                needsBorderRedraw = false
                val canvas = holder.lockCanvas() ?: return
                canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR)
                val rect = RectF(2f, 2f, width - 2f, height - 2f)
                canvas.drawRoundRect(rect, 12f, 12f, borderFillPaint)
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
        holder.unlockCanvasAndPost(canvas)
    }

    fun onDoubleTap() {
        isScreenVisible = !isScreenVisible
        if (!isScreenVisible) needsBorderRedraw = true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun surfaceCreated(h: SurfaceHolder) {
        surfaceReady = false
    }

    override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, h2: Int) {
        surfaceReady = true
        post { renderManager?.requestRedraw(gbaSlot) }
    }

    override fun surfaceDestroyed(h: SurfaceHolder) {
        surfaceReady = false
    }
}

