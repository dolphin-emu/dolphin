package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.content.res.Configuration
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.doOnLayout
import androidx.fragment.app.FragmentManager
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.overlay.InputOverlay
import org.dolphinemu.dolphinemu.R

class GbaEmulationActivity(
    private val activity: EmulationActivity,
    private val binding: ActivityEmulationBinding
) {
    val gbaViews = mutableListOf<GbaOverlayView>()
    val lastGbaTapTimes = mutableMapOf<Int, Long>()
    var isGbaLocked = false
    private var isMenuOpen = false
    private var lockedLandscapeLayoutGeneration = 0
    private var isWaitingForGcOffsetReset = false

    // Check if any menus are opened, disable GBA touch controls to stop conflicting with the menus
    private val backStackListener = FragmentManager.OnBackStackChangedListener {
        val fm = activity.supportFragmentManager
        val menuOpen =
            fm.findFragmentById(R.id.frame_menu) != null || fm.findFragmentById(R.id.frame_submenu) != null
        if (menuOpen == isMenuOpen) return@OnBackStackChangedListener
        isMenuOpen = menuOpen
        if (!isMenuOpen && !isGbaLocked) reattachTouchListeners()
    }

    fun initViews() {

        val globalGbaPrefs = activity.getSharedPreferences("gba_overlay", Context.MODE_PRIVATE)
        isGbaLocked = globalGbaPrefs.getBoolean("gba_locked", false)
        activity.supportFragmentManager.addOnBackStackChangedListener(backStackListener)

        for (slot in 0 until 4) {
            if (IntSetting.getSettingForSIDevice(slot).int != InputOverlay.EMULATED_GBA_CONTROLLER) continue
            val view = GbaOverlayView(activity)
            view.gbaSlot = slot
            val slotPrefs =
                activity.getSharedPreferences("gba_overlay_${slot}", Context.MODE_PRIVATE)
            val sw = slotPrefs.getFloat("gba_width", 480f).coerceIn(120f, 960f)
            val sh = slotPrefs.getFloat("gba_height", 320f).coerceIn(80f, 640f)
            binding.root.addView(view, 0, FrameLayout.LayoutParams(sw.toInt(), sh.toInt()))
            view.visibility = View.VISIBLE
            InputOverrider.registerGba(slot)
            attachGbaTouchListener(view, slot, slotPrefs)
            gbaViews.add(view)
        }

        if (gbaViews.isNotEmpty() && NativeLibrary.IsGameMetadataValid()) {
            GbaRenderManager.attach(gbaViews)
            binding.root.doOnLayout { requestGbaLayout(retryLockedLandscape = true) }
        }
    }

    fun onTitleChanged() {
        val activeSlots = (0 until 4).filter {
            IntSetting.getSettingForSIDevice(it).int == InputOverlay.EMULATED_GBA_CONTROLLER
        }
        gbaViews.forEachIndexed { index, view ->
            if (index < activeSlots.size) {
                view.gbaSlot = activeSlots[index]
                view.visibility = View.VISIBLE
                InputOverrider.registerGba(activeSlots[index])
            } else {
                view.visibility = View.GONE
            }
        }
        if (gbaViews.isNotEmpty()) {
            if (GbaRenderManager.isAttached()) {
                GbaRenderManager.updateViews(gbaViews)
            } else {
                GbaRenderManager.attach(gbaViews)
            }
            requestGbaLayout(retryLockedLandscape = true)
        }
    }

    fun onDestroy() {
        activity.supportFragmentManager.removeOnBackStackChangedListener(backStackListener)
        GbaRenderManager.detach()
        for (slot in 0 until 4) {
            InputOverrider.unregisterGba(slot)
        }
        gbaViews.forEach { binding.root.removeView(it) }
        gbaViews.clear()
    }

    fun onConfigurationChanged() {
        if (gbaViews.isNotEmpty()) {
            GbaRenderManager.updateViews(gbaViews)
            requestGbaLayout(retryLockedLandscape = true)
        }
    }

    fun handleTouch(event: MotionEvent): Boolean {
        if (isGbaLocked || isMenuOpen || gbaViews.isEmpty()) return false
        val loc = IntArray(2)
        for (gbaView in gbaViews) {
            gbaView.getLocationOnScreen(loc)
            val bounds = android.graphics.Rect(
                loc[0], loc[1],
                loc[0] + gbaView.width,
                loc[1] + gbaView.height
            )
            if (bounds.contains(event.rawX.toInt(), event.rawY.toInt())) {
                return gbaView.dispatchTouchEvent(event)
            }
        }
        return false
    }

    fun setGbaViewsTouchable(touchable: Boolean) {
        gbaViews.forEach { view ->
            view.isClickable = touchable
            view.isFocusable = touchable
            view.isFocusableInTouchMode = touchable
            if (!touchable) view.setOnTouchListener(null)
        }
    }

    // Re-attaches drag/scale listeners after exiting overlay-edit mode.
    fun reattachTouchListeners() {
        if (isGbaLocked) return
        gbaViews.forEach { view ->
            val slot = view.gbaSlot
            val slotPrefs = activity.getSharedPreferences(
                "gba_overlay_${slot}", Context.MODE_PRIVATE
            )
            attachGbaTouchListener(view, slot, slotPrefs)
        }
    }

    private fun attachGbaTouchListener(
        view: GbaOverlayView,
        slot: Int,
        slotPrefs: android.content.SharedPreferences
    ) {
        var dragX = 0f
        var dragY = 0f
        val params = view.layoutParams as FrameLayout.LayoutParams
        var cw = params.width.toFloat()
        var ch = params.height.toFloat()

        val scaleDetector = ScaleGestureDetector(
            activity,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(d: ScaleGestureDetector): Boolean {
                    if (isGbaLocked) return true
                    val sf = d.scaleFactor
                    val ow = cw;
                    val oh = ch
                    cw = (cw * sf).coerceIn(120f, 960f)
                    ch = cw * (2f / 3f)
                    view.x += (ow - cw) / 2f
                    view.y += (oh - ch) / 2f
                    val p = view.layoutParams as FrameLayout.LayoutParams
                    p.width = cw.toInt(); p.height = ch.toInt()
                    view.layoutParams = p
                    slotPrefs.edit()
                        .putFloat("gba_width", cw).putFloat("gba_height", ch)
                        .putFloat("gba_x", view.x).putFloat("gba_y", view.y)
                        .apply()
                    return true
                }
            })

        view.setOnTouchListener { v, event ->
            if (isGbaLocked) return@setOnTouchListener false
            scaleDetector.onTouchEvent(event)
            if (scaleDetector.isInProgress) return@setOnTouchListener true
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    dragX = event.rawX - v.x
                    dragY = event.rawY - v.y
                }

                MotionEvent.ACTION_MOVE -> {
                    v.x = event.rawX - dragX
                    v.y = event.rawY - dragY
                }

                MotionEvent.ACTION_UP -> {
                    v.performClick()
                    val now = System.currentTimeMillis()
                    val last = lastGbaTapTimes[slot] ?: 0L
                    if (now - last < 300) view.onDoubleTap()
                    lastGbaTapTimes[slot] = now
                    slotPrefs.edit()
                        .putFloat("gba_x", v.x).putFloat("gba_y", v.y)
                        .apply()
                }
            }
            true
        }
    }

    fun applyGbaLayout() {
        if (gbaViews.isEmpty()) return
        val tw = binding.root.width
        val th = binding.root.height
        val count = gbaViews.size
        val isLandscape = activity.resources.configuration.orientation ==
            Configuration.ORIENTATION_LANDSCAPE

        if (isGbaLocked) {
            if (isLandscape) {
                if (!isWaitingForGcOffsetReset) {
                    isWaitingForGcOffsetReset = true
                    binding.root.post { applyGbaLayout() }
                    return
                }
                isWaitingForGcOffsetReset = false

                val slotH = th / count
                val gcDrawWidth = GbaLibrary.getGCDrawWidth()
                // Determine the aspect ratio to estimate the sidebar width if emulation hasn't started or isn't drawing.
                val aspectMode = IntSetting.GFX_ASPECT_RATIO.int
                val aspectRatio = when (aspectMode) {
                    1 -> 16f / 9f
                    2 -> 4f / 3f
                    3 -> if (th > 0) tw.toFloat() / th.toFloat() else 4f / 3f
                    else -> 4f / 3f
                }

                val estimatedGcWidth = (th * aspectRatio).toInt().coerceAtMost(tw)
                val maxSidebarW = (tw - estimatedGcWidth).coerceAtLeast(0)
                val rawSidebarW = if (gcDrawWidth > 0) {
                    (tw - gcDrawWidth).coerceAtLeast(0)
                } else {
                    maxSidebarW
                }
                val gcHorizontalMargin = if (aspectMode == 1) {
                    rawSidebarW / 2
                } else {
                    0
                }

                var actualSidebarW = 0

                gbaViews.forEachIndexed { i, v ->
                    v.isScreenVisible = true
                    v.needsBorderRedraw = false
                    v.setOnTouchListener(null)
                    val p = v.layoutParams as FrameLayout.LayoutParams

                    // Fill exact sidebar width
                    var targetW = rawSidebarW
                    var targetH = (targetW * 2f / 3f).toInt()

                    // If height exceeds slot, scale down from height
                    if (targetH > slotH) {
                        targetH = slotH
                        targetW = (targetH * 3f / 2f).toInt()
                    }

                    p.width = targetW
                    p.height = targetH
                    v.layoutParams = p

                    // Re-snapping: Force X to 0 and calculate centered Y within the slot
                    v.x = 0f
                    v.y = (i * slotH).toFloat() + (slotH - targetH) / 2f
                    v.visibility = View.VISIBLE

                    if (targetW > actualSidebarW) actualSidebarW = targetW
                }
                val gcOffset = if (aspectMode == 1) {
                    (actualSidebarW - gcHorizontalMargin).coerceAtLeast(0)
                } else {
                    actualSidebarW / 2
                }
                GbaLibrary.setGCLeftOffset(gcOffset)
            } else {
                // Portrait Logic
                isWaitingForGcOffsetReset = false
                val gih = (tw * 3f / 4f).toInt()
                val topBar = (th - gih) / 2
                val gbaY = topBar + gih
                val availH = th - gbaY
                val maxH = 400

                when (count) {
                    1 -> {
                        val targetH = (tw * 2f / 3f).toInt().coerceAtMost(availH).coerceAtMost(maxH)
                        val targetW = (targetH * 3f / 2f).toInt()

                        with(gbaViews[0]) {
                            setOnTouchListener(null)
                            val p = layoutParams as FrameLayout.LayoutParams
                            p.width = targetW; p.height = targetH; layoutParams = p
                            x = (tw - targetW) / 2f
                            y = gbaY.toFloat() + (availH - targetH) / 2f
                            visibility = View.VISIBLE
                        }
                    }

                    else -> {
                        // Multi-screen grid (2, 3, or 4)
                        val cols = if (count <= 2) count else 2
                        val rows = if (count <= 2) 1 else 2
                        val slotW = tw / cols
                        val slotH = availH / rows

                        val targetH =
                            (slotW * 2f / 3f).toInt().coerceAtMost(slotH).coerceAtMost(maxH)
                        val targetW = (targetH * 3f / 2f).toInt()

                        gbaViews.forEachIndexed { i, v ->
                            v.setOnTouchListener(null)
                            val p = v.layoutParams as FrameLayout.LayoutParams
                            p.width = targetW; p.height = targetH; v.layoutParams = p

                            val col = i % cols
                            val row = i / cols

                            v.x = (col * slotW).toFloat() + (slotW - targetW) / 2f
                            v.y = gbaY.toFloat() + (row * slotH).toFloat() + (slotH - targetH) / 2f
                            v.visibility = View.VISIBLE
                        }
                    }
                }
            }
        } else {
            isWaitingForGcOffsetReset = false
            gbaViews.forEachIndexed { i, view ->
                restoreViewFromPrefs(view, view.gbaSlot, i)
            }
        }
        activity.getSharedPreferences("gba_overlay", Context.MODE_PRIVATE).edit()
            .putBoolean("gba_locked", isGbaLocked).apply()
    }

    // Shared logic for reading a view's saved position/size from SharedPreferences.
    // Used by the unlocked branch of applyGbaLayout()
    private fun restoreViewFromPrefs(view: GbaOverlayView, slot: Int, index: Int) {
        val sp = activity.getSharedPreferences("gba_overlay_${slot}", Context.MODE_PRIVATE)
        val sw = sp.getFloat("gba_width", 480f).coerceIn(120f, 960f)
        val sh = sp.getFloat("gba_height", 320f).coerceIn(80f, 640f)
        val screenW = activity.resources.displayMetrics.widthPixels.toFloat()
        val screenH = activity.resources.displayMetrics.heightPixels.toFloat()
        var sx = sp.getFloat("gba_x", 16f + index * 20f)
        var sy = sp.getFloat("gba_y", screenH - sh - 16f - index * 20f)
        if (sx < 0 || sx > screenW) sx = 16f + index * 20f
        if (sy < 0 || sy > screenH) sy = screenH - sh - 16f
        val p = view.layoutParams as FrameLayout.LayoutParams
        p.width = sw.toInt(); p.height = sh.toInt(); view.layoutParams = p
        view.x = sx; view.y = sy
        attachGbaTouchListener(view, slot, sp)
    }

    fun toggleGBASnap() {
        isGbaLocked = !isGbaLocked
        if (!isGbaLocked) {
            GbaLibrary.setGCLeftOffset(0)
        }
        requestGbaLayout(retryLockedLandscape = true)
    }

    private fun requestGbaLayout(retryLockedLandscape: Boolean = false) {
        binding.root.post {
            applyGbaLayout()
            if (retryLockedLandscape) {
                scheduleLockedLandscapeLayoutRetries()
            }
        }
    }

    private fun scheduleLockedLandscapeLayoutRetries() {
        if (!isLockedLandscape()) return

        val generation = ++lockedLandscapeLayoutGeneration
        val retryDelays = longArrayOf(100L, 300L)
        retryDelays.forEach { delayMs ->
            binding.root.postDelayed({
                if (generation == lockedLandscapeLayoutGeneration && isLockedLandscape()) {
                    applyGbaLayout()
                }
            }, delayMs)
        }
    }

    private fun isLockedLandscape(): Boolean =
        isGbaLocked &&
            activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

    fun resetGBAScreens() {
        if (gbaViews.isEmpty() || isGbaLocked) return
        activity.runOnUiThread {
            val screenH = activity.resources.displayMetrics.heightPixels.toFloat()
            gbaViews.forEachIndexed { i, view ->
                val slot = view.gbaSlot
                val dx = 16f + i * 20f
                val dy = screenH - 320f - 16f - i * 20f
                activity.getSharedPreferences("gba_overlay_${slot}", Context.MODE_PRIVATE).edit()
                    .putFloat("gba_x", dx).putFloat("gba_y", dy)
                    .putFloat("gba_width", 480f).putFloat("gba_height", 320f).apply()
                val p = view.layoutParams as? FrameLayout.LayoutParams ?: return@forEachIndexed
                p.width = 480; p.height = 320; view.layoutParams = p
                view.x = dx; view.y = dy
            }
        }
    }

    fun resetGbaCore() {
        for (slot in 0 until 4) {
            if (IntSetting.getSettingForSIDevice(slot).int == InputOverlay.EMULATED_GBA_CONTROLLER)
                GbaLibrary.resetGbaCore(slot)
        }
    }
}
